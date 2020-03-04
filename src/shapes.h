#ifndef SHAPES_H__INCLUDED
#define SHAPES_H__INCLUDED

void rect_draw(struct parts_t *parts, cairo_t *cr, gboolean selected);
void rect_draw_handle(struct parts_t *parts, cairo_t *cr);
gboolean rect_select(struct parts_t *parts, int x, int y, gboolean selected);
void rect_drag_step(struct parts_t *parts, int x, int y);
void rect_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *rect_create(int x, int y);

void arrow_draw(struct parts_t *parts, cairo_t *cr, gboolean selected);
void arrow_draw_handle(struct parts_t *parts, cairo_t *cr);
gboolean arrow_select(struct parts_t *parts, int x, int y, gboolean selected);
void arrow_drag_step(struct parts_t *p, int x, int y);
void arrow_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *arrow_create(int x, int y);

void text_draw(struct parts_t *parts, cairo_t *cr, gboolean selected);
void text_draw_handle(struct parts_t *parts, cairo_t *cr);
gboolean text_select(struct parts_t *parts, int x, int y, gboolean selected);
void text_drag_step(struct parts_t *p, int x, int y);
void text_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *text_create(int x, int y);
gboolean text_filter_keypress(struct parts_t *part, GdkEventKey *ev);
void text_focus_in(void);
void text_focus_out(void);
void text_init(GtkWidget *top, GtkWidget *w);
void focus_on_click(struct parts_t *parts, int x, int y);

void mask_draw(struct parts_t *parts, cairo_t *cr, gboolean selected);
void mask_draw_handle(struct parts_t *parts, cairo_t *cr);
gboolean mask_select(struct parts_t *parts, int x, int y, gboolean selected);
void mask_drag_step(struct parts_t *p, int x, int y);
void mask_drag_fini(struct parts_t *parts, int x, int y);
struct parts_t *mask_create(int x, int y);

#endif	/* ifndef SHAPES_H__INCLUDED */
