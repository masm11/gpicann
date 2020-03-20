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

#include "common.h"
#include "settings.h"

#define INITIAL_COLOR		"red"
#define INITIAL_FONT		"Sans Bold 24"
#define INITIAL_THICKNESS	7

static GtkWidget *color;
static GtkWidget *font;
static GtkWidget *thickness;

static GdkRGBA default_color;
static char *default_fontname;
static int default_thickness;

static gboolean skip_color_callback;
static gboolean skip_font_callback;
static gboolean skip_thickness_callback;

static void (*color_changed_callback)(const GdkRGBA *rgba);
static void (*font_changed_callback)(const char *fontname);
static void (*thickness_changed_callback)(int thickness);

static void color_changed_cb(GtkColorButton *widget, gpointer user_data)
{
    if (!skip_color_callback) {
	if (color_changed_callback != NULL)
	    (*color_changed_callback)(settings_get_color());
    }
}

static void font_changed_cb(GtkFontButton *widget, gpointer user_data)
{
    if (!skip_font_callback) {
	if (font_changed_callback != NULL) {
	    char *fontname = settings_get_font();
	    (*font_changed_callback)(fontname);
	    g_free(fontname);
	}
    }
}

static void thickness_changed_cb(GtkSpinButton *widget, gpointer user_data)
{
    if (!skip_thickness_callback) {
	if (thickness_changed_callback != NULL)
	    (*thickness_changed_callback)(settings_get_thickness());
    }
}

static const GdkRGBA *initial_color(void)
{
    static gboolean inited = FALSE;
    static GdkRGBA rgba;
    if (!inited) {
	gdk_rgba_parse(&rgba, INITIAL_COLOR);
	inited = TRUE;
    }
    return &rgba;
}

void settings_set_color_changed_callback(void (*func)(const GdkRGBA *))
{
    color_changed_callback = func;
}

void settings_set_font_changed_callback(void (*func)(const char *))
{
    font_changed_callback = func;
}

void settings_set_thickness_changed_callback(void (*func)(int))
{
    thickness_changed_callback = func;
}

const GdkRGBA *settings_get_color(void)
{
    if (color != NULL) {
	static GdkRGBA rgba;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color), &rgba);
	return &rgba;
    } else
	return initial_color();
}

char *settings_get_font(void)
{
    if (font != NULL)
	return gtk_font_chooser_get_font(GTK_FONT_CHOOSER(font));
    else
	return g_strdup(INITIAL_FONT);
}

int settings_get_thickness(void)
{
    if (thickness != NULL)
	return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(thickness));
    else
	return INITIAL_THICKNESS;
}

void settings_set_color(const GdkRGBA *rgba)
{
    skip_color_callback = TRUE;
    if (color != NULL) {
	if (rgba == NULL)
	    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color), &default_color);
	else
	    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color), rgba);
    }
    skip_color_callback = FALSE;
}

void settings_set_font(const char *fontname)
{
    skip_font_callback = TRUE;
    if (font != NULL) {
	if (fontname == NULL)
	    gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font), default_fontname);
	else
	    gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font), fontname);
    }
    skip_font_callback = FALSE;
}

void settings_set_thickness(int value)
{
    skip_thickness_callback = TRUE;
    if (thickness != NULL) {
	if (value < 0)
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness), default_thickness);
	else
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness), value);
    }
    skip_thickness_callback = FALSE;
}

void settings_set_default_color(const GdkRGBA *rgba)
{
    default_color = *rgba;
}

void settings_set_default_font(const char *fontname)
{
    if (default_fontname != NULL)
	g_free(default_fontname);
    default_fontname = g_strdup(fontname);
}

void settings_set_default_thickness(int value)
{
    default_thickness = value;
}

GtkWidget *settings_create_widgets(void)
{
    default_color = *initial_color();
    default_fontname = g_strdup(INITIAL_FONT);
    default_thickness = INITIAL_THICKNESS;
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    color = gtk_color_button_new_with_rgba(initial_color());
    gtk_widget_set_tooltip_text(color, _("Color"));
    g_signal_connect(G_OBJECT(color), "color-set", G_CALLBACK(color_changed_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 0);
    gtk_widget_show(color);
    
    font = gtk_font_button_new_with_font(INITIAL_FONT);
    gtk_widget_set_tooltip_text(font, _("Font"));
    g_signal_connect(G_OBJECT(font), "font-set", G_CALLBACK(font_changed_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), font, FALSE, FALSE, 0);
    gtk_widget_show(font);
    
    thickness = gtk_spin_button_new_with_range(0, 1023, 1);
    gtk_widget_set_tooltip_text(thickness, _("Thickness"));
    g_signal_connect(G_OBJECT(thickness), "value-changed", G_CALLBACK(thickness_changed_cb), NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness), INITIAL_THICKNESS);
    gtk_box_pack_start(GTK_BOX(hbox), thickness, FALSE, FALSE, 0);
    gtk_widget_show(thickness);
    
    return hbox;
}
