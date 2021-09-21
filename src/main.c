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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "common.h"
#include "shapes.h"
#include "handle.h"
#include "settings.h"
#include "state_mgmt.h"
#include "tcos.h"

static GtkWidget *toplevel;

/* undoable の先頭が現在の状態。
 * 操作開始時に undoable の先頭を deep copy して、undoable の前に追加。
 * 操作中は undoable の先頭をいじる。
 * 操作終了時にはそのまま放置。
 * undo したら、undoable の先頭を redoable の先頭に移動して、画面を再描画。
 * redo したら、redoable の先頭を undoable の先頭に移動して、画面を再描画。
 * 操作開始時は redoable をクリア。
 *
 * ステップ数が少ないことが前提の構造。
 */
struct history_t *undoable, *redoable;

static GtkWidget *evbox;
static GtkWidget *drawable;

/**** parts ****/

struct parts_t *parts_alloc(void)
{
    struct parts_t *p = g_new0(struct parts_t, 1);
    
    p->fg = *settings_get_color();
    p->fontname = settings_get_font();
    p->thickness = settings_get_thickness();
    
    return p;
}

struct parts_t *parts_dup(struct parts_t *orig)
{
    struct parts_t *p = g_new0(struct parts_t, 1);
    *p = *orig;
    p->next = p->back = NULL;
    if (p->text != NULL)
	p->text = g_strdup(p->text);
    if (p->fontname != NULL)
	p->fontname = g_strdup(p->fontname);
    // p->pixbuf はそのままでいいかな
    return p;
}

/**** history ****/

void history_append_parts(struct history_t *hp, struct parts_t *pp)
{
    if (hp->parts_list != NULL) {
	pp->back = hp->parts_list_end;
	pp->back->next = pp;
    } else
	hp->parts_list = pp;
    hp->parts_list_end = pp;
}

static void history_insert_parts_after(struct history_t *hp, struct parts_t *pp, struct parts_t *after)
{
    pp->back = after;
    pp->next = after->next;
    
    if (after->next != NULL)
	after->next->back = pp;
    else
	hp->parts_list_end = pp;
    after->next = pp;
}

static void history_unlink_parts(struct history_t *hp, struct parts_t *parts)
{
    if (parts->back == NULL)
	hp->parts_list = parts->next;
    else
	parts->back->next = parts->next;
    
    if (parts->next == NULL)
	hp->parts_list_end = parts->back;
    else
	parts->next->back = parts->back;
    
    parts->back = parts->next = NULL;
    
    if (hp->selp == parts)
	hp->selp = NULL;
}

static struct history_t *history_dup(struct history_t *orig)
{
    struct history_t *hp = g_new0(struct history_t, 1);
    
    for (struct parts_t *op = orig->parts_list; op != NULL; op = op->next) {
	struct parts_t *pp = parts_dup(op);
	history_append_parts(hp, pp);
	if (orig->selp == op)
	    hp->selp = pp;
    }
    
    return hp;
}

void history_copy_top_of_undoable(void)
{
    struct history_t *hp = history_dup(undoable);
    hp->next = undoable;
    undoable = hp;
    
    redoable = NULL;	/* leaks a little */
}

static void history_undo(void)
{
    struct history_t *hp = undoable;
    if (hp->next == NULL) {
	/* can't undo */
	return;
    }
    undoable = hp->next;
    
    hp->next = redoable;
    redoable = hp;
}

static void history_redo(void)
{
    struct history_t *hp = redoable;
    if (hp == NULL) {
	/* can't redo */
	return;
    }
    redoable = hp->next;

    hp->next = undoable;
    undoable = hp;
}

/****/

static void base_draw(struct parts_t *parts, cairo_t *cr, gboolean selected)
{
    gdk_cairo_set_source_pixbuf(cr, parts->pixbuf, 0, 0);
    cairo_paint(cr);
}

static gboolean base_select(struct parts_t *parts, int x, int y, gboolean selected)
{
    return TRUE;
}

/****/

struct {
    void (*draw)(struct parts_t *parts, cairo_t *cr, gboolean selected);
    void (*draw_handle)(struct parts_t *parts, cairo_t *cr);
    gboolean (*select)(struct parts_t *parts, int x, int y, gboolean selected);
    void (*drag_step)(struct parts_t *parts, int x, int y);
    void (*drag_fini)(struct parts_t *parts, int x, int y);
} parts_ops[PARTS_NR] = {
    { base_draw, NULL, base_select },
    { arrow_draw, arrow_draw_handle, arrow_select, arrow_drag_step, arrow_drag_fini },
    { text_draw, text_draw_handle, text_select, text_drag_step, text_drag_fini },
    { rect_draw, rect_draw_handle, rect_select, rect_drag_step, rect_drag_fini },
    { mask_draw, mask_draw_handle, mask_select, mask_drag_step, mask_drag_fini },
};

void call_draw(struct parts_t *p, cairo_t *cr, gboolean selected)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].draw != NULL)
	(*parts_ops[p->type].draw)(p, cr, selected);
}

void call_draw_handle(struct parts_t *p, cairo_t *cr)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].draw_handle != NULL)
	(*parts_ops[p->type].draw_handle)(p, cr);
}

gboolean call_select(struct parts_t *p, int x, int y, gboolean selected)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].select != NULL)
	return (*parts_ops[p->type].select)(p, x, y, selected);
    return FALSE;
}

void call_drag_step(struct parts_t *p, int x, int y)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].drag_step != NULL)
	(*parts_ops[p->type].drag_step)(p, x, y);
}

void call_drag_fini(struct parts_t *p, int x, int y)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].drag_fini != NULL)
	(*parts_ops[p->type].drag_fini)(p, x, y);
}

/****/

static void draw(GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    struct parts_t *lp;
    
    for (lp = undoable->parts_list; lp != NULL; lp = lp->next) {
	cairo_save(cr);
	call_draw(lp, cr, lp == undoable->selp);
	cairo_restore(cr);
    }
    
    if ((lp = undoable->selp) != NULL) {
	cairo_save(cr);
	call_draw_handle(lp, cr);
	cairo_restore(cr);
    }
    
    struct parts_t *p = undoable->selp;
    if (p == NULL) {
	settings_set_color(NULL);
	settings_set_font(NULL);
	settings_set_thickness(-1);
    } else {
	settings_set_color(&p->fg);
	settings_set_font(p->fontname);
	settings_set_thickness(p->thickness);
    }
}

/****/


static void button_event(GtkWidget *evbox, GdkEvent *ev, gpointer user_data)
{
    gtk_widget_grab_focus(evbox);
    mode_handle(ev);
}

static void delete_it(void)
{
    history_copy_top_of_undoable();
    history_unlink_parts(undoable, undoable->selp);	/* leaks a little */
}

static void raise_it(void)
{
    history_copy_top_of_undoable();
    struct parts_t *p = undoable->selp;
    history_unlink_parts(undoable, p);
    history_append_parts(undoable, p);
    undoable->selp = p;
}

static void lower_it(void)
{
    history_copy_top_of_undoable();
    struct parts_t *p = undoable->selp;
    history_unlink_parts(undoable, p);
    history_insert_parts_after(undoable, p, undoable->parts_list);
    undoable->selp = p;
}

static gboolean key_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
    if (ev->type == GDK_KEY_PRESS) {
	if (ev->keyval == GDK_KEY_z && (ev->state & GDK_MODIFIER_MASK) == GDK_CONTROL_MASK) {
	    history_undo();
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (ev->keyval == GDK_KEY_Z && (ev->state & GDK_MODIFIER_MASK) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
	    history_redo();
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (ev->keyval == GDK_KEY_BackSpace && !text_has_focus() && undoable->selp != NULL) {
	    delete_it();
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (ev->keyval == GDK_KEY_f && (ev->state & GDK_MODIFIER_MASK) == GDK_CONTROL_MASK && !text_has_focus() && undoable->selp != NULL) {
	    raise_it();
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (ev->keyval == GDK_KEY_b && (ev->state & GDK_MODIFIER_MASK) == GDK_CONTROL_MASK && !text_has_focus() && undoable->selp != NULL) {
	    lower_it();
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (ev->keyval == GDK_KEY_q && (ev->state & GDK_MODIFIER_MASK) == GDK_CONTROL_MASK) {
	    exit(0);
	    return TRUE;
	}
	if (text_filter_keypress(ev))
	    return TRUE;
    }
    
    return FALSE;
}

static void mode_cb(GtkToolButton *item, gpointer user_data)
{
    mode_switch(GPOINTER_TO_INT(user_data));
}

static cairo_status_t write_png_data(void *closure, const unsigned char *data, unsigned int length)
{
    FILE *fp = closure;
    if (fwrite(data, length, 1, fp) != 1)
	return CAIRO_STATUS_WRITE_ERROR;
    return CAIRO_STATUS_SUCCESS;
}

static void save_as_png(cairo_surface_t *surface)
{
    int err;
    char *fname = NULL;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(_("Save as PNG"),
	    GTK_WINDOW(toplevel),
	    GTK_FILE_CHOOSER_ACTION_SAVE,
	    _("Cancel"), GTK_RESPONSE_CANCEL,
	    _("OK"), GTK_RESPONSE_ACCEPT,
	    NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);
    
    if (fname == NULL)
	return;
    
    struct stat st;
    if (stat(fname, &st) == 0) {
	GtkWidget *dialog3 = gtk_message_dialog_new(
		GTK_WINDOW(toplevel),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK_CANCEL,
		_("The file already exists. Overwrite it?"));
	int res = gtk_dialog_run(GTK_DIALOG(dialog3));
	gtk_widget_destroy(dialog3);
	if (res != GTK_RESPONSE_OK)
	    return;
    }
    
    FILE *fp = fopen(fname, "wb");
    if (fp == NULL) {
	err = errno;
	goto err;
    }
    
    cairo_surface_write_to_png_stream(surface, write_png_data, fp);
    
    if (ferror(fp)) {
	err = errno;
	fclose(fp);
	goto err;
    }
    
    if (fclose(fp) == EOF) {
	err = errno;
	goto err;
    }
    
    return;
    
 err:
    (void) 0;
    GtkWidget *dialog2 = gtk_message_dialog_new(
	    GTK_WINDOW(toplevel),
	    GTK_DIALOG_MODAL,
	    GTK_MESSAGE_ERROR,
	    GTK_BUTTONS_CLOSE,
	    "%s: %s", strerror(err), fname);
    gtk_dialog_run(GTK_DIALOG(dialog2));
    gtk_widget_destroy(dialog2);
}

static void export(GtkToolButton *item, gpointer user_data)
{
    int width = undoable->parts_list->width;
    int height = undoable->parts_list->height;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    cairo_t *cr = cairo_create(surface);
    
    for (struct parts_t *lp = undoable->parts_list; lp != NULL; lp = lp->next) {
	cairo_save(cr);
	call_draw(lp, cr, FALSE);
	cairo_restore(cr);
    }
    cairo_surface_flush(surface);
    
    save_as_png(surface);
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static void copy(GtkToolButton *item, gpointer user_data)
{
    int width = undoable->parts_list->width;
    int height = undoable->parts_list->height;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    cairo_t *cr = cairo_create(surface);

    for (struct parts_t *lp = undoable->parts_list; lp != NULL; lp = lp->next) {
    cairo_save(cr);
    call_draw(lp, cr, FALSE);
    cairo_restore(cr);
    }
    cairo_surface_flush(surface);

    GtkClipboard *clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
    gtk_clipboard_set_image (clip, pixbuf);
    g_object_unref (pixbuf);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

}

static void color_changed_cb(const GdkRGBA *rgba)
{
    if (undoable->selp != NULL) {
	history_copy_top_of_undoable();
	undoable->selp->fg = *rgba;
	
	gtk_widget_queue_draw(drawable);
    } else
	settings_set_default_color(rgba);
}

static void font_changed_cb(const char *fontname)
{
    if (undoable->selp != NULL) {
	history_copy_top_of_undoable();
	struct parts_t *p = undoable->selp;
	if (p->fontname != NULL)
	    g_free(p->fontname);
	p->fontname = g_strdup(fontname);
	
	gtk_widget_queue_draw(drawable);
    } else
	settings_set_default_font(fontname);
}

static void thickness_changed_cb(int thickness)
{
    if (undoable->selp != NULL) {
	history_copy_top_of_undoable();
	undoable->selp->thickness = thickness;
	
	gtk_widget_queue_draw(drawable);
    } else
	settings_set_default_thickness(thickness);
}

static void show_about_dialog(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), PACKAGE_NAME);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), PACKAGE_VERSION);
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "Copyright © 2020 Yuuki Harano");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "Screenshot Annotation Tool");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/masm11/gpicann");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), "gpicann");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    
    gtk_init(&argc, &argv);
    if (argc < 2) {
	fprintf(stderr, "usage: gpicann <filename.png>\n");
	exit(1);
    }
    
    undoable = NULL;
    redoable = NULL;
    
    prepare_icons();
    
    GError *err = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(argv[1], &err);
    if (pixbuf == NULL) {
	fprintf(stderr, "%s", err->message);
	exit(1);
    }
    
    struct parts_t *initial = parts_alloc();
    initial->type = PARTS_BASE;
    initial->width = gdk_pixbuf_get_width(pixbuf);
    initial->height = gdk_pixbuf_get_height(pixbuf);
    initial->pixbuf = pixbuf;
    
    struct history_t *hist = g_new0(struct history_t, 1);
    hist->parts_list = initial;
    
    undoable = hist;
    redoable = NULL;
    
    toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect_swapped(G_OBJECT(toplevel), "delete-event", G_CALLBACK(exit), 0);
    gtk_widget_show(toplevel);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(toplevel), vbox);
    gtk_widget_show(vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "Export");
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(item), _("Export"));
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "file-export-symbolic");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(export), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
    GtkToolItem *item = gtk_tool_button_new(NULL, "Copy");
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(item), _("Copy"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "copy-symbolic");
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(copy), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    struct {
	const char *tooltip;
	const char *icon_name;
	int mode;
    } mode_buttons[] = {
	{ _("Select"),    "select-symbolic",                  MODE_EDIT },
	{ _("Rectangle"), "rectangle-outline-symbolic",       MODE_RECT },
	{ _("Arrow"),     "arrow-bottom-left-thick-symbolic", MODE_ARROW },
	{ _("Text"),      "format-text-symbolic",             MODE_TEXT },
	{ _("Blur"),      "blur-symbolic",                    MODE_MASK },
    };
    GtkRadioToolButton *last_radio_tool_button = NULL;
    for (int i = 0; i < sizeof(mode_buttons) / sizeof(mode_buttons[0]); i++) {
	GtkToolItem *item = gtk_radio_tool_button_new_from_widget(last_radio_tool_button);
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(item), mode_buttons[i].tooltip);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), mode_buttons[i].icon_name);
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(mode_buttons[i].mode));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
	
	last_radio_tool_button = GTK_RADIO_TOOL_BUTTON(item);
    }
    {
	GtkToolItem *item = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    
    GtkWidget *settings = settings_create_widgets();
    gtk_box_pack_start(GTK_BOX(hbox), settings, FALSE, FALSE, 0);
    gtk_widget_show(settings);
    settings_set_color_changed_callback(color_changed_cb);
    settings_set_font_changed_callback(font_changed_cb);
    settings_set_thickness_changed_callback(thickness_changed_cb);
    
    GtkWidget *menubar = gtk_menu_bar_new();
    gtk_box_pack_end(GTK_BOX(hbox), menubar, FALSE, FALSE, 0);
    gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menubar), GTK_PACK_DIRECTION_RTL);
    gtk_widget_show(menubar);
    GtkWidget *item = gtk_menu_item_new_with_label("︙");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    gtk_widget_show(item);
    GtkWidget *menu = gtk_menu_new();
    gtk_widget_show(menu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    GtkWidget *subitem = gtk_menu_item_new_with_label(_("About gpicann..."));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), subitem);
    gtk_widget_show(subitem);
    g_signal_connect(G_OBJECT(subitem), "activate", G_CALLBACK(show_about_dialog), NULL);
    
    evbox = gtk_event_box_new();
    gtk_widget_add_events(evbox, GDK_KEY_PRESS_MASK);
    gtk_widget_set_can_focus(evbox, TRUE);
    gtk_widget_set_focus_on_click(evbox, TRUE);
    g_signal_connect(G_OBJECT(evbox), "key-press-event", G_CALLBACK(key_event), NULL);
    g_signal_connect(G_OBJECT(evbox), "button-press-event", G_CALLBACK(button_event), NULL);
    g_signal_connect(G_OBJECT(evbox), "button-release-event", G_CALLBACK(button_event), NULL);
    g_signal_connect(G_OBJECT(evbox), "motion-notify-event", G_CALLBACK(button_event), NULL);
    gtk_widget_show(evbox);
    gtk_box_pack_start(GTK_BOX(vbox), evbox, TRUE, TRUE, 0);
    
    drawable = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(drawable), "draw", G_CALLBACK(draw), NULL);
    gtk_widget_set_size_request(drawable, initial->width, initial->height);
    gtk_widget_show(drawable);
    gtk_container_add(GTK_CONTAINER(evbox), drawable);
    
    mode_init(drawable);
    text_init(toplevel, drawable);
    
    gtk_main();
    return 0;
}
