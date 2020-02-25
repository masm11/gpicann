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
    HANDLE_EDGE_L,
    HANDLE_EDGE_R,
    HANDLE_STEP,
    HANDLE_GRIP,

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

void arrow_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    
    struct {
	double x, y;
    } coords[5];
    
    /* point */
    coords[0].x = parts->x + parts->width;
    coords[0].y = parts->y + parts->height;
    
    /* grip */
    coords[1].x = parts->x;
    coords[1].y = parts->y;
    
    /* step */
    double total_len = sqrt(
	    (coords[0].x - coords[1].x) * (coords[0].x - coords[1].x) +
	    (coords[0].y - coords[1].y) * (coords[0].y - coords[1].y));
    double triangle_len = parts->triangle_len;
    coords[2].x = (coords[1].x - coords[0].x) * triangle_len / total_len + coords[0].x;
    coords[2].y = (coords[1].y - coords[0].y) * triangle_len / total_len + coords[0].y;
    
    /* edge_l */
    double theta = parts->theta;
    double wide = triangle_len * tan(theta);
    int v0x = coords[1].x - coords[0].x;
    int v0y = coords[1].y - coords[0].y;
    int v1x = -v0y;
    int v1y = v0x;
    double v1len = sqrt(v1x * v1x + v1y * v1y); // == total_len
    coords[3].x = wide * v1x / v1len + coords[2].x;
    coords[3].y = wide * v1y / v1len + coords[2].y;
    
    /* edge_r */
    int v2x = v0y;
    int v2y = -v0x;
    double v2len = sqrt(v2x * v2x + v2y * v2y); // == total_len
    coords[4].x = wide * v2x / v1len + coords[2].x;
    coords[4].y = wide * v2y / v1len + coords[2].y;
    
    cairo_set_line_width(cr, parts->thickness);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    cairo_move_to(cr, coords[0].x, coords[0].y);
    cairo_line_to(cr, coords[3].x, coords[3].y);
    cairo_line_to(cr, coords[4].x, coords[4].y);
    cairo_close_path(cr);
    cairo_stroke(cr);
    
    cairo_move_to(cr, coords[2].x, coords[2].y);
    cairo_line_to(cr, coords[1].x, coords[1].y);
    cairo_stroke(cr);
    
    cairo_set_line_width(cr, 1);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    
    cairo_move_to(cr, coords[0].x, coords[0].y);
    cairo_line_to(cr, coords[3].x, coords[3].y);
    cairo_line_to(cr, coords[4].x, coords[4].y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

#if 0
void rect_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
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
    
    return x >= x1 && x < x2 && y >= y1 && y < y2;
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

