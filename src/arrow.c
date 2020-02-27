#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"

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

struct handle_geom_t {
    double cx, cy;
    double x, y, width, height;
};

static void make_handle_geoms(struct parts_t *p, struct handle_geom_t *bufp)
{
    struct handle_geom_t *bp = bufp;
    
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
    bp->cx = wide * v1x / v1len + bufp[HANDLE_STEP].cx;
    bp->cy = wide * v1y / v1len + bufp[HANDLE_STEP].cy;
    bp++;
    
    /* edge_r */
    int v2x = v0y;
    int v2y = -v0x;
    double v2len = sqrt(v2x * v2x + v2y * v2y); // == total_len
    bp->cx = wide * v2x / v2len + bufp[HANDLE_STEP].cx;
    bp->cy = wide * v2y / v2len + bufp[HANDLE_STEP].cy;
    bp++;
    
    for (int i = 0; i < HANDLE_NR; i++) {
	bp = bufp + i;
	bp->x = bp->cx - 4;
	bp->y = bp->cy - 4;
	bp->width = 8;
	bp->height = 8;
    }
}

void arrow_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
    struct handle_geom_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    
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

void arrow_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
    struct handle_geom_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
    cairo_save(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    for (int i = 0; i < 8; i++) {
	cairo_rectangle(cr, handles[i].x, handles[i].y, handles[i].width, handles[i].height);
	cairo_stroke(cr);
    }
    cairo_restore(cr);
}

gboolean arrow_select(struct parts_t *parts, int x, int y, gboolean selected)
{
    struct handle_geom_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
    if (selected) {
	for (int i = 0; i < HANDLE_NR; i++) {
	    if (x >= handles[i].x && x < handles[i].x + handles[i].width
		    && y >= handles[i].y && y < handles[i].y + handles[i].height) {
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
		dragging_handle = i;
		return TRUE;
	    }
	}
    }
    
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
    
    beg_x = x;
    beg_y = y;
    orig_x = parts->x;
    orig_y = parts->y;
    dragging_handle = -1;
    
    return distance <= parts->thickness && inner_prod_ac >= 0 && inner_prod_bc >= 0;
}

void arrow_drag_step(struct parts_t *p, int x, int y)
{
    struct handle_geom_t handles[HANDLE_NR];
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
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    p->triangle_len = 50;
    p->theta = M_PI / 8;
    p->thickness = 10;
    
    return p;
}

