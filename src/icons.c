#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "common.h"
#include "icons.inc"

void prepare_icons(void)
{
    char *cache_dir = getenv("XDG_CACHE_HOME");
    if (cache_dir == NULL)
	cache_dir = g_strdup_printf("%s/.cache", getenv("HOME"));
    char *icon_dir = g_strdup_printf("%s/gpicann/icons", cache_dir);
    char *mkdir = g_strdup_printf("mkdir -p %s", icon_dir);
    system(mkdir);
    
#define NR_ICONS (sizeof icon_table / sizeof icon_table[0])
    
    for (int i = 0; i < NR_ICONS; i++) {
	char *icon_path = g_strdup_printf("%s/%s.svg", icon_dir, icon_table[i].filename);
	FILE *fp;
	if ((fp = fopen(icon_path, "wb")) == NULL) {
	    perror(icon_path);
	    exit(1);
	}
	if (fwrite(icon_table[i].data, strlen(icon_table[i].data), 1, fp) != 1) {
	    if (ferror(fp)) {
		perror(icon_path);
		exit(1);
	    }
	    fprintf(stderr, "Is is failed to create icon cache: %s", icon_path);
	    exit(1);
	}
	fclose(fp);
    }
    
#undef NR_ICONS
    
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    gtk_icon_theme_prepend_search_path(icon_theme, icon_dir);
}
