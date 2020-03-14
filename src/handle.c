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
#include <gtk/gtk.h>

#include "handle.h"

void handle_draw(struct handle_t *handles, int nr, cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    for (int i = 0; i < nr; i++) {
	cairo_rectangle(cr, handles[i].x, handles[i].y, handles[i].width, handles[i].height);
	cairo_stroke(cr);
    }
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    double dashes = 1;
    cairo_set_dash(cr, &dashes, 1, 0);
    for (int i = 0; i < nr; i++) {
	cairo_rectangle(cr, handles[i].x, handles[i].y, handles[i].width, handles[i].height);
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
