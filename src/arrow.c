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
    bp->cx = wide * v2x / v1len + bufp[HANDLE_STEP].cx;
    bp->cy = wide * v2y / v1len + bufp[HANDLE_STEP].cy;
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
    
    cairo_set_line_width(cr, parts->thickness);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    cairo_move_to(cr, handles[HANDLE_POINT].cx, handles[HANDLE_POINT].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_L].cx, handles[HANDLE_EDGE_L].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_R].cx, handles[HANDLE_EDGE_R].cy);
    cairo_close_path(cr);
    cairo_stroke(cr);
    
    cairo_move_to(cr, handles[HANDLE_STEP].cx, handles[HANDLE_STEP].cy);
    cairo_line_to(cr, handles[HANDLE_GRIP].cx, handles[HANDLE_GRIP].cy);
    cairo_stroke(cr);
    
    cairo_set_line_width(cr, 1);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    
    cairo_move_to(cr, handles[HANDLE_POINT].cx, handles[HANDLE_POINT].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_L].cx, handles[HANDLE_EDGE_L].cy);
    cairo_line_to(cr, handles[HANDLE_EDGE_R].cx, handles[HANDLE_EDGE_R].cy);
    cairo_close_path(cr);
    cairo_fill(cr);
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

#if 0

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
#endif

struct parts_t *arrow_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    
    p->type = PARTS_ARROW;
    p->x = x;
    p->y = y;
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    p->triangle_len = 20;
    p->theta = M_PI / 8;
    p->thickness = 10;
    
    return p;
}

