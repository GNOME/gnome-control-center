#ifndef GNOME_WINDOW_MANAGER_LIST_H
#define GNOME_WINDOW_MANAGER_LIST_H

#include <gtk/gtk.h>

#include "gnome-window-manager.h"

void gnome_wm_manager_init (void);

/* gets the currently active window manager */
GnomeWindowManager *gnome_wm_manager_get_current (GdkScreen *screen);

gboolean gnome_wm_manager_spawn_config_tool_for_current (GdkScreen  *screen,
                                                         GError    **error);

#endif
