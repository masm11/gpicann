#include <stdio.h>
#include <gtk/gtk.h>

static GtkWidget *toplevel;

enum {
    PARTS_BASE,
    PARTS_LINE,    // 矢印含む
    PARTS_TEXT,
    PARTS_RECT,

    PARTS_NR
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
    struct parts_t *parts_list, *parts_list_end;
};
static struct history_t *undoable, *redoable;

static GtkWidget *drawable;

/**** parts ****/

static struct parts_t *parts_alloc(void)
{
    struct parts_t *p = malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    return p;
}

static struct parts_t *parts_dup(struct parts_t *orig)
{
    struct parts_t *p = malloc(sizeof *p);
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
    struct history_t *hp = malloc(sizeof *hp);
    memset(hp, 0, sizeof *hp);
    
    for (struct parts_t *op = orig->parts_list; op != NULL; op = op->next) {
	struct parts_t *pp = parts_dup(op);
	history_append_parts(hp, pp);
    }
    
    return hp;
}

/****/

static void base_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    gdk_cairo_set_source_pixbuf(cr, parts->pixbuf, 0, 0);
    cairo_paint(cr);
}

/****/

static void rect_draw(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    cairo_set_source_rgba(cr, parts->fg.r, parts->fg.g, parts->fg.b, parts->fg.a);
    cairo_rectangle(cr, parts->x, parts->y, parts->width, parts->height);
    cairo_fill(cr);
}

static struct parts_t *rect_create(int x, int y)
{
    struct parts_t *p = parts_alloc();
    p->type = PARTS_RECT;
    p->x = x;
    p->y = y;
    p->fg.r = p->fg.g = p->fg.b = p->fg.a = 1.0;
    p->bg.r = p->bg.g = p->bg.b = p->bg.a = 1.0;
    return p;
}

/****/

struct {
    void (*draw)(struct parts_t *parts, GtkWidget *drawable, cairo_t *cr, gpointer user_data);
} parts_ops[PARTS_NR] = {
    { base_draw },
    { },
    { },
    { rect_draw },
};

static void draw(GtkWidget *drawable, cairo_t *cr, gpointer user_data)
{
    struct parts_t *lp;
    
    for (lp = undoable->parts_list; lp != NULL; lp = lp->next) {
	cairo_save(cr);
	if (lp->type >= 0 && lp->type < PARTS_NR) {
	    if (parts_ops[lp->type].draw != NULL)
		(parts_ops[lp->type].draw)(lp, drawable, cr, user_data);
	} else {
	    fprintf(stderr, "unknown parts type: %d.\n", lp->type);
	    exit(1);
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
    
    struct history_t *hp = history_dup(undoable);
    hp->next = undoable;
    undoable = hp;
    struct parts_t *p = rect_create(20, 30);
    p->width = 30;
    p->height = 20;
    history_append_parts(hp, p);
    
    gtk_main();
    return 0;
}
