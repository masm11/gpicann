#include <stdio.h>
#include <gtk/gtk.h>

static GtkWidget *toplevel;

enum {
    PARTS_BASE,
    PARTS_LINE,    // 矢印含む
    PARTS_TEXT,
    PARTS_RECT,
};

struct parts_t {
    struct parts_t *next, *back;
    
    int type;
    int x, y, width, height;
    int arrow_type;
    char *text;
    struct color_t {
	double r, g, b, a;
    } fg, bg;
    
    GdkPixbuf *pixbuf;
};

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
struct history_t {
    struct history_t *next;
    struct parts_t *parts_list;
};
static struct history_t *undoable, *redoable;

static GtkWidget *drawable;

static struct parts_t *parts_alloc(void)
{
    struct parts_t *p = malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    return p;
}

static void draw(GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    struct parts_t *lp;
    
    for (lp = undoable->parts_list; lp != NULL; lp = lp->next) {
	cairo_save(cr);
	switch (lp->type) {
	case PARTS_BASE:
	    gdk_cairo_set_source_pixbuf(cr, lp->pixbuf, 0, 0);
	    cairo_paint(cr);
	    break;
	default:
	    fprintf(stderr, "unknown parts type: %d.\n", lp->type);
	}
	cairo_restore(cr);
    }
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
    
    struct history_t *hist = malloc(sizeof *hist);
    memset(hist, 0, sizeof *hist);
    hist->parts_list = initial;
    
    undoable = hist;
    redoable = NULL;
    
    toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
    drawable = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(drawable), "draw", G_CALLBACK(draw), NULL);
    gtk_widget_show(drawable);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    gtk_main();
    return 0;
}
