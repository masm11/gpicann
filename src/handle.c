#include <stdio.h>
#include <gtk/gtk.h>

#include "handle.h"

void handle_draw(struct handle_t *handles, int nr, cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    for (int i = 0; i < nr; i++) {
	cairo_rectangle(cr, handles[i].x, handles[i].y, handles[i].width, handles[i].height);
	cairo_stroke(cr);
    }
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    for (int i = 0; i < nr; i++) {
	cairo_rectangle(cr, handles[i].x+1, handles[i].y+1, handles[i].width-2, handles[i].height-2);
	cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void handle_calc_geom(struct handle_t *handles, int nr)
{
    for (int i = 0; i < nr; i++) {
	handles[i].x = handles[i].cx - 4;
	handles[i].y = handles[i].cy - 4;
	handles[i].width = 8;
	handles[i].height = 8;
    }
}
