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
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"
#include "handle.h"
#include "settings.h"

enum {
    HANDLE_TOP_LEFT,
    HANDLE_TOP,
    HANDLE_TOP_RIGHT,
    HANDLE_LEFT,
    HANDLE_RIGHT,
    HANDLE_BOTTOM_LEFT,
    HANDLE_BOTTOM,
    HANDLE_BOTTOM_RIGHT,

    HANDLE_NR
};

static int beg_x = 0, beg_y = 0;
static int orig_x = 0, orig_y = 0, orig_w = 0, orig_h = 0;
static int dragging_handle = -1;

static void make_handle_geoms(struct parts_t *p, struct handle_t *bufp)
{
    struct handle_t *bp = bufp;
    
    bp->cx = p->x;
    bp->cy = p->y;
    bp++;
    
    bp->cx = p->x + p->width / 2;
    bp->cy = p->y;
    bp++;
    
    bp->cx = p->x + p->width;
    bp->cy = p->y;
    bp++;
    
    bp->cx = p->x;
    bp->cy = p->y + p->height / 2;
    bp++;
    
    bp->cx = p->x + p->width;
    bp->cy = p->y + p->height / 2;
    bp++;
    
    bp->cx = p->x;
    bp->cy = p->y + p->height;
    bp++;
    
    bp->cx = p->x + p->width / 2;
    bp->cy = p->y + p->height;
    bp++;
    
    bp->cx = p->x + p->width;
    bp->cy = p->y + p->height;
    bp++;
    
    handle_calc_geom(bufp, HANDLE_NR);
}
    
static gboolean div_9_inited = FALSE;
static unsigned char div_9[9 * 256];
static void div_9_init(void)
{
    if (!div_9_inited) {
	div_9_inited = TRUE;
	for (int i = 0; i < 9 * 256; i++)
	    div_9[i] = i / 9;
    }
}

static void blur_pixel(unsigned char *datap, int x, int y, int width, int height, int stride, unsigned char *data_dst)
{
    int dx_l, dx_r, dy_u, dy_d;
    
    if (unlikely(x == 0))
	dx_l = 0;
    else
	dx_l = -4;
    if (likely(x + 1 < width))
	dx_r = 4;
    else
	dx_r = 0;
    if (unlikely(y == 0))
	dy_u = 0;
    else
	dy_u = -stride;
    if (likely(y + 1 < height))
	dy_d = stride;
    else
	dy_d = 0;
    
    unsigned int acc_r = 0, acc_g = 0, acc_b = 0;
    
#define SUM_UP(ptr) (			\
    argb = *(uint32_t *) (ptr),		\
    acc_r += (argb >> 16) & 0xff,	\
    acc_g += (argb >>  8) & 0xff,	\
    acc_b += (argb >>  0) & 0xff)
    
    uint32_t argb;
    SUM_UP(datap + dy_u + dx_l);
    SUM_UP(datap + dy_u);
    SUM_UP(datap + dy_u + dx_r);
    SUM_UP(datap + dx_l);
    SUM_UP(datap);
    SUM_UP(datap + dx_r);
    SUM_UP(datap + dy_d + dx_l);
    SUM_UP(datap + dy_d);
    SUM_UP(datap + dy_d + dx_r);
    
#undef SUM_UP

    acc_r = div_9[acc_r];
    acc_g = div_9[acc_g];
    acc_b = div_9[acc_b];
    *(uint32_t *) data_dst = acc_r << 16 | acc_g << 8 | acc_b;
}

void mask_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    int x = parts->x;
    int y = parts->y;
    int width = parts->width;
    int height = parts->height;
    
    if (!div_9_inited)
	div_9_init();
    
    if (width < 0) {
	x += width;
	width = -width;
    }
    if (height < 0) {
	y += height;
	height = -height;
    }

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
    unsigned char *data = g_malloc(stride * height);
    unsigned char *data2 = g_malloc(stride * height);
    
    cairo_pattern_t *pat = cairo_pattern_create_for_surface(cairo_get_target(cr));
    cairo_matrix_t mat;
    cairo_get_matrix(cr, &mat);
    cairo_matrix_translate(&mat, x, y);
    cairo_pattern_set_matrix(pat, &mat);
    
    cairo_surface_t *cs1 = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, width, height, stride);
    cairo_t *cr1 = cairo_create(cs1);
    cairo_set_source(cr1, pat);
    cairo_paint(cr1);
    cairo_destroy(cr1);
    cairo_surface_destroy(cs1);
    
    cairo_pattern_destroy(pat);
    
    for (int i = 0; i < 24; i++) {
	for (int y = 0; y < height; y++) {
	    unsigned char *sp = data + y * stride;
	    unsigned char *dp = data2 + y * stride;
	    for (int x = 0; x < width; x++, sp += 4, dp += 4)
		blur_pixel(sp, x, y, width, height, stride, dp);
	}
	
	unsigned char *t = data;
	data = data2;
	data2 = t;
    }

    cairo_surface_t *cs2 = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, width, height, stride);
    cairo_pattern_t *pat2 = cairo_pattern_create_for_surface(cs2);
    cairo_matrix_t mat2;
    cairo_get_matrix(cr, &mat2);
    cairo_matrix_translate(&mat2, -(x + mat2.x0), -(y + mat2.y0));
    cairo_pattern_set_matrix(pat2, &mat2);
    
    cairo_save(cr);
    cairo_set_source(cr, pat2);
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);
    cairo_restore(cr);
    
    cairo_pattern_destroy(pat2);
    cairo_surface_destroy(cs2);
    
    g_free(data);
    g_free(data2);
}

void mask_draw_handle(struct parts_t *parts, cairo_t *cr)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    handle_draw(handles, HANDLE_NR, cr);
}

gboolean mask_select(struct parts_t *parts, int x, int y, gboolean selected)
{
    struct handle_t handles[HANDLE_NR];
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
		dragging_handle = i;
		return TRUE;
	    }
	}
    }
    
    int x1, x2, y1, y2, t;
    
    x1 = parts->x;
    x2 = x1 + parts->width;
    y1 = parts->y;
    y2 = y1 + parts->height;
    
    if (x2 < x1) {
	t = x1;
	x1 = x2;
	x2 = t;
    }
    if (y2 < y1) {
	t = y1;
	y1 = y2;
	y2 = t;
    }
    
    beg_x = x;
    beg_y = y;
    orig_x = parts->x;
    orig_y = parts->y;
    dragging_handle = -1;
    
    return x >= x1 && x < x2 && y >= y1 && y < y2;
}

void mask_drag_step(struct parts_t *p, int x, int y)
{
    int dx = x - beg_x;
    int dy = y - beg_y;
    
    switch (dragging_handle) {
    case HANDLE_TOP_LEFT:
    case HANDLE_LEFT:
    case HANDLE_BOTTOM_LEFT:
	p->x = orig_x + dx;
	p->width = orig_w - dx;
    }
    
    switch (dragging_handle) {
    case HANDLE_TOP_RIGHT:
    case HANDLE_RIGHT:
    case HANDLE_BOTTOM_RIGHT:
	p->width = orig_w + dx;
    }
    
    switch (dragging_handle) {
    case HANDLE_TOP_LEFT:
    case HANDLE_TOP:
    case HANDLE_TOP_RIGHT:
	p->y = orig_y + dy;
	p->height = orig_h - dy;
    }
    
    switch (dragging_handle) {
    case HANDLE_BOTTOM_LEFT:
    case HANDLE_BOTTOM:
    case HANDLE_BOTTOM_RIGHT:
	p->height = orig_h + dy;
    }
    
    if (dragging_handle == -1) {
	p->x = orig_x + dx;
	p->y = orig_y + dy;
    }
}

void mask_drag_fini(struct parts_t *parts, int x, int y)
{
}

struct parts_t *mask_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    
    p->type = PARTS_MASK;
    p->x = x;
    p->y = y;
    
    return p;
}
