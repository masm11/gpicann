#ifndef COMMON_H__INCLUDED
#define COMMON_H__INCLUDED

#include "config.h"
#include <locale.h>
#include "gettext.h"
#define _(String) gettext(String)

#define likely(e) __builtin_expect(!!(e), 1)
#define unlikely(e) __builtin_expect(!!(e), 0)

enum {
    PARTS_BASE,
    PARTS_ARROW,
    PARTS_TEXT,
    PARTS_RECT,
    PARTS_MASK,

    PARTS_NR
};

struct parts_t {
    struct parts_t *next, *back;
    
    int type;
    int x, y, width, height;
    int arrow_type;
    int thickness;
    int triangle_len;
    double theta;
    char *fontname;
    char *text;
    GdkRGBA fg;
    
    GdkPixbuf *pixbuf;
};

struct history_t {
    struct history_t *next;
    struct parts_t *parts_list, *parts_list_end;
    struct parts_t *selp;
};

extern struct history_t *undoable, *redoable;

struct parts_t *parts_alloc(void);
struct parts_t *parts_dup(struct parts_t *orig);

void history_copy_top_of_undoable(void);
void history_append_parts(struct history_t *hp, struct parts_t *pp);

void call_draw(struct parts_t *p, cairo_t *cr, gboolean selected);;
void call_draw_handle(struct parts_t *p, cairo_t *cr);
gboolean call_select(struct parts_t *p, int x, int y, gboolean selected);
void call_drag_step(struct parts_t *p, int x, int y);
void call_drag_fini(struct parts_t *p, int x, int y);

void prepare_icons(void);

#endif	/* ifndef COMMON_H__INCLUDED */
