#include "gnome-window-manager.h"

#include <gmodule.h>

GObject *
gnome_window_manager_new (GnomeDesktopItem *item)
{
  const char *settings_lib;
  char *module_name;
  GnomeWindowManagerNewFunc wm_new_func = NULL;
  GObject *wm;
  GModule *module;
  gboolean success;

  settings_lib = gnome_desktop_item_get_string (item, "GnomeSettingsLibrary");

  module_name = g_module_build_path (GNOME_WINDOW_MANAGER_MODULE_PATH,
				     settings_lib);

  module = g_module_open (module_name, G_MODULE_BIND_LAZY);
  if (module == NULL) {
    g_warning ("Couldn't load window manager settings module `%s' (%s)", module_name, g_module_error ());
    return NULL;
  }

  success = g_module_symbol (module, "window_manager_new",
			     (gpointer *) &wm_new_func);  
  
  if ((!success) || wm_new_func == NULL) {
    g_warning ("Couldn't load window manager settings module `%s`, couldn't find symbol \'window_manager_new\'", module_name);
    return NULL;
  }

  wm = (wm_new_func) ();
  
  return (wm);
}
