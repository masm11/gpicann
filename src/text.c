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

static int cursor_pos = 0;   // in bytes.

static GtkWidget *toplevel, *drawable;

struct parts_t *focused_parts;

static GtkIMContext *im_context;

struct {
    gchar *str;
    PangoAttrList *attrs;
    gint pos;
} preedit;

static char *insert_string(const char *orig, int pos, const char *str);

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

static int cursor_next_pos_in_bytes(struct parts_t *parts, int pos)
{
    glong pos_in_chars_till_pos, pos_in_chars_total;
    gunichar *uni_str_till_pos = g_utf8_to_ucs4(parts->text, pos, NULL, &pos_in_chars_till_pos, NULL);
    gunichar *uni_str_total = g_utf8_to_ucs4(parts->text, strlen(parts->text), NULL, &pos_in_chars_total, NULL);

    if (pos_in_chars_till_pos + 1 <= pos_in_chars_total)
	pos_in_chars_till_pos++;

    glong new_pos = 0;
    gchar *str_till_newpos = g_ucs4_to_utf8(uni_str_total, pos_in_chars_till_pos, NULL, &new_pos, NULL);

    g_free(uni_str_till_pos);
    g_free(uni_str_total);
    g_free(str_till_newpos);

    return new_pos;
}

static int cursor_prev_pos_in_bytes(struct parts_t *parts, int pos)
{
    glong pos_in_chars_till_pos, pos_in_chars_total;
    gunichar *uni_str_till_pos = g_utf8_to_ucs4(parts->text, pos, NULL, &pos_in_chars_till_pos, NULL);
    gunichar *uni_str_total = g_utf8_to_ucs4(parts->text, strlen(parts->text), NULL, &pos_in_chars_total, NULL);

    if (pos_in_chars_till_pos - 1 >= 0)
	pos_in_chars_till_pos--;

    glong new_pos = 0;
    gchar *str_till_newpos = g_ucs4_to_utf8(uni_str_total, pos_in_chars_till_pos, NULL, &new_pos, NULL);

    g_free(uni_str_till_pos);
    g_free(uni_str_total);
    g_free(str_till_newpos);

    return new_pos;
}

void text_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
{
#if 0
#define DIFF 4.0
#define NR 16

    for (int i = 0; i < NR; i++) {
	int dx = DIFF * cos(2 * M_PI / NR * i) + DIFF / 2;
	int dy = DIFF * sin(2 * M_PI / NR * i) + DIFF / 2;
	cairo_save(cr);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.05);
	cairo_rectangle(cr, parts->x + dx, parts->y + dy, parts->width, parts->height);
	cairo_fill(cr);
	cairo_restore(cr);
    }

#undef NR
#undef DIFF
#endif
    
    PangoLayout *layout = gtk_widget_create_pango_layout(drawable, parts->text);
    pango_layout_set_width(layout, parts->width * PANGO_SCALE);
    
    
    PangoFontDescription *font_desc = pango_font_description_new();
    pango_font_description_set_family(font_desc, "Noto Sans Mono CJK JP");
    pango_font_description_set_size(font_desc, 32768);
    pango_layout_set_font_description(layout, font_desc);
    
    PangoAttrList *attr_list = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_foreground_new(0xffff * parts->fg.r, 0xffff * parts->fg.g, 0xffff * parts->fg.b);
    attr->start_index = 0;
    attr->end_index = strlen(parts->text);
    pango_attr_list_change(attr_list, attr);
    
    pango_layout_set_attributes(layout, attr_list);
    
    PangoAttribute *curs_bg_pre = pango_attr_foreground_new(0xffff * parts->fg.r, 0xffff * parts->fg.g, 0xffff * parts->fg.b);
    PangoAttribute *curs_bg = pango_attr_foreground_new(0xffff * parts->fg.r, 0xffff * parts->fg.g, 0xffff * parts->fg.b);
    PangoAttribute *curs_fg = pango_attr_background_new(0, 0, 0);
    int cursor_next = cursor_next_pos_in_bytes(parts, cursor_pos);
    curs_bg_pre->start_index = cursor_pos;
    curs_bg_pre->end_index = cursor_pos;
    curs_bg->start_index = cursor_pos;
    curs_bg->end_index = cursor_next;
    curs_fg->start_index = cursor_pos;
    curs_fg->end_index = cursor_next;
    pango_attr_list_change(attr_list, curs_bg);
    pango_attr_list_change(attr_list, curs_bg_pre);
    pango_attr_list_change(attr_list, curs_fg);
    
    if (parts == focused_parts && preedit.attrs != NULL) {
	if (strlen(preedit.str) != 0) {
	    pango_attr_list_splice(attr_list, preedit.attrs, cursor_pos, strlen(preedit.str));
	    gchar *str = insert_string(parts->text, cursor_pos, preedit.str);
	    pango_layout_set_text(layout, str, strlen(str));
	}
    }
    
    pango_layout_set_attributes(layout, attr_list);
    
    cairo_move_to(cr, parts->x, parts->y);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    pango_cairo_show_layout(cr, layout);
}

void text_draw_handle(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr)
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

gboolean text_select(struct parts_t *parts, int x, int y, gboolean selected)
{
    struct handle_geom_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    
    if (selected) {
	for (int i = 0; i < HANDLE_NR; i++) {
	    if (x >= handles[i].x && x < handles[i].x + handles[i].width
		    && y >= handles[i].y && y < handles[i].y + handles[i].height) {
		focused_parts = parts;
		beg_x = x;
		beg_y = y;
		orig_x = parts->x;
		orig_y = parts->y;
		orig_w = parts->width;
		orig_h = parts->height;
		dragging_handle = i;
		cursor_pos = 0;
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
    cursor_pos = 0;
    
    if (x >= x1 && x < x2 && y >= y1 && y < y2) {
	focused_parts = parts;
	return TRUE;
    }
    return FALSE;
}

void text_drag_step(struct parts_t *p, int x, int y)
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

void text_drag_fini(struct parts_t *parts, int x, int y)
{
}

struct parts_t *text_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    
    p->type = PARTS_TEXT;
    p->x = x;
    p->y = y;
    p->width = 100;
    p->height = 50;
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    p->text = g_strdup("");
    
    return p;
}

static char *insert_string(const char *orig, int pos, const char *str)
{
    return g_strdup_printf("%.*s%s%s", cursor_pos, orig, str, orig + pos);
}

static void insert_string_at_cursor(struct parts_t *parts, const char *str)
{
    gchar *new_str = insert_string(parts->text, cursor_pos, str);
    if (parts->text != NULL)
	g_free(parts->text);
    parts->text = new_str;
    cursor_pos += strlen(str);
}

gboolean text_filter_keypress(struct parts_t *parts, GdkEventKey *ev)
{
    if (im_context != NULL) {
	if (focused_parts != NULL) {
	    if (ev->keyval == GDK_KEY_Right) {
		int cursor_next = cursor_next_pos_in_bytes(focused_parts, cursor_pos);
		cursor_pos = cursor_next;
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	    if (ev->keyval == GDK_KEY_Left) {
		int cursor_next = cursor_prev_pos_in_bytes(focused_parts, cursor_pos);
		cursor_pos = cursor_next;
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	    if (ev->keyval == GDK_KEY_Return) {
		insert_string_at_cursor(parts, "\n");
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	}
	if (gtk_im_context_filter_keypress (im_context, ev))
	    return TRUE;
    }
    return FALSE;
}

static void im_context_commit_cb(GtkIMContext *imc, gchar *str, gpointer user_data)
{
    if (im_context == NULL)
	return;
    if (focused_parts == NULL)
	return;
    if (cursor_pos >= strlen(focused_parts->text))
	cursor_pos = strlen(focused_parts->text);
    if (cursor_pos < 0)
	cursor_pos = 0;
    insert_string_at_cursor(focused_parts, str);
    
    gtk_widget_queue_draw(drawable);
}

static gboolean im_context_retrieve_surrounding_cb(GtkIMContext *imc, gpointer user_data)
{
    gtk_im_context_set_surrounding(imc, "", -1, 0);
    return TRUE;
}

static gboolean im_context_delete_surrounding_cb(GtkIMContext *imc, int offset, int n_chars, gpointer user_data)
{
    return TRUE;
}

static void im_context_preedit_changed_cb(GtkIMContext *imc, gpointer user_data)
{
    char *str;
    PangoAttrList *attrs;
    int pos;
    
    if (im_context == NULL)
	return;
    
    gtk_im_context_get_preedit_string(imc, &str, &attrs, &pos);
    
    if (preedit.str != NULL)
	g_free(preedit.str);
    if (preedit.attrs != NULL)
	pango_attr_list_unref(preedit.attrs);
    preedit.str = str;
    preedit.attrs = attrs;

    gtk_widget_queue_draw(drawable);
}

static void im_context_preedit_end_cb(GtkIMContext *imc, gpointer user_data)
{
    if (im_context == NULL)
	return;
    
    if (preedit.str != NULL)
	g_free(preedit.str);
    if (preedit.attrs != NULL)
	pango_attr_list_unref(preedit.attrs);
    preedit.str = NULL;
    preedit.attrs = NULL;
}

static void im_context_preedit_start_cb(GtkIMContext *imc, gpointer user_data)
{
}

void text_focus_in(void)
{
    if (im_context != NULL) {
	gtk_im_context_reset (im_context);
	gtk_im_context_set_client_window (im_context, gtk_widget_get_window (toplevel));
	gtk_im_context_focus_in (im_context);
    }
}

void text_focus_out(void)
{
    if (im_context != NULL) {
	gtk_im_context_reset (im_context);
	gtk_im_context_focus_out (im_context);
	gtk_im_context_set_client_window (im_context, NULL);
    }
}

void text_init(GtkWidget *top, GtkWidget *w)
{
    toplevel = top;
    drawable = w;
    
    im_context = gtk_im_multicontext_new();
    g_signal_connect(im_context, "commit", G_CALLBACK(im_context_commit_cb), NULL);
    g_signal_connect(im_context, "retrieve-surrounding", G_CALLBACK(im_context_retrieve_surrounding_cb), NULL);
    g_signal_connect(im_context, "delete-surrounding", G_CALLBACK(im_context_delete_surrounding_cb), NULL);
    g_signal_connect(im_context, "preedit-changed", G_CALLBACK(im_context_preedit_changed_cb), NULL);
    g_signal_connect(im_context, "preedit-end", G_CALLBACK(im_context_preedit_end_cb), NULL);
    g_signal_connect(im_context, "preedit-start", G_CALLBACK(im_context_preedit_start_cb), NULL);
    gtk_im_context_set_use_preedit (im_context, TRUE);

    text_focus_in();
}
