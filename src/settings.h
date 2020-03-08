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
