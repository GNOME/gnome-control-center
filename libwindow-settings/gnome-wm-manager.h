#ifndef GNOME_WINDOW_MANAGER_LIST_H
#define GNOME_WINDOW_MANAGER_LIST_H

#include <gtk/gtk.h>

#include "gnome-window-manager.h"

void gnome_wm_manager_init (GtkWidget *some_window);

/* returns a GList of available window managers */
GList *             gnome_wm_manager_get_list     (void);

/* sets the currently active window manager in GConf */
void                gnome_wm_manager_set_current  (GnomeWindowManager *wm);

/* gets the currently active window manager from GConf */
GnomeWindowManager *gnome_wm_manager_get_current  (void);

/* change to the wm specified in GConf */
void                gnome_wm_manager_change_wm_to_settings (void);

/* return TRUE if wm1 and wm2 refer to the same window manager */
gboolean            gnome_wm_manager_same_wm (GnomeWindowManager *wm1, GnomeWindowManager *wm2);

#endif
