#include <config.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <gconf/gconf-client.h>

#include "metacity-window-manager.h"

#define METACITY_THEME_KEY "/apps/metacity/general/theme"
#define METACITY_FONT_KEY  "/apps/metacity/general/titlebar_font"
#define METACITY_FOCUS_KEY "/apps/metacity/general/focus_mode"
#define METACITY_USE_SYSTEM_FONT_KEY "/apps/metacity/titlebar_uses_system_font"

static GnomeWindowManagerClass *parent_class;

struct _MetacityWindowManagerPrivate {
  int padding;
};

/* this function is called when the shared lib is loaded */
GObject *
window_manager_new (void)
{
  GObject *wm;

  wm = g_object_new (metacity_window_manager_get_type (), NULL);

  return wm;
}

static void     
metacity_set_theme (GnomeWindowManager *wm, const char *theme_name)
{
  gconf_client_set_string (gconf_client_get_default (),
			   METACITY_THEME_KEY,
			   theme_name, NULL);
}

static GList *
add_themes_from_dir (GList *current_list, const char *path)
{
  DIR *theme_dir;
  struct dirent *entry;
  char *theme_file_path;
  GList *node;
  gboolean found = FALSE;

  if (!(g_file_test (path, G_FILE_TEST_EXISTS) && g_file_test (path, G_FILE_TEST_IS_DIR))) {
    return current_list;
  }

  theme_dir = opendir (path);

  for (entry = readdir (theme_dir); entry != NULL; entry = readdir (theme_dir)) {
    theme_file_path = g_build_filename (path, entry->d_name, "metacity-theme-1.xml", NULL);

    if (g_file_test (theme_file_path, G_FILE_TEST_EXISTS)) {

      for (node = current_list; (node != NULL) && (!found); node = node->next) {
	found = (strcmp (node->data, entry->d_name) == 0);
      }
      
      if (!found) {
	current_list = g_list_prepend (current_list, g_strdup (entry->d_name));
      }
    }

    /*g_free (entry);*/
    g_free (theme_file_path);
  }
   
  closedir (theme_dir);

  return current_list;
}

static GList *  
metacity_get_theme_list (GnomeWindowManager *wm)
{
  GList *themes = NULL;
  char *home_dir_themes;

  home_dir_themes = g_build_filename (g_get_home_dir (), ".metacity/themes", NULL);

  themes = add_themes_from_dir (themes, METACITY_THEME_DIR);
  themes = add_themes_from_dir (themes, "/usr/share/metacity/themes");
  themes = add_themes_from_dir (themes, home_dir_themes);

  g_free (home_dir_themes);

  return themes;
}

static void     
metacity_set_font (GnomeWindowManager *wm, const char *font)
{
  GConfClient *client;

  client = gconf_client_get_default ();

  gconf_client_set_bool (client, METACITY_USE_SYSTEM_FONT_KEY, FALSE, NULL);
  gconf_client_set_string (client, METACITY_FONT_KEY, font, NULL);
}

static void     
metacity_set_focus_follows_mouse (GnomeWindowManager *wm, gboolean focus_follows_mouse)
{
  const char *focus_mode;

  if (focus_follows_mouse) {
    focus_mode = "sloppy";
  } else {
    focus_mode = "click";
  }

  gconf_client_set_string (gconf_client_get_default (),
			   METACITY_FOCUS_KEY,
			   focus_mode, NULL);
}

static char *
metacity_get_user_theme_folder (GnomeWindowManager *wm)
{
  return g_build_filename (g_get_home_dir (), ".metacity/themes", NULL);
}

static void
metacity_window_manager_init (MetacityWindowManager *metacity_window_manager, MetacityWindowManagerClass *class)
{
	metacity_window_manager->p = g_new0 (MetacityWindowManagerPrivate, 1);
}

static void
metacity_window_manager_finalize (GObject *object) 
{
	MetacityWindowManager *metacity_window_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_METACITY_WINDOW_MANAGER (object));

	metacity_window_manager = METACITY_WINDOW_MANAGER (object);

	g_free (metacity_window_manager->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
metacity_window_manager_class_init (MetacityWindowManagerClass *class) 
{
	GObjectClass *object_class;
	GnomeWindowManagerClass *wm_class;

	object_class = G_OBJECT_CLASS (class);
	wm_class = GNOME_WINDOW_MANAGER_CLASS (class);

	object_class->finalize = metacity_window_manager_finalize;

	wm_class->set_theme               = metacity_set_theme;
	wm_class->get_theme_list          = metacity_get_theme_list;
	wm_class->set_font                = metacity_set_font;
	wm_class->set_focus_follows_mouse = metacity_set_focus_follows_mouse;
	wm_class->get_user_theme_folder   = metacity_get_user_theme_folder;

	parent_class = g_type_class_peek_parent (class);
}

GType
metacity_window_manager_get_type (void)
{
	static GType metacity_window_manager_type = 0;

	if (!metacity_window_manager_type) {
		static GTypeInfo metacity_window_manager_info = {
			sizeof (MetacityWindowManagerClass),
			NULL, /* GBaseInitFunc */
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) metacity_window_manager_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (MetacityWindowManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) metacity_window_manager_init,
			NULL
		};

		metacity_window_manager_type = 
			g_type_register_static (gnome_window_manager_get_type (), 
						"MetacityWindowManager",
						&metacity_window_manager_info, 0);
	}

	return metacity_window_manager_type;
}


