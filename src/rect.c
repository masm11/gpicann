#include <stdio.h>
#include <gtk/gtk.h>

#include "common.h"
#include "shapes.h"

static int beg_x = 0, beg_y = 0;
static int orig_x = 0, orig_y = 0;

void rect_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
    cairo_set_source_rgba(cr, parts->fg.r, parts->fg.g, parts->fg.b, parts->fg.a);
    cairo_rectangle(cr, parts->x, parts->y, parts->width, parts->height);
    cairo_fill(cr);
}

void rect_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
    struct {
	int x;
	int y;
    } poses[] = {
	{ parts->x,                parts->y },
	{ parts->x + parts->width, parts->y },
	{ parts->x,                parts->y + parts->height },
	{ parts->x + parts->width, parts->y + parts->height },
    };
    
    cairo_save(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    for (int i = 0; i < 4; i++) {
	cairo_rectangle(cr, poses[i].x - 5, poses[i].y - 5, 10, 10);
	cairo_stroke(cr);
    }
    cairo_restore(cr);
}

gboolean rect_select(struct parts_t *parts, int x, int y, gboolean selected)
{
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
    
    return x >= x1 && x < x2 && y >= y1 && y < y2;
}

void rect_drag_step(struct parts_t *parts, int x, int y)
{
    parts->x = orig_x + (x - beg_x);
    parts->y = orig_y + (y - beg_y);
    printf("step\n");
}

void rect_drag_fini(struct parts_t *parts, int x, int y)
{
    printf("fini\n");
}

struct parts_t *rect_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    p->type = PARTS_RECT;
    p->x = x;
    p->y = y;
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    
    return p;
}
