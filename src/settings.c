#include <gtk/gtk.h>

#include "settings.h"

#define INITIAL_COLOR		"red"
#define INITIAL_FONT		"Sans 24"
#define INITIAL_THICKNESS	7

static GtkWidget *color;
static GtkWidget *font;
static GtkWidget *thickness;

static void (*color_changed_callback)(const GdkRGBA *rgba);
static void (*font_changed_callback)(const char *fontname);
static void (*thickness_changed_callback)(int thickness);

static void color_changed_cb(GtkColorButton *widget, gpointer user_data)
{
    if (color_changed_callback != NULL)
	(*color_changed_callback)(settings_get_color());
}

static void font_changed_cb(GtkFontButton *widget, gpointer user_data)
{
    if (font_changed_callback != NULL) {
	char *fontname = settings_get_font();
	(*font_changed_callback)(fontname);
	g_free(fontname);
    }
}

static void thickness_changed_cb(GtkSpinButton *widget, gpointer user_data)
{
    if (thickness_changed_callback != NULL)
	(*thickness_changed_callback)(settings_get_thickness());
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

GtkWidget *settings_create_widgets(void)
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    color = gtk_color_button_new_with_rgba(initial_color());
    gtk_widget_set_tooltip_text(color, "Color");
    g_signal_connect(G_OBJECT(color), "color-set", G_CALLBACK(color_changed_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 0);
    gtk_widget_show(color);
    
    font = gtk_font_button_new_with_font(INITIAL_FONT);
    gtk_widget_set_tooltip_text(font, "Font");
    g_signal_connect(G_OBJECT(font), "font-set", G_CALLBACK(font_changed_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), font, FALSE, FALSE, 0);
    gtk_widget_show(font);
    
    thickness = gtk_spin_button_new_with_range(0, 1023, 1);
    gtk_widget_set_tooltip_text(thickness, "Thickness");
    g_signal_connect(G_OBJECT(thickness), "value-changed", G_CALLBACK(thickness_changed_cb), NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness), INITIAL_THICKNESS);
    gtk_box_pack_start(GTK_BOX(hbox), thickness, FALSE, FALSE, 0);
    gtk_widget_show(thickness);
    
    return hbox;
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
    if (color != NULL)
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color), rgba);
}

void settings_set_font(const char *fontname)
{
    if (font != NULL)
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font), fontname);
}

void settings_set_thickness(int value)
{
    if (thickness != NULL)
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness), value);
}
