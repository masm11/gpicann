#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"

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

struct handle_geom_t {
    int x, y, width, height;
};

static void make_handle_geoms(struct parts_t *p, struct handle_geom_t *bufp)
{
    struct handle_geom_t *bp = bufp;
    
    bp->x = p->x;
    bp->y = p->y;
    bp++;
    
    bp->x = p->x + p->width / 2;
    bp->y = p->y;
    bp++;
    
    bp->x = p->x + p->width;
    bp->y = p->y;
    bp++;
    
    bp->x = p->x;
    bp->y = p->y + p->height / 2;
    bp++;
    
    bp->x = p->x + p->width;
    bp->y = p->y + p->height / 2;
    bp++;
    
    bp->x = p->x;
    bp->y = p->y + p->height;
    bp++;
    
    bp->x = p->x + p->width / 2;
    bp->y = p->y + p->height;
    bp++;
    
    bp->x = p->x + p->width;
    bp->y = p->y + p->height;
    bp++;
    
    for (int i = 0; i < HANDLE_NR; i++) {
	bp = bufp + i;
	bp->x -= 4;
	bp->y -= 4;
	bp->width = 8;
	bp->height = 8;
    }
}

void rect_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
#define DIFF 4.0
#define NR 16

    for (int i = 0; i < NR; i++) {
	int dx = DIFF * cos(2 * M_PI / NR * i) + DIFF / 2;
	int dy = DIFF * sin(2 * M_PI / NR * i) + DIFF / 2;
	cairo_save(cr);
	cairo_set_line_width(cr, parts->thickness);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.05);
	cairo_rectangle(cr, parts->x + dx, parts->y + dy, parts->width, parts->height);
	cairo_stroke(cr);
	cairo_restore(cr);
    }

#undef NR
#undef DIFF

    cairo_set_line_width(cr, parts->thickness);
    cairo_set_source_rgba(cr, parts->fg.r, parts->fg.g, parts->fg.b, parts->fg.a);
    cairo_rectangle(cr, parts->x, parts->y, parts->width, parts->height);
    cairo_stroke(cr);
}

void rect_draw_handle(struct parts_t *parts, cairo_t *cr)
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

gboolean rect_select(struct parts_t *parts, int x, int y, gboolean selected)
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
    
    if (x >= x1 && x < x2 && y >= y1 - parts->thickness / 2 && y < y1 + parts->thickness)
	return TRUE;
    if (x >= x1 && x < x2 && y >= y2 - parts->thickness / 2 && y < y2 + parts->thickness)
	return TRUE;
    if (y >= y1 && y < y2 && x >= x1 - parts->thickness / 2 && x < x1 + parts->thickness)
	return TRUE;
    if (y >= y1 && y < y2 && x >= x2 - parts->thickness / 2 && x < x2 + parts->thickness)
	return TRUE;
    return FALSE;
}

void rect_drag_step(struct parts_t *p, int x, int y)
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

void rect_drag_fini(struct parts_t *parts, int x, int y)
{
}

struct parts_t *rect_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    
    p->type = PARTS_RECT;
    p->x = x;
    p->y = y;
    p->fg.r = 1;
    p->fg.g = 0;
    p->fg.b = 0;
    p->fg.a = 1;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    p->thickness = 5;
    
    return p;
}
