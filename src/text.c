/*    gpicann - Screenshot Annotation Tool
 *    Copyright (C) 2020 Yuuki Harano
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>
#include <math.h>

#include "common.h"
#include "shapes.h"
#include "handle.h"
#include "settings.h"
#include "tcos.h"

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

static int cursor_next_pos_in_bytes(const char *text, int pos)
{
    glong pos_in_chars_till_pos, pos_in_chars_total;
    gunichar *uni_str_till_pos = g_utf8_to_ucs4(text, pos, NULL, &pos_in_chars_till_pos, NULL);
    gunichar *uni_str_total = g_utf8_to_ucs4(text, strlen(text), NULL, &pos_in_chars_total, NULL);

    if (pos_in_chars_till_pos + 1 <= pos_in_chars_total)
	pos_in_chars_till_pos++;

    glong new_pos = 0;
    gchar *str_till_newpos = g_ucs4_to_utf8(uni_str_total, pos_in_chars_till_pos, NULL, &new_pos, NULL);

    g_free(uni_str_till_pos);
    g_free(uni_str_total);
    g_free(str_till_newpos);

    return new_pos;
}

static int cursor_prev_pos_in_bytes(const char *text, int pos)
{
    glong pos_in_chars_till_pos, pos_in_chars_total;
    gunichar *uni_str_till_pos = g_utf8_to_ucs4(text, pos, NULL, &pos_in_chars_till_pos, NULL);
    gunichar *uni_str_total = g_utf8_to_ucs4(text, strlen(text), NULL, &pos_in_chars_total, NULL);

    if (pos_in_chars_till_pos - 1 >= 0)
	pos_in_chars_till_pos--;

    glong new_pos = 0;
    gchar *str_till_newpos = g_ucs4_to_utf8(uni_str_total, pos_in_chars_till_pos, NULL, &new_pos, NULL);
    
    g_free(uni_str_till_pos);
    g_free(uni_str_total);
    g_free(str_till_newpos);

    return new_pos;
}

static void set_forecolor(PangoAttrList *attr_list, int start, int end, double r, double g, double b)
{
    PangoAttribute *attr = pango_attr_foreground_new(65535 * r, 65535 * g, 65535 * b);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_change(attr_list, attr);
}

static void set_backcolor(PangoAttrList *attr_list, int start, int end, double r, double g, double b)
{
    PangoAttribute *attr = pango_attr_background_new(65535 * r, 65535 * g, 65535 * b);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_change(attr_list, attr);
}

static void set_forealpha(PangoAttrList *attr_list, int start, int end, double a)
{
    // full-transparent: 0.0 ->     0 -> 1
    // semi-transparent: 0.5 -> 32767 -> 32768
    // non-transparent:  1.0 -> 65535 -> 0
    PangoAttribute *attr = pango_attr_foreground_alpha_new((int)(65535 * a) + 1);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_change(attr_list, attr);
}

static void set_backalpha(PangoAttrList *attr_list, int start, int end, double a)
{
    PangoAttribute *attr = pango_attr_background_alpha_new((int)(65535 * a) + 1);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_change(attr_list, attr);
}

static PangoLayout *make_outline(PangoLayout *layout, int cursor_pos)
{
    layout = pango_layout_copy(layout);
    
    PangoAttrList *attr_list = pango_layout_get_attributes(layout);
    
    const char *str = pango_layout_get_text(layout);
    
    set_forecolor(attr_list, 0, strlen(str), 1, 1, 1);
    set_forealpha(attr_list, 0, strlen(str), 1);
    
    if (cursor_pos >= 0) {
	/* don't draw outline at the cursored char. */
	int cursor_next = cursor_next_pos_in_bytes(pango_layout_get_text(layout), cursor_pos);
	set_forealpha(attr_list, cursor_pos, cursor_next, 0);
    }
    
    return layout;
}

void text_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    gchar *text = g_strdup(parts->text);
    
    PangoLayout *layout = gtk_widget_create_pango_layout(drawable, text);
    pango_layout_set_width(layout, parts->width * PANGO_SCALE);
    
    PangoFontDescription *font_desc = pango_font_description_from_string(parts->fontname);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);
    
    PangoAttrList *attr_list = pango_attr_list_new();
    // +1 for preedit str coloring at the end of text.
    set_forecolor(attr_list, 0, strlen(text) + 1, parts->fg.red, parts->fg.green, parts->fg.blue);
    
    int cursoring_pos = cursor_pos;
    
    if (parts == focused_parts && preedit.attrs != NULL) {
	if (strlen(preedit.str) != 0) {
	    pango_attr_list_splice(attr_list, preedit.attrs, cursoring_pos, strlen(preedit.str));
	    gchar *str = insert_string(text, cursoring_pos, preedit.str);
	    g_free(text);
	    text = str;
	    pango_layout_set_text(layout, text, strlen(text));
	    cursoring_pos += strlen(preedit.str);
	}
    }
    
    pango_layout_set_attributes(layout, attr_list);
    
    PangoRectangle cursor_rect = {
	.x = 0,
	.y = 0,
	.width = 0,
	.height = 0,
    };
    if (parts == focused_parts) {
	pango_layout_index_to_pos(layout, cursoring_pos, &cursor_rect);
	cursor_rect.x /= PANGO_SCALE;
	cursor_rect.y /= PANGO_SCALE;
	cursor_rect.width /= PANGO_SCALE;
	cursor_rect.height /= PANGO_SCALE;
	if (cursor_rect.width == 0) {	// at the end of line.
	    PangoLayout *layout_cursor = pango_layout_copy(layout);
	    PangoRectangle rect;
	    pango_layout_set_text(layout_cursor, " ", 1);
	    pango_layout_index_to_pos(layout_cursor, 0, &rect);
	    cursor_rect.width = rect.width / PANGO_SCALE;
	    g_object_unref(layout_cursor);
	}
	if (cursor_rect.width == 0)
	    cursor_rect.width = 1;
    } else
	cursoring_pos = -1;
    
    PangoLayout *layout_outline = make_outline(layout, cursoring_pos);
    
#define DIFF 4.0
#define PADDING 32
    
    int width, height;
    pango_layout_get_size(layout, &width, &height);
    width /= PANGO_SCALE;
    height /= PANGO_SCALE;
    if (parts->width < width)
	parts->width = width;
    if (parts->height < height)
	parts->height = height;
    width += PADDING * 2;
    height += PADDING * 2;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    
    cairo_matrix_t mat;
    
    /* make text */
    
    unsigned char *data0 = g_malloc0(stride * height);
    cairo_surface_t *sf0 = cairo_image_surface_create_for_data(data0, CAIRO_FORMAT_ARGB32, width, height, stride);
    cairo_t *cr0 = cairo_create(sf0);
    
    cairo_save(cr0);
    cairo_move_to(cr0, PADDING, PADDING);
    pango_cairo_show_layout(cr0, layout);
    cairo_restore(cr0);
    cairo_surface_flush(sf0);
    
    cairo_pattern_t *pat0 = cairo_pattern_create_for_surface(sf0);
    
    /* make outline */
    
    unsigned char *data1 = g_malloc0(stride * height);
    cairo_surface_t *sf1 = cairo_image_surface_create_for_data(data1, CAIRO_FORMAT_ARGB32, width, height, stride);
    cairo_t *cr1 = cairo_create(sf1);
    
    for (int i = 0; i < TCOS_NR; i++) {
	int dx = DIFF * tcos(i);
	int dy = DIFF * tsin(i);
	cairo_matrix_init_identity(&mat);
	cairo_matrix_translate(&mat, -dx, -dy);
	cairo_pattern_set_matrix(pat0, &mat);
	
	cairo_save(cr1);
	cairo_set_source(cr1, pat0);
	cairo_rectangle(cr1, 0, 0, width, height);
	cairo_fill(cr1);
	cairo_restore(cr1);
    }
    
    cairo_matrix_init_identity(&mat);
    cairo_pattern_set_matrix(pat0, &mat);
    
    cairo_save(cr1);
    cairo_set_source(cr1, pat0);
    cairo_rectangle(cr1, 0, 0, width, height);
    cairo_fill(cr1);
    cairo_restore(cr1);
    
    cairo_surface_flush(sf1);
    
    for (int y = 0; y < height; y++) {
	uint32_t *p = (uint32_t *) (data1 + stride * y);
	for (int x = 0; x < width; x++) {
	    uint32_t argb = *p;
	    unsigned int a = argb >> 24;
	    argb = a << 24 | a << 16 | a << 8 | a;	// alpha'ed white
	    *p++ = argb;
	}
    }
    
    cairo_pattern_t *pat1 = cairo_pattern_create_for_surface(sf1);
    
    /* make shadow */
    
    static gboolean alpha_div_20_inited = FALSE;
    static unsigned char alpha_div_20[256];
    if (!alpha_div_20_inited) {
	alpha_div_20_inited = TRUE;
	for (int i = 0; i < 256; i++)
	    alpha_div_20[i] = i / 20;
    }
    unsigned char *data2 = g_malloc0(stride * height);
    for (int y = 0; y < height; y++) {
	uint32_t *sp = (uint32_t *) (data1 + stride * y);
	uint32_t *dp = (uint32_t *) (data2 + stride * y);
	for (int x = 0; x < width; x++) {
	    uint32_t argb = *sp++;
	    unsigned int a = argb >> 24;
	    *dp++ = alpha_div_20[a] << 24;	/* * 0.05, black */
	}
    }
    
    cairo_surface_t *sf2 = cairo_image_surface_create_for_data(data2, CAIRO_FORMAT_ARGB32, width, height, stride);
    cairo_pattern_t *pat2 = cairo_pattern_create_for_surface(sf2);
    
    /* draw outline shadow */
    
    for (int i = 0; i < TCOS_NR; i++) {
	int dx = DIFF * tcos(i) + DIFF / 2;
	int dy = DIFF * tsin(i) + DIFF / 2;
	
	cairo_matrix_init_identity(&mat);
	cairo_matrix_translate(&mat, -(parts->x + dx - PADDING), -(parts->y + dy - PADDING));
	cairo_pattern_set_matrix(pat2, &mat);
	
	cairo_save(cr);
	cairo_set_source(cr, pat2);
	cairo_rectangle(cr, parts->x + dx - PADDING, parts->y + dy - PADDING, width, height);
	cairo_fill(cr);
	cairo_restore(cr);
    }
    
    /* draw cursor shadow */
    
    for (int i = 0; i < TCOS_NR; i++) {
	int dx = DIFF * tcos(i) + DIFF / 2;
	int dy = DIFF * tsin(i) + DIFF / 2;
	
	cairo_save(cr);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.05);
	cairo_rectangle(cr,
		parts->x + cursor_rect.x + dx, parts->y + cursor_rect.y + dy,
		cursor_rect.width, cursor_rect.height);
	cairo_fill(cr);
	cairo_restore(cr);
    }
    
    /* draw outline */
    
    cairo_matrix_init_identity(&mat);
    cairo_matrix_translate(&mat, -(parts->x - PADDING), -(parts->y - PADDING));
    cairo_pattern_set_matrix(pat1, &mat);
    
    cairo_save(cr);
    cairo_set_source(cr, pat1);
    cairo_rectangle(cr, parts->x - PADDING, parts->y - PADDING, width, height);
    cairo_fill(cr);
    cairo_restore(cr);
    
    /* draw cursor */
    
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_rectangle(cr,
	    parts->x + cursor_rect.x, parts->y + cursor_rect.y,
	    cursor_rect.width, cursor_rect.height);
    cairo_fill(cr);
    cairo_restore(cr);
    
    /* draw text */
    
    cairo_matrix_init_identity(&mat);
    cairo_matrix_translate(&mat, -(parts->x - PADDING), -(parts->y - PADDING));
    cairo_pattern_set_matrix(pat0, &mat);
    
    cairo_save(cr);
    cairo_set_source(cr, pat0);
    cairo_rectangle(cr, parts->x - PADDING, parts->y - PADDING, width, height);
    cairo_fill(cr);
    cairo_restore(cr);
    
#undef PADDING
#undef DIFF
    
    cairo_pattern_destroy(pat2);
    cairo_pattern_destroy(pat1);
    cairo_pattern_destroy(pat0);
    cairo_surface_destroy(sf2);
    g_free(data2);
    cairo_destroy(cr1);
    cairo_surface_destroy(sf1);
    g_free(data1);
    cairo_destroy(cr0);
    cairo_surface_destroy(sf0);
    g_free(data0);
    g_object_unref(layout_outline);
    pango_attr_list_unref(attr_list);
    g_object_unref(layout);
    g_free(text);
}

void text_draw_handle(struct parts_t *parts, cairo_t *cr)
{
    struct handle_t handles[HANDLE_NR];
    make_handle_geoms(parts, handles);
    handle_draw(handles, HANDLE_NR, cr);
}

gboolean text_select(struct parts_t *parts, int x, int y, gboolean selected)
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
    
    if (x >= x1 && x < x2 && y >= y1 && y < y2)
	return TRUE;
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
    p->width = 200;
    p->height = 100;
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

void text_focus(struct parts_t *parts, int x, int y)
{
    PangoLayout *layout = gtk_widget_create_pango_layout(drawable, parts->text);
    pango_layout_set_width(layout, parts->width * PANGO_SCALE);
    
    PangoFontDescription *font_desc = pango_font_description_from_string(parts->fontname);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);
    
    int new_cursor_pos, trail;
    if (pango_layout_xy_to_index(layout, (x - parts->x) * PANGO_SCALE, (y - parts->y) * PANGO_SCALE, &new_cursor_pos, &trail)) {
	cursor_pos = new_cursor_pos;
	focused_parts = parts;
	gtk_widget_queue_draw(drawable);
    } else {
	cursor_pos = strlen(parts->text);
	focused_parts = parts;
	gtk_widget_queue_draw(drawable);
    }
    
    g_object_unref(layout);
}

void text_unfocus(void)
{
    focused_parts = NULL;
}

gboolean text_has_focus(void)
{
    return focused_parts != NULL;
}

gboolean text_filter_keypress(GdkEventKey *ev)
{
    if (im_context != NULL) {
	if (gtk_im_context_filter_keypress (im_context, ev))
	    return TRUE;
	if (focused_parts != NULL) {
	    if (ev->keyval == GDK_KEY_Right) {
		int cursor_next = cursor_next_pos_in_bytes(focused_parts->text, cursor_pos);
		cursor_pos = cursor_next;
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	    if (ev->keyval == GDK_KEY_Left) {
		int cursor_next = cursor_prev_pos_in_bytes(focused_parts->text, cursor_pos);
		cursor_pos = cursor_next;
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	    if (ev->keyval == GDK_KEY_BackSpace) {
		int new_pos = cursor_prev_pos_in_bytes(focused_parts->text, cursor_pos);
		if (new_pos < cursor_pos) {
		    gchar *new_str = g_strdup_printf("%.*s%s",
			    new_pos, focused_parts->text,
			    focused_parts->text + cursor_pos);
		    g_free(focused_parts->text);
		    focused_parts->text = new_str;
		    cursor_pos = new_pos;
		    gtk_widget_queue_draw(drawable);
		    return TRUE;
		}
	    }
	    if (ev->keyval == GDK_KEY_Return) {
		insert_string_at_cursor(focused_parts, "\n");
		gtk_widget_queue_draw(drawable);
		return TRUE;
	    }
	}
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
