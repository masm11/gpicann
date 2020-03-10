#include <stdio.h>
#include <gtk/gtk.h>

#include "common.h"
#include "shapes.h"
#include "state_mgmt.h"

struct mode_edit_work_t {
    int step;
    int beg_x, beg_y;
    guint32 last_click_time;
    struct parts_t *last_click_parts;
};

static void mode_edit_init(struct mode_edit_work_t *w)
{
    memset(w, 0, sizeof *w);
}

static void mode_edit_handle(struct mode_edit_work_t *w, GdkEvent *ev)
{
    enum {
	STEP_IDLE,
	STEP_AFTER_PRESS,
	STEP_MOTION,
	STEP_EDITING_TEXT,
	STEP_AFTER_PRESS_TEXT,
    };
    
    switch (w->step) {
    case STEP_IDLE:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    for (struct parts_t *p = undoable->parts_list_end; p != NULL; p = p->back) {
		if (call_select(p, ep->x, ep->y, p == undoable->selp)) {
		    if (p->type == PARTS_BASE)
			undoable->selp = NULL;
		    else {
			undoable->selp = p;
			w->step = STEP_AFTER_PRESS;
		    }
		    break;
		}
	    }
	}
	break;
	
    case STEP_AFTER_PRESS:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    if (ep->time - w->last_click_time < 500 && w->last_click_parts == undoable->selp && undoable->selp->type == PARTS_TEXT) {
		/* double click */
		text_focus(undoable->selp, ep->x, ep->y);
		w->step = STEP_EDITING_TEXT;
	    } else {
		/* maybe single */
		w->last_click_parts = undoable->selp;
		w->last_click_time = ep->time;
		w->step = STEP_IDLE;
	    }
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    int dx = ep->x - w->beg_x;
	    int dy = ep->y - w->beg_y;
	    if (dx < 0)
		dx = -dx;
	    if (dy < 0)
		dy = -dy;
#define EPSILON 3
	    if (dx >= EPSILON || dy >= EPSILON) {
		history_copy_top_of_undoable();
		struct parts_t *p = undoable->selp;
		call_drag_step(p, ep->x, ep->y);
		w->last_click_parts = NULL;
		w->last_click_time = 0;
		w->step = STEP_MOTION;
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
	    w->step = STEP_IDLE;
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    struct parts_t *p = undoable->selp;
	    call_drag_step(p, ep->x, ep->y);
	    break;
	}
	break;
	
    case STEP_EDITING_TEXT:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    for (struct parts_t *p = undoable->parts_list_end; p != NULL; p = p->back) {
		if (call_select(p, ep->x, ep->y, p == undoable->selp)) {
		    if (p->type == PARTS_BASE) {
			undoable->selp = NULL;
			text_unfocus();
			w->step = STEP_IDLE;
		    } else if (p == undoable->selp) {
			w->step = STEP_AFTER_PRESS_TEXT;
		    } else {
			undoable->selp = p;
			text_unfocus();
			w->step = STEP_AFTER_PRESS;
		    }
		    break;
		}
	    }
	}
	break;

    case STEP_AFTER_PRESS_TEXT:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    GdkEventButton *ep = &ev->button;
	    text_focus(undoable->selp, ep->x, ep->y);
	    w->step = STEP_EDITING_TEXT;
	    break;
	}
	if (ev->type == GDK_MOTION_NOTIFY) {
	    GdkEventMotion *ep = &ev->motion;
	    int dx = ep->x - w->beg_x;
	    int dy = ep->y - w->beg_y;
	    if (dx < 0)
		dx = -dx;
	    if (dy < 0)
		dy = -dy;
#define EPSILON 3
	    if (dx >= EPSILON || dy >= EPSILON) {
		history_copy_top_of_undoable();
		struct parts_t *p = undoable->selp;
		call_drag_step(p, ep->x, ep->y);
		w->last_click_parts = NULL;
		w->last_click_time = 0;
		text_unfocus();
		w->step = STEP_MOTION;
		break;
	    }
#undef EPSILON
	}
	break;
    }
}

static void mode_edit_fini(struct mode_edit_work_t *w)
{
}

struct mode_rect_work_t {
    int step;
};

static void mode_rect_init(struct mode_rect_work_t *w)
{
    memset(w, 0, sizeof *w);
}

static void mode_rect_handle(struct mode_rect_work_t *w, GdkEvent *ev)
{
    switch (w->step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = rect_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    w->step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    w->step = 0;
	    break;
	}
	break;
    }
}

static void mode_rect_fini(struct mode_rect_work_t *w)
{
}

struct mode_arrow_work_t {
    int step;
};

static void mode_arrow_init(struct mode_arrow_work_t *w)
{
    memset(w, 0, sizeof *w);
}

static void mode_arrow_handle(struct mode_arrow_work_t *w, GdkEvent *ev)
{
    switch (w->step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = arrow_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    w->step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    w->step = 0;
	    break;
	}
	break;
    }
}

static void mode_arrow_fini(struct mode_arrow_work_t *w)
{
}

struct mode_text_work_t {
    int step;
};

static void mode_text_init(struct mode_text_work_t *w)
{
    memset(w, 0, sizeof *w);
}

static void mode_text_handle(struct mode_text_work_t *w, GdkEvent *ev)
{
    switch (w->step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = text_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    text_select(p, ev->button.x, ev->button.y, FALSE);
	    text_focus(p, ev->button.x, ev->button.y);
	    hp->selp = p;
	    
	    w->step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    w->step = 0;
	    break;
	}
	break;
    }
}

static void mode_text_fini(struct mode_text_work_t *w)
{
    text_unfocus();
}

struct mode_mask_work_t {
    int step;
};

static void mode_mask_init(struct mode_mask_work_t *w)
{
    memset(w, 0, sizeof *w);
}

static void mode_mask_handle(struct mode_mask_work_t *w, GdkEvent *ev)
{
    switch (w->step) {
    case 0:
	if (ev->type == GDK_BUTTON_PRESS && ev->button.button == 1) {
	    history_copy_top_of_undoable();
	    struct history_t *hp = undoable;
	    
	    struct parts_t *p = mask_create(ev->button.x, ev->button.y);
	    history_append_parts(hp, p);
	    
	    w->step++;
	}
	break;
	
    case 1:
	if (ev->type == GDK_MOTION_NOTIFY) {
	    undoable->parts_list_end->width = ev->motion.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->motion.y - undoable->parts_list_end->y;
	    break;
	}
	if (ev->type == GDK_BUTTON_RELEASE && ev->button.button == 1) {
	    undoable->parts_list_end->width = ev->button.x - undoable->parts_list_end->x;
	    undoable->parts_list_end->height = ev->button.y - undoable->parts_list_end->y;
	    w->step = 0;
	    break;
	}
	break;
    }
}

static void mode_mask_fini(struct mode_mask_work_t *w)
{
}

static union mode_work_t {
    struct mode_edit_work_t edit;
    struct mode_rect_work_t rect;
    struct mode_arrow_work_t arrow;
    struct mode_text_work_t text;
    struct mode_mask_work_t mask;
} work;
static int mode;
static GtkWidget *drawable;

static struct {
    void (*init)(union mode_work_t *);
    void (*handle)(union mode_work_t *, GdkEvent *ev);
    void (*fini)(union mode_work_t *);
} modes[] = {
#define INIT(func) ((void (*)(union mode_work_t *)) (func))
#define HANDLE(func) ((void (*)(union mode_work_t *, GdkEvent *)) (func))
#define FINI(func) ((void (*)(union mode_work_t *)) (func))
    { INIT(mode_edit_init), HANDLE(mode_edit_handle), FINI(mode_edit_fini) },
    { INIT(mode_rect_init), HANDLE(mode_rect_handle), FINI(mode_rect_fini) },
    { INIT(mode_arrow_init), HANDLE(mode_arrow_handle), FINI(mode_arrow_fini) },
    { INIT(mode_text_init), HANDLE(mode_text_handle), FINI(mode_text_fini) },
    { INIT(mode_mask_init), HANDLE(mode_mask_handle), FINI(mode_mask_fini) },
#undef INIT
#undef HANDLE
#undef FINI
};

void mode_init(GtkWidget *widget)
{
    drawable = widget;
    mode = MODE_EDIT;
    (*modes[mode].init)(&work);
}

void mode_handle(GdkEvent *ev)
{
    (*modes[mode].handle)(&work, ev);
    gtk_widget_queue_draw(drawable);
}

void mode_switch(int new_mode)
{
    if (new_mode != mode) {
	(*modes[mode].fini)(&work);
	mode = new_mode;
	(*modes[mode].init)(&work);
	gtk_widget_queue_draw(drawable);
    }
}
