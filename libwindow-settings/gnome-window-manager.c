#include "gnome-window-manager.h"

#include <gmodule.h>

static GObjectClass *parent_class;

struct _GnomeWindowManagerPrivate {
  char *window_manager_name;
  GnomeDesktopItem *ditem;
};

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

  (GNOME_WINDOW_MANAGER (wm))->p->window_manager_name = g_strdup (gnome_desktop_item_get_string (item, GNOME_DESKTOP_ITEM_NAME));
  (GNOME_WINDOW_MANAGER (wm))->p->ditem = gnome_desktop_item_ref (item);
  
  return (wm);
}

const char * 
gnome_window_manager_get_name (GnomeWindowManager *wm)
{
  return wm->p->window_manager_name;
}

GnomeDesktopItem *
gnome_window_manager_get_ditem (GnomeWindowManager *wm)
{
  return gnome_desktop_item_ref (wm->p->ditem);
}

void         
gnome_window_manager_set_theme (GnomeWindowManager *wm, const char *theme_name)
{
  GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
  klass->set_theme (theme_name);
}

GList *
gnome_window_manager_get_theme_list (GnomeWindowManager *wm)
{
  GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
  return klass->get_theme_list ();
}

void
gnome_window_manager_set_font (GnomeWindowManager *wm, const char *font)
{
  GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
  klass->set_font (font);
}

void         
gnome_window_manager_set_focus_follows_mouse (GnomeWindowManager *wm, gboolean focus_follows_mouse)
{
  GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
  klass->set_focus_follows_mouse (focus_follows_mouse);
}

static void
gnome_window_manager_init (GnomeWindowManager *gnome_window_manager, GnomeWindowManagerClass *class)
{
	gnome_window_manager->p = g_new0 (GnomeWindowManagerPrivate, 1);
}

static void
gnome_window_manager_finalize (GObject *object) 
{
	GnomeWindowManager *gnome_window_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_GNOME_WINDOW_MANAGER (object));

	gnome_window_manager = GNOME_WINDOW_MANAGER (object);

	g_free (gnome_window_manager->p);

	parent_class->finalize (object);
}


static void
gnome_window_manager_class_init (GnomeWindowManagerClass *class) 
{
	GObjectClass *object_class;
	GnomeWindowManagerClass *wm_class;

	object_class = G_OBJECT_CLASS (class);
	wm_class = GNOME_WINDOW_MANAGER_CLASS (class);

	object_class->finalize = gnome_window_manager_finalize;

	wm_class->set_theme               = NULL;
	wm_class->get_theme_list          = NULL;
	wm_class->set_font                = NULL;
	wm_class->set_focus_follows_mouse = NULL;

	parent_class = g_type_class_peek_parent (class);
}

GType
gnome_window_manager_get_type (void)
{
	static GType gnome_window_manager_type = 0;

	if (!gnome_window_manager_type) {
		static GTypeInfo gnome_window_manager_info = {
			sizeof (GnomeWindowManagerClass),
			NULL, /* GBaseInitFunc */
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) gnome_window_manager_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (GnomeWindowManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) gnome_window_manager_init,
			NULL
		};

		gnome_window_manager_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"GWindowManager",
						&gnome_window_manager_info, 0);
	}

	return gnome_window_manager_type;
}


