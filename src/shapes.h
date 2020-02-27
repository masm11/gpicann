#ifndef SHAPES_H__INCLUDED
#define SHAPES_H__INCLUDED

void rect_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr);
void rect_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr);
gboolean rect_select(struct parts_t *parts, int x, int y, gboolean selected);
void rect_drag_step(struct parts_t *parts, int x, int y);
void rect_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *rect_create(int x, int y);

void arrow_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr);
void arrow_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr);
gboolean arrow_select(struct parts_t *parts, int x, int y, gboolean selected);
void arrow_drag_step(struct parts_t *p, int x, int y);
void arrow_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *arrow_create(int x, int y);

#endif	/* ifndef SHAPES_H__INCLUDED */
