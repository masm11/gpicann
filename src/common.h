#ifndef COMMON_H__INCLUDED
#define COMMON_H__INCLUDED

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
    char *text;
    struct color_t {
	double r, g, b, a;
    } fg, bg;
    
    GdkPixbuf *pixbuf;
};

struct history_t {
    struct history_t *next;
    struct parts_t *parts_list, *parts_list_end;
    struct parts_t *selp;
};


struct parts_t *parts_alloc(void);
struct parts_t *parts_dup(struct parts_t *orig);

#endif	/* ifndef COMMON_H__INCLUDED */
