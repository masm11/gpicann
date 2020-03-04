#include <stdio.h>
#include <errno.h>
#include <gtk/gtk.h>

#include "common.h"
#include "shapes.h"

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
static struct history_t *undoable, *redoable;

static GtkWidget *drawable;

static gboolean text_editing = FALSE;

/**** parts ****/

struct parts_t *parts_alloc(void)
{
    struct parts_t *p = g_new0(struct parts_t, 1);
    return p;
}

struct parts_t *parts_dup(struct parts_t *orig)
{
    struct parts_t *p = g_new0(struct parts_t, 1);
    *p = *orig;
    p->next = p->back = NULL;
    if (p->text != NULL)
	p->text = g_strdup(p->text);
    // p->pixbuf はそのままでいいかな
    return p;
}

/**** history ****/

static void history_append_parts(struct history_t *hp, struct parts_t *pp)
{
    if (hp->parts_list != NULL) {
	pp->back = hp->parts_list_end;
	pp->back->next = pp;
    } else
	hp->parts_list = pp;
    hp->parts_list_end = pp;
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

static void history_copy_top_of_undoable(void)
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

static void base_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr, gboolean selected)
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
    void (*draw)(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr, gboolean selected);
    void (*draw_handle)(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr);
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

static inline void call_draw(struct parts_t *p, GtkWidget *drawable, cairo_t *cr, gboolean selected)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].draw != NULL)
	(parts_ops[p->type].draw)(p, drawable, cr, selected);
}

static inline void call_draw_handle(struct parts_t *p, GtkWidget *drawable, cairo_t *cr)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].draw_handle != NULL)
	(parts_ops[p->type].draw_handle)(p, drawable, cr);
}

static inline gboolean call_select(struct parts_t *p, int x, int y, gboolean selected)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].select != NULL)
	return (parts_ops[p->type].select)(p, x, y, selected);
    return FALSE;
}

static inline void call_drag_step(struct parts_t *p, int x, int y)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].drag_step != NULL)
	(parts_ops[p->type].drag_step)(p, x, y);
}

static inline void call_drag_fini(struct parts_t *p, int x, int y)
{
    if (p->type < 0 || p->type >= PARTS_NR) {
	fprintf(stderr, "unknown parts type: %d.\n", p->type);
	exit(1);
    }
    if (parts_ops[p->type].drag_fini != NULL)
	(parts_ops[p->type].drag_fini)(p, x, y);
}

/****/

static void draw(GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    struct parts_t *lp;
    
    for (lp = undoable->parts_list; lp != NULL; lp = lp->next) {
	cairo_save(cr);
	call_draw(lp, drawable, cr, lp == undoable->selp);
	cairo_restore(cr);
    }
    
    if ((lp = undoable->selp) != NULL) {
	cairo_save(cr);
	call_draw_handle(lp, drawable, cr);
	cairo_restore(cr);
    }
}

/****/

enum {
    MODE_EDIT,
    MODE_RECT,
    MODE_ARROW,
    MODE_TEXT,
    MODE_MASK,
};

static int mode = MODE_EDIT;

static void button_event_edit(GdkEvent *ev)
{
    static struct parts_t *previously_selected_parts = NULL;
    static int step = 0;
    static int beg_x = 0, beg_y = 0;
    
    enum {
	STEP_IDLE,
	STEP_AFTER_PRESS,
	STEP_MOTION,
	STEP_IDLE_TEXT,
	STEP_AFTER_PRESS_TEXT,
	STEP_MOTION_TEXT,
	STEP_EDIT_TEXT,
    };

    switch (step) {
    case STEP_IDLE:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    for (struct parts_t *p = undoable->parts_list_end; p != NULL; p = p->back) {
		if (call_select(p, ep->x, ep->y, p == undoable->selp)) {
		    switch (p->type) {
		    case PARTS_BASE:
			undoable->selp = NULL;
			break;
		    case PARTS_TEXT:
			undoable->selp = p;
			previously_selected_parts = NULL;
			step = STEP_AFTER_PRESS_TEXT;
			break;
		    default:
			undoable->selp = p;
			step = STEP_AFTER_PRESS;
			break;
		    }
		    break;
		}
	    }
	}
	break;
	
    case STEP_AFTER_PRESS:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    step = STEP_IDLE;
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    int dx = ep->x - beg_x;
	    int dy = ep->y - beg_y;
	    if (dx < 0)
		dx = -dx;
	    if (dy < 0)
		dy = -dy;
#define EPSILON 3
	    if (dx >= EPSILON || dy >= EPSILON) {
		history_copy_top_of_undoable();
		struct parts_t *p = undoable->selp;
		call_drag_step(p, ep->x, ep->y);
		step = STEP_MOTION;
		break;
	    }
#undef EPSILON
	}
	break;
	
    case STEP_MOTION:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    struct parts_t *p = undoable->selp;
	    call_drag_step(p, ep->x, ep->y);
	    call_drag_fini(p, ep->x, ep->y);
	    step = STEP_IDLE;
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    struct parts_t *p = undoable->selp;
	    call_drag_step(p, ep->x, ep->y);
	    break;
	}
	break;
	
    case STEP_IDLE_TEXT:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    for (struct parts_t *p = undoable->parts_list_end; p != NULL; p = p->back) {
		if (call_select(p, ep->x, ep->y, p == undoable->selp)) {
		    if (p->type != PARTS_BASE) {
			undoable->selp = p;
			if (p->type == PARTS_TEXT)
			    step = STEP_AFTER_PRESS_TEXT;
			else
			    step = STEP_AFTER_PRESS;
			break;
		    } else
			undoable->selp = NULL;
		}
	    }
	}
	break;
	
    case STEP_AFTER_PRESS_TEXT:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    if (previously_selected_parts == undoable->selp) {
		focus_on_click(undoable->selp, ev->button.x, ev->button.y);
		text_editing = TRUE;
		step = STEP_EDIT_TEXT;
	    } else {
		previously_selected_parts = undoable->selp;
		step = STEP_IDLE_TEXT;
	    }
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    int dx = ep->x - beg_x;
	    int dy = ep->y - beg_y;
	    if (dx < 0)
		dx = -dx;
	    if (dy < 0)
		dy = -dy;
#define EPSILON 3
	    if (dx >= EPSILON || dy >= EPSILON) {
		history_copy_top_of_undoable();
		struct parts_t *p = undoable->selp;
		call_drag_step(p, ep->x, ep->y);
		step = STEP_MOTION_TEXT;
		break;
	    }
#undef EPSILON
	}
	break;
	
    case STEP_MOTION_TEXT:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    struct parts_t *p = undoable->selp;
	    call_drag_step(p, ep->x, ep->y);
	    call_drag_fini(p, ep->x, ep->y);
	    previously_selected_parts = undoable->selp;
	    step = STEP_IDLE_TEXT;
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    struct parts_t *p = undoable->selp;
	    call_drag_step(p, ep->x, ep->y);
	    break;
	}
	break;
	
    case STEP_EDIT_TEXT:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    for (struct parts_t *p = undoable->parts_list_end; p != NULL; p = p->back) {
		if (call_select(p, ep->x, ep->y, p == undoable->selp)) {
		    if (p->type != PARTS_BASE) {
			undoable->selp = p;
			text_editing = FALSE;
			if (p->type == PARTS_TEXT)
			    step = STEP_AFTER_PRESS_TEXT;
			else
			    step = STEP_AFTER_PRESS;
			break;
		    } else {
			undoable->selp = NULL;
			text_editing = FALSE;
			step = STEP_AFTER_PRESS;
		    }
		}
	    }
	}
	break;
    }
    
    gtk_widget_queue_draw(drawable);
}

static void button_event_rect(GdkEvent *ev)
{
    static int step = 0;
    
    switch (step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = rect_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    step = 0;
	    break;
	}
	break;
    }
}

static void button_event_arrow(GdkEvent *ev)
{
    static int step = 0;
    
    switch (step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = arrow_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    step = 0;
	    break;
	}
	break;
    }
}

static void button_event_text(GdkEvent *ev)
{
    static int step = 0;
    
    switch (step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = text_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    text_select(p, ev->button.x, ev->button.y, FALSE);
	    hp->selp = p;
	    
	    step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    gtk_widget_queue_draw(drawable);
	    step = 0;
	    break;
	}
	break;
    }
}

static void button_event_mask(GdkEvent *ev)
{
    static int step = 0;
    
    switch (step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = mask_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    gtk_widget_queue_draw(drawable);
	    step = 0;
	    break;
	}
	break;
    }
}

static void button_event(GtkWidget *evbox, GdkEvent *ev, gpointer user_data)
{
    switch (mode) {
    case MODE_EDIT:
	button_event_edit(ev);
	break;
    case MODE_RECT:
	button_event_rect(ev);
	break;
    case MODE_ARROW:
	button_event_arrow(ev);
	break;
    case MODE_TEXT:
	button_event_text(ev);
	break;
    case MODE_MASK:
	button_event_mask(ev);
	break;
    }
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
	if (ev->keyval == GDK_KEY_BackSpace && !text_editing && undoable->selp != NULL) {
	    history_copy_top_of_undoable();
	    if (undoable->selp->back == NULL)
		undoable->parts_list = undoable->selp->next;
	    else
		undoable->selp->back->next = undoable->selp->next;
	    if (undoable->selp->next == NULL)
		undoable->parts_list_end = undoable->selp->back;
	    else
		undoable->selp->next->back = undoable->selp->back;
	    undoable->selp->back = undoable->selp->next = NULL;
	    undoable->selp = NULL;	/* leaks a little */
	    gtk_widget_queue_draw(drawable);
	    return TRUE;
	}
	if (undoable->selp != NULL && undoable->selp->type == PARTS_TEXT) {
	    if (text_filter_keypress(undoable->selp, ev))
		return TRUE;
	}
    }
    
    return FALSE;
}

static void mode_cb(GtkToolButton *item, gpointer user_data)
{
    mode = GPOINTER_TO_INT(user_data);
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
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save as PNG",
	    GTK_WINDOW(toplevel),
	    GTK_FILE_CHOOSER_ACTION_SAVE,
	    "Cancel", GTK_RESPONSE_CANCEL,
	    "OK", GTK_RESPONSE_ACCEPT,
	    NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);
    
    if (fname == NULL)
	return;
    
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
	call_draw(lp, NULL, cr, FALSE);
	cairo_restore(cr);
    }
    cairo_surface_flush(surface);
    
    save_as_png(surface);
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    if (argc < 2) {
	fprintf(stderr, "usage: gpicann <filename.png>\n");
	exit(1);
    }
    
    undoable = NULL;
    redoable = NULL;
    
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
    gtk_widget_add_events(toplevel, GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(toplevel), "key-press-event", G_CALLBACK(key_event), NULL);
    gtk_widget_show(toplevel);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(toplevel), vbox);
    gtk_widget_show(vbox);
    
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "download");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "folder-download");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(export), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "ok");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "text-editor");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(MODE_EDIT));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "rectangle");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "media-playback-stop");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(MODE_RECT));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "arrow");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "media-playlist-consecutive");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(MODE_ARROW));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "text");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "insert-text");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(MODE_TEXT));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    {
	GtkToolItem *item = gtk_tool_button_new(NULL, "mask");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "goa-panel");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(mode_cb), GINT_TO_POINTER(MODE_MASK));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_show(GTK_WIDGET(item));
    }
    
    GtkWidget *evbox = gtk_event_box_new();
    gtk_widget_add_events(evbox, GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(evbox), "button-press-event", G_CALLBACK(button_event), NULL);
    g_signal_connect(G_OBJECT(evbox), "button-release-event", G_CALLBACK(button_event), NULL);
    g_signal_connect(G_OBJECT(evbox), "motion-notify-event", G_CALLBACK(button_event), NULL);
    gtk_widget_show(evbox);
    gtk_box_pack_start(GTK_BOX(vbox), evbox, TRUE, TRUE, 0);
    
    drawable = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(drawable), "draw", G_CALLBACK(draw), NULL);
    gtk_widget_show(drawable);
    gtk_container_add(GTK_CONTAINER(evbox), drawable);
    
    text_init(toplevel, drawable);
    
    gtk_main();
    return 0;
}
