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
#ifndef SETTINGS_H__INCLUDED
#define SETTINGS_H__INCLUDED

GtkWidget *settings_create_widgets(void);
void settings_set_color_changed_callback(void (*func)(const GdkRGBA *));
void settings_set_font_changed_callback(void (*func)(const char *));
void settings_set_thickness_changed_callback(void (*func)(int));
const GdkRGBA *settings_get_color(void);
char *settings_get_font(void);
int settings_get_thickness(void);
void settings_set_color(const GdkRGBA *rgba);
void settings_set_font(const char *fontname);
void settings_set_thickness(int value);

#endif	/* ifndef SETTINGS_H__INCLUDED */
