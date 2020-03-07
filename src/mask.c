#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"
#include "handle.h"

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

static inline unsigned int get_pixel(unsigned char *data, int x, int y, int width, int height, int stride)
{
    if (unlikely(x >= width))
	x = width - 1;
    if (unlikely(x < 0))
	x = 0;
    if (unlikely(y >= height))
	y = height - 1;
    if (unlikely(y < 0))
	y = 0;
    return *(unsigned int *) (data + y * stride + x * 4);
}

static inline void put_pixel(unsigned char *data, int x, int y, int width, int height, int stride, unsigned int value)
{
    if (unlikely(x >= width))
	x = width - 1;
    if (unlikely(x < 0))
	x = 0;
    if (unlikely(y >= height))
	y = height - 1;
    if (unlikely(y < 0))
	y = 0;
    *(unsigned int *) (data + y * stride + x * 4) = value;
}

void mask_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    int x = parts->x;
    int y = parts->y;
    int width = parts->width;
    int height = parts->height;

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
    
    for (int i = 0; i < 20; i++) {
	for (int y = 0; y < height; y++) {
	    for (int x = 0; x < width; x++) {
		unsigned int r = 0, g = 0, b = 0;
		for (int dy = -1; dy < 2; dy++) {
		    for (int dx = -1; dx < 2; dx++) {
			unsigned int argb = get_pixel(data, x + dx, y + dy, width, height, stride);
			r += (argb >> 16) & 0xff;
			g += (argb >>  8) & 0xff;
			b += (argb >>  0) & 0xff;
		    }
		}
		r /= 9;
		g /= 9;
		b /= 9;
		put_pixel(data2, x, y, width, height, stride, r << 16 | g << 8 | b);
	    }
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
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    
    return p;
}
