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

static unsigned int avg_color(unsigned char *data, int width, int height, int stride, int cx, int cy)
{
    int beg_x = cx - 32 / 2;
    int end_x = cx + 32 / 2;
    int beg_y = cy - 32 / 2;
    int end_y = cy + 32 / 2;
    if (beg_x < 0)
	beg_x = 0;
    if (beg_y < 0)
	beg_y = 0;
    if (end_x >= width)
	end_x = width;
    if (end_y >= height)
	end_y = height;
    
    unsigned long avg_r = 0, avg_g = 0, avg_b = 0;
    unsigned int n = 0;
    for (int y = beg_y; y < end_y; y++) {
	unsigned char *p = data + stride * y + beg_x * 4;
	for (int x = beg_x; x < end_x; x++) {
	    unsigned int rgb = *(uint32_t *) p;
	    avg_r += (rgb >> 16) & 0xff;
	    avg_g += (rgb >>  8) & 0xff;
	    avg_b += (rgb >>  0) & 0xff;
	    n++;
	    p += 4;
	}
    }
    
    return (avg_r / n) << 16 | (avg_g / n) << 8 | (avg_b / n);
}

static void grad_region(unsigned char *data, int width, int height, int stride,
	int rx, int ry, int rw, int rh,
	unsigned int rgb0, unsigned int rgb1, unsigned int rgb2, unsigned int rgb3)
{
    struct {
	unsigned char r, g, b;
    } rgb[4] = {
	{ rgb0 >> 16, rgb0 >> 8, rgb0 >> 0 },
	{ rgb1 >> 16, rgb1 >> 8, rgb1 >> 0 },
	{ rgb2 >> 16, rgb2 >> 8, rgb2 >> 0 },
	{ rgb3 >> 16, rgb3 >> 8, rgb3 >> 0 },
    }, top[rw], bot[rw];
    
    for (int dx = 0; dx < rw; dx++) {
	top[dx].r = (rgb[0].r * (rw - dx) + rgb[1].r * dx) / rw;
	top[dx].g = (rgb[0].g * (rw - dx) + rgb[1].g * dx) / rw;
	top[dx].b = (rgb[0].b * (rw - dx) + rgb[1].b * dx) / rw;
    }
    
    for (int dx = 0; dx < rw; dx++) {
	bot[dx].r = (rgb[2].r * (rw - dx) + rgb[3].r * dx) / rw;
	bot[dx].g = (rgb[2].g * (rw - dx) + rgb[3].g * dx) / rw;
	bot[dx].b = (rgb[2].b * (rw - dx) + rgb[3].b * dx) / rw;
    }
    
    for (int dy = 0; dy < rh; dy++) {
	for (int dx = 0; dx < rw; dx++) {
	    unsigned char mid_r, mid_g, mid_b;
	    mid_r = (top[dx].r * (rh - dy) + bot[dx].r * dy) / rh;
	    mid_g = (top[dx].g * (rh - dy) + bot[dx].g * dy) / rh;
	    mid_b = (top[dx].b * (rh - dy) + bot[dx].b * dy) / rh;
	    
	    uint32_t mid_rgb = mid_r << 16 | mid_g << 8 | mid_b;
	    *(uint32_t *) (data + stride * (ry + dy) + (rx + dx) * 4) = mid_rgb;
	}
    }
}

void mask_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    int x = parts->x;
    int y = parts->y;
    int width = parts->width;
    int height = parts->height;
    
    if (width == 0 || height == 0)
	return;

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
    
    int nr_x, nr_y;
    nr_x = width / 32;
    nr_y = height / 32;
    if (nr_x < 2)
	nr_x = 2;
    if (nr_y < 2)
	nr_y = 2;
    int xs[nr_x + 1], ys[nr_y + 1];
    for (int i = 0; i < nr_x; i++)
	xs[i] = width * i / nr_x;
    for (int i = 0; i < nr_y; i++)
	ys[i] = height * i / nr_y;
    xs[nr_x] = width;
    ys[nr_y] = height;
    
    unsigned int rgb[nr_y + 1][nr_x + 1];
    for (int j = 0; j <= nr_y; j++) {
	for (int i = 0; i <= nr_x; i++)
	    rgb[j][i] = avg_color(data, width, height, stride, xs[i], ys[j]);
    }
    
    for (int j = 0; j < nr_y; j++) {
	for (int i = 0; i < nr_x; i++) {
	    grad_region(data, width, height, stride,
		    xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j],
		    rgb[j][i], rgb[j][i + 1], rgb[j + 1][i], rgb[j + 1][i + 1]);
	}
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
