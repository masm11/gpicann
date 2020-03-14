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
#ifndef STATE_MGMT_H__INCLUDED
#define STATE_MGMT_H__INCLUDED

enum {
    MODE_EDIT,
    MODE_RECT,
    MODE_ARROW,
    MODE_TEXT,
    MODE_MASK,
};

void mode_init(GtkWidget *widget);
void mode_handle(GdkEvent *ev);
void mode_switch(int new_mode);

#endif	/* ifndef STATE_MGMT_H__INCLUDED */
