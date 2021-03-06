/*    gpicann - Screenshot Annotation Tool
 *    Copyright (C) 2020 Yuuki Harano
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"
#include "handle.h"
#include "settings.h"
#include "tcos.h"

enum {
    /*            EDGE_R
     *            **
     *          ****
     * POINT  *************** GRIP
     *          **** STEP
     *            **
     *            EDGE_L
     */

    HANDLE_POINT,
    HANDLE_GRIP,
    HANDLE_STEP,
    HANDLE_EDGE_L,
    HANDLE_EDGE_R,

    HANDLE_NR
};

static int beg_x = 0, beg_y = 0;
static int orig_x = 0, orig_y = 0, orig_w = 0, orig_h = 0;
static int orig_edge_l_x = 0, orig_edge_l_y = 0, orig_edge_r_x = 0, orig_edge_r_y = 0;
static int orig_step_x = 0, orig_step_y = 0;
static int dragging_handle = -1;

static void make_handle_geoms(struct parts_t *p, struct handle_t *bufp)
{
    struct handle_t *bp = bufp;
    
    /* point */
    bp->cx = p->x + p->width;
    bp->cy = p->y + p->height;
    bp++;
    
    /* grip */
    bp->cx = p->x;
    bp->cy = p->y;
    bp++;
    
    /* step */
    double total_len = sqrt(
	    (bufp[HANDLE_POINT].cx - bufp[HANDLE_GRIP].cx) * (bufp[HANDLE_POINT].cx - bufp[HANDLE_GRIP].cx) +
	    (bufp[HANDLE_POINT].cy - bufp[HANDLE_GRIP].cy) * (bufp[HANDLE_POINT].cy - bufp[HANDLE_GRIP].cy));
    if (total_len < 1)
	total_len = 1;
    double triangle_len = p->triangle_len;
    bp->cx = (bufp[HANDLE_GRIP].cx - bufp[HANDLE_POINT].cx) * triangle_len / total_len + bufp[HANDLE_POINT].cx;
    bp->cy = (bufp[HANDLE_GRIP].cy - bufp[HANDLE_POINT].cy) * triangle_len / total_len + bufp[HANDLE_POINT].cy;
    bp++;
    
    /* edge_l */
    double theta = p->theta;
    double wide = triangle_len * tan(theta);
    double v0x = bufp[HANDLE_GRIP].cx - bufp[HANDLE_POINT].cx;
    double v0y = bufp[HANDLE_GRIP].cy - bufp[HANDLE_POINT].cy;
    double v1x = -v0y;
    double v1y = v0x;
    double v1len = sqrt(v1x * v1x + v1y * v1y); // == total_len
    if (v1len < 1)
	v1len = 0;
    bp->cx = wide * v1x / v1len + bufp[HANDLE_STEP].cx;
    bp->cy = wide * v1y / v1len + bufp[HANDLE_STEP].cy;
    bp++;
    
    /* edge_r */
    int v2x = v0y;
    int v2y = -v0x;
    double v2len = sqrt(v2x * v2x + v2y * v2y); // == total_len
    if (v2len < 1)
	v2len = 0;
    bp->cx = wide * v2x / v2len + bufp[HANDLE_STEP].cx;
    bp->cy = wide * v2y / v2len + bufp[HANDLE_STEP].cy;
    bp++;
    
    handle_calc_geom(bufp, HANDLE_NR);
}

void arrow_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
#define DIFF 4.0

    for (int i = 0; i < TCOS_NR; i++) {
	int dx = DIFF * tcos(i) + DIFF / 2;
	int dy = DIFF * tsin(i) + DIFF / 2;
	cairo_save(cr);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.05);

	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, handles[HANDLE_POINT].cx + dx, handles[HANDLE_POINT].cy + dy);
	cairo_line_to(cr, handles[HANDLE_EDGE_L].cx + dx, handles[HANDLE_EDGE_L].cy + dy);
	cairo_line_to(cr, handles[HANDLE_EDGE_R].cx + dx, handles[HANDLE_EDGE_R].cy + dy);
	cairo_close_path(cr);
	cairo_fill(cr);
    
	cairo_set_line_width(cr, parts->thickness);
	cairo_move_to(cr, handles[HANDLE_STEP].cx + dx, handles[HANDLE_STEP].cy + dy);
	cairo_line_to(cr, handles[HANDLE_GRIP].cx + dx, handles[HANDLE_GRIP].cy + dy);
	cairo_stroke(cr);

	cairo_restore(cr);
    }

#undef DIFF



    cairo_set_source_rgba(cr, parts->fg.red, parts->fg.green, parts->fg.blue, 1);
    
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, handles[HANDLE_POINT].cx, handles[HANDLE_POINT].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_L].cx, handles[HANDLE_EDGE_L].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_R].cx, handles[HANDLE_EDGE_R].cy);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    cairo_set_line_width(cr, parts->thickness);
    cairo_move_to(cr, handles[HANDLE_STEP].cx, handles[HANDLE_STEP].cy);
    cairo_line_to(cr, handles[HANDLE_GRIP].cx, handles[HANDLE_GRIP].cy);
    cairo_stroke(cr);
}

void arrow_draw_handle(struct parts_t *parts, cairo_t *cr)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    handle_draw(handles, HANDLE_NR, cr);
}

static int on_handle(struct handle_t *handles, int x, int y)
{
    for (int i = 0; i < HANDLE_NR; i++) {
	if (x >= handles[i].x && x < handles[i].x + handles[i].width
		&& y >= handles[i].y && y < handles[i].y + handles[i].height) {
	    return i;
	}
    }
    
    return -1;
}

static gboolean on_line(struct parts_t *parts, struct handle_t *handles, int x, int y)
{
    double ax = x - handles[HANDLE_GRIP].cx;
    double ay = y - handles[HANDLE_GRIP].cy;
    double bx = x - handles[HANDLE_POINT].cx;
    double by = y - handles[HANDLE_POINT].cy;
    double cx = handles[HANDLE_POINT].cx - handles[HANDLE_GRIP].cx;
    double cy = handles[HANDLE_POINT].cy - handles[HANDLE_GRIP].cy;
    
    double a2 = ax * ax + ay * ay;
    double b2 = bx * bx + by * by;
    double c2 = cx * cx + cy * cy;
    double distance = sqrt(((c2 - a2 - b2) * (c2 - a2 - b2) - 4 * a2 * b2) / (-4 * c2));
    
    double inner_prod_ac = ax * cx + ay * cy;
    double inner_prod_bc = bx * -cx + by * -cy;
    
    return distance <= parts->thickness && inner_prod_ac >= 0 && inner_prod_bc >= 0;
}

static gboolean on_triangle(struct handle_t *handles, int x, int y)
{
    double a0x = handles[HANDLE_EDGE_R].x - handles[HANDLE_EDGE_L].x;
    double a0y = handles[HANDLE_EDGE_R].y - handles[HANDLE_EDGE_L].y;
    double b0x = handles[HANDLE_POINT].x - handles[HANDLE_EDGE_R].x;
    double b0y = handles[HANDLE_POINT].y - handles[HANDLE_EDGE_R].y;
    double c0x = handles[HANDLE_EDGE_L].x - handles[HANDLE_POINT].x;
    double c0y = handles[HANDLE_EDGE_L].y - handles[HANDLE_POINT].y;
    
    double a1x = x - handles[HANDLE_EDGE_L].x;
    double a1y = y - handles[HANDLE_EDGE_L].y;
    double b1x = x - handles[HANDLE_EDGE_R].x;
    double b1y = y - handles[HANDLE_EDGE_R].y;
    double c1x = x - handles[HANDLE_POINT].x;
    double c1y = y - handles[HANDLE_POINT].y;
    
    double outer_prod_a = a0x * a1y - a0y * a1x;
    double outer_prod_b = b0x * b1y - b0y * b1x;
    double outer_prod_c = c0x * c1y - c0y * c1x;

    if (outer_prod_a > 0 && outer_prod_b > 0 && outer_prod_c > 0)
	return TRUE;
    if (outer_prod_a < 0 && outer_prod_b < 0 && outer_prod_c < 0)
	return TRUE;
    return FALSE;
}

gboolean arrow_select(struct parts_t *parts, int x, int y, gboolean selected)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
    beg_x = x;
    beg_y = y;
    orig_x = parts->x;
    orig_y = parts->y;
    orig_w = parts->width;
    orig_h = parts->height;
    orig_edge_l_x = handles[HANDLE_EDGE_L].x;
    orig_edge_l_y = handles[HANDLE_EDGE_L].y;
    orig_edge_r_x = handles[HANDLE_EDGE_R].x;
    orig_edge_r_y = handles[HANDLE_EDGE_R].y;
    orig_step_x = handles[HANDLE_STEP].x;
    orig_step_y = handles[HANDLE_STEP].y;
    
    if (selected) {
	dragging_handle = on_handle(handles, x, y);
	if (dragging_handle >= 0)
	    return TRUE;
    }
    
    dragging_handle = -1;
    return on_line(parts, handles, x, y) || on_triangle(handles, x, y);
}

void arrow_drag_step(struct parts_t *p, int x, int y)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(p, handles);
    
    int dx = x - beg_x;
    int dy = y - beg_y;
    
    double new_x, new_y;
    switch (dragging_handle) {
    case -1:
	p->x = orig_x + dx;
	p->y = orig_y + dy;
	break;
	
    case HANDLE_GRIP:
	p->x = orig_x + dx;
	p->y = orig_y + dy;
	p->width = orig_w - dx;
	p->height = orig_h - dy;
	break;
	
    case HANDLE_POINT:
	p->width = orig_w + dx;
	p->height = orig_h + dy;
	break;

    case HANDLE_STEP:
    case HANDLE_EDGE_L:
    case HANDLE_EDGE_R:
	if (TRUE) {
	    switch (dragging_handle) {
	    case HANDLE_EDGE_L:
		new_x = orig_edge_l_x + dx;
		new_y = orig_edge_l_y + dy;
		break;
	    case HANDLE_EDGE_R:
		new_x = orig_edge_r_x + dx;
		new_y = orig_edge_r_y + dy;
		break;
	    case HANDLE_STEP:
		new_x = orig_step_x + dx;
		new_y = orig_step_y + dy;
		break;
	    }
	    double cx = handles[HANDLE_GRIP].x - handles[HANDLE_POINT].x;
	    double cy = handles[HANDLE_GRIP].y - handles[HANDLE_POINT].y;
	    double ax = new_x - handles[HANDLE_POINT].x;
	    double ay = new_y - handles[HANDLE_POINT].y;
	    double clen = sqrt(cx * cx + cy * cy);
	    double alen = sqrt(ax * ax + ay * ay);
	    double theta = acos((ax * cx + ay * cy) / clen / alen);
	    switch (dragging_handle) {
	    case HANDLE_EDGE_L:
	    case HANDLE_EDGE_R:
		p->theta = theta;
		p->triangle_len = alen * cos(theta);
		break;
	    case HANDLE_STEP:
		p->triangle_len = alen * cos(theta);
		break;
	    }
	}
	break;
    }
}

void arrow_drag_fini(struct parts_t *parts, int x, int y)
{
}

struct parts_t *arrow_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    
    p->type = PARTS_ARROW;
    p->x = x;
    p->y = y;
    p->triangle_len = 50;
    p->theta = M_PI / 8;
    
    return p;
}

