#include <config.h>

#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <glib-object.h>

#include "theme-common.h"

static void theme_info_free (ThemeInfo *info);

typedef struct _ThemeCallbackData
{
  GFunc func;
  gpointer  data;
} ThemeCallbackData;

GList *theme_list = NULL;
GList *callbacks = NULL;
const gchar *suffix = "gtk-2.0";
const gchar *key_suffix = "gtk-2.0-key";


static ThemeInfo *
find_theme_info_by_dir (const gchar *theme_dir)
{
  GList *list;

  for (list = theme_list; list; list = list->next)
    {
      ThemeInfo *info = list->data;

      if (! strcmp (info->path, theme_dir))
	return info;
    }

  return NULL;
}

static void
update_theme_dir (const gchar *theme_dir)
{
  ThemeInfo *info = NULL;
  gboolean changed = FALSE;
  gboolean has_gtk = FALSE;
  gboolean has_keybinding = FALSE;
    
  gchar *tmp;

  tmp = g_build_filename (theme_dir, suffix, NULL);
  if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
    {
      has_gtk = TRUE;
    }
  g_free (tmp);
  
  tmp = g_build_filename (theme_dir, key_suffix, NULL);
  if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
    {
      has_keybinding = TRUE;
    }
  g_free (tmp);

  info = find_theme_info_by_dir (theme_dir);

  if (info)
    {
      if (!has_gtk && ! has_keybinding)
	{
	  theme_list = g_list_remove (theme_list, info);
	  theme_info_free (info);
	  changed = TRUE;
	}
      else if ((info->has_keybinding != has_keybinding) ||
	       (info->has_gtk != has_gtk))
	{
	  info->has_keybinding = has_keybinding;
	  info->has_gtk = has_gtk;
	  changed = TRUE;
	}
    }
  else
    {
      if (has_gtk || has_keybinding)
	{
	  info = g_new0 (ThemeInfo, 1);
	  info->path = g_strdup (theme_dir);
	  info->name = g_strdup (strrchr (theme_dir, '/') + 1);
	  info->has_gtk = has_gtk;
	  info->has_keybinding = has_keybinding;

	  theme_list = g_list_prepend (theme_list, info);
	  changed = TRUE;
	}
    }
  if (changed)
    {
      GList *list;

      for (list = callbacks; list; list = list->next)
	{
	  ThemeCallbackData *callback_data = list->data;

	  (* callback_data->func) ((gpointer)theme_dir, callback_data->data);
	}
    }

}

static void
top_theme_dir_changed_callback (GnomeVFSMonitorHandle    *handle,
				const gchar              *monitor_uri,
				const gchar              *info_uri,
				GnomeVFSMonitorEventType  event_type,
				gpointer                  user_data)
{
  switch (event_type)
    {
    case GNOME_VFS_MONITOR_EVENT_CHANGED:
    case GNOME_VFS_MONITOR_EVENT_CREATED:
    case GNOME_VFS_MONITOR_EVENT_DELETED:
      if (!strncmp (info_uri, "file://", strlen ("file://")))
	update_theme_dir (info_uri + strlen ("file://"));
      else
	update_theme_dir (info_uri);
      break;
    default:
      break;
    }
}

static void
themes_common_list_add_dir (const char *dirname)
{
  GnomeVFSMonitorHandle *handle = NULL;
  DIR *dir;
  struct dirent *de;

  g_return_if_fail (dirname != NULL);

  dir = opendir (dirname);

  gnome_vfs_monitor_add (&handle,
			 dirname,
			 GNOME_VFS_MONITOR_DIRECTORY,
			 top_theme_dir_changed_callback,
			 NULL);


  if (!dir)
    return;

  while ((de = readdir (dir)))
    {
      char *tmp;
		
      if (de->d_name[0] == '.')
	continue;

      tmp = g_build_filename (dirname, de->d_name, NULL);
      update_theme_dir (tmp);
      g_free (tmp);
    }
  closedir (dir);
}


void
theme_common_init (void)
{
  static gboolean initted = FALSE;
  gchar *dir;
  GnomeVFSURI *uri;

  if (initted)
    return;
  initted = TRUE;

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);

  /* Make sure it exists */
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);

  gnome_vfs_uri_unref (uri);

  themes_common_list_add_dir (dir);
  g_free (dir);

  dir = gtk_rc_get_theme_dir ();
  themes_common_list_add_dir (dir);
  g_free (dir);
}

GList *
theme_common_get_list (void)
{
  theme_common_init ();
  return theme_list;
}

void
theme_common_register_theme_change (GFunc    func,
				    gpointer data)
{
  ThemeCallbackData *callback_data;

  callback_data = g_new0 (ThemeCallbackData, 1);

  callback_data->func = func;
  callback_data->data = data;

  callbacks = g_list_prepend (callbacks, callback_data);
}

static void
theme_info_free (ThemeInfo *info)
{
  g_free (info->path);
  g_free (info->name);
  g_free (info);
}

