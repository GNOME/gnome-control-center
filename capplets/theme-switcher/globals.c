#include "da.h"

GtkWidget *readme_display;
gchar     *readme_current;
GtkWidget *icon_display;
GtkWidget *icon_current;
GtkWidget *current_theme;
GtkWidget *system_list;
GtkWidget *user_list;
GtkWidget *preview_socket;
gint       prog_fd;
gchar      gtkrc_tmp[1024];
