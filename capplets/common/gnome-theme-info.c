#include <config.h>

#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <glib-object.h>
#include <libgnome/gnome-desktop-item.h>
#include "gnome-theme-info.h"

#define GTK_THEME_KEY "X-GNOME-Metatheme/GtkTheme"
#define METACITY_THEME_KEY "X-GNOME-Metatheme/MetacityTheme"
#define SAWFISH_THEME_KEY "X-GNOME-Metatheme/SawfishTheme"
#define ICON_THEME_KEY "X-GNOME-Metatheme/IconTheme"
#define SOUND_THEME_KEY "X-GNOME-Metatheme/SoundTheme"
#define APPLICATION_FONT_KEY "X-GNOME-Metatheme/ApplicationFont"
#define BACKGROUND_IMAGE_KEY "X-GNOME-Metatheme/BackgroundImage"


typedef struct _ThemeCallbackData
{
  GFunc func;
  gpointer  data;
} ThemeCallbackData;


GHashTable *theme_hash = NULL;
GHashTable *icon_theme_hash = NULL;
GHashTable *meta_theme_hash = NULL;
GList *callbacks = NULL;


const gchar *gtk2_suffix = "gtk-2.0";
const gchar *key_suffix = "gtk-2.0-key";
const gchar *metacity_suffix = "metacity-1";
const gchar *icon_theme_file = "index.theme";
const gchar *meta_theme_file = "index.theme";

static GnomeThemeMetaInfo *
read_meta_theme (const gchar *theme_name,
		 const gchar *meta_theme_file)
{
  GnomeThemeMetaInfo *meta_theme_info;
  GnomeDesktopItem *meta_theme_ditem;
  const gchar *str;

  meta_theme_ditem = gnome_desktop_item_new_from_file (meta_theme_file, 0, NULL);
  if (meta_theme_ditem == NULL)
    return NULL;

  meta_theme_info = gnome_theme_meta_info_new ();
  meta_theme_info->path = g_strdup (meta_theme_file);

  str = gnome_desktop_item_get_string (meta_theme_ditem, GNOME_DESKTOP_ITEM_NAME);
  if (str == NULL)
    {
      gnome_theme_meta_info_free (meta_theme_info);
      return NULL;
    }
  meta_theme_info->readable_name = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, GNOME_DESKTOP_ITEM_COMMENT);
  if (str != NULL)
    meta_theme_info->comment = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, GNOME_DESKTOP_ITEM_ICON);
  if (str != NULL)
    meta_theme_info->icon_file = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, GTK_THEME_KEY);
  if (str == NULL)
    {
      gnome_theme_meta_info_free (meta_theme_info);
      return NULL;
    }
  meta_theme_info->gtk_theme_name = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, METACITY_THEME_KEY);
  if (str == NULL)
    {
      gnome_theme_meta_info_free (meta_theme_info);
      return NULL;
    }
  meta_theme_info->metacity_theme_name = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, ICON_THEME_KEY);
  if (str == NULL)
    {
      gnome_theme_meta_info_free (meta_theme_info);
      return NULL;
    }
  meta_theme_info->icon_theme_name = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, APPLICATION_FONT_KEY);
  if (str != NULL)
    meta_theme_info->application_font = g_strdup (str);
    
  str = gnome_desktop_item_get_string (meta_theme_ditem, BACKGROUND_IMAGE_KEY);
  if (str != NULL)
    meta_theme_info->background_image = g_strdup (str);

  return meta_theme_info;
}

static GnomeThemeIconInfo *
read_icon_theme (const gchar *icon_theme_file)
{
  GnomeThemeIconInfo *icon_theme_info;
  GnomeDesktopItem *icon_theme_ditem;
  const gchar *name;

  icon_theme_ditem = gnome_desktop_item_new_from_file (icon_theme_file, 0, NULL);
  if (icon_theme_ditem == NULL)
    return NULL;

  name = gnome_desktop_item_get_string (icon_theme_ditem, "Icon Theme/Name");
  if (name == NULL)
    return NULL;

  icon_theme_info = gnome_theme_icon_info_new ();
  icon_theme_info->name = g_strdup (name);
  icon_theme_info->path = g_strdup (icon_theme_file);

  return icon_theme_info;
}

static void
update_theme_dir (const gchar *theme_dir)
{
  GnomeThemeInfo *info = NULL;
  gboolean changed = FALSE;
  gboolean has_gtk = FALSE;
  gboolean has_keybinding = FALSE;
  gboolean has_metacity = FALSE;
  gchar *tmp;

  tmp = g_build_filename (theme_dir, meta_theme_file, NULL);
  if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR))
    {
      GnomeThemeMetaInfo *meta_theme_info;

      meta_theme_info = read_meta_theme (tmp, strrchr (theme_dir, '/'));
      if (meta_theme_info != NULL)
	g_hash_table_insert (meta_theme_hash, meta_theme_info->name, meta_theme_info);
    }
  g_free (tmp);

  tmp = g_build_filename (theme_dir, gtk2_suffix, NULL);
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

  tmp = g_build_filename (theme_dir, metacity_suffix, NULL);
  if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
    {
      has_metacity = TRUE;
    }
  g_free (tmp);

  info = gnome_theme_info_find_by_dir (theme_dir);

  if (info)
    {
      if (!has_gtk && ! has_keybinding && ! has_metacity)
	{
	  g_hash_table_remove (theme_hash, info->name);
	  gnome_theme_info_free (info);
	  changed = TRUE;
	}
      else if ((info->has_keybinding != has_keybinding) ||
	       (info->has_gtk != has_gtk) ||
	       (info->has_metacity != has_metacity))
	{
	  info->has_keybinding = has_keybinding;
	  info->has_gtk = has_gtk;
	  info->has_metacity = has_metacity;
	  changed = TRUE;
	}
    }
  else
    {
      if (has_gtk || has_keybinding || has_metacity)
	{
	  info = gnome_theme_info_new ();
	  info->path = g_strdup (theme_dir);
	  info->name = g_strdup (strrchr (theme_dir, '/') + 1);
	  info->has_gtk = has_gtk;
	  info->has_keybinding = has_keybinding;
	  info->has_metacity = has_metacity;

	  g_hash_table_insert (theme_hash, info->name, info);
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
update_icon_theme_dir (const gchar *theme_dir)
{
  GnomeThemeIconInfo *icon_theme_info = NULL;
  gboolean changed = FALSE;
  gchar *tmp;

  tmp = g_build_filename (theme_dir, icon_theme_file, NULL);
  if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR))
    {
      icon_theme_info = read_icon_theme (tmp);
    }

  g_free (tmp);

  if (icon_theme_info)
    {
      g_hash_table_insert (icon_theme_hash, icon_theme_info->name, icon_theme_info);
      changed = TRUE;
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
  GFreeFunc *func = user_data;

  switch (event_type)
    {
    case GNOME_VFS_MONITOR_EVENT_CHANGED:
    case GNOME_VFS_MONITOR_EVENT_CREATED:
    case GNOME_VFS_MONITOR_EVENT_DELETED:
      if (!strncmp (info_uri, "file://", strlen ("file://")))
	(*func) ((char *)info_uri + strlen ("file://"));
      else
	(*func) ((char *)info_uri);
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
			 update_theme_dir);

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

static void
icon_themes_add_dir (const char *dirname)
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
			 update_icon_theme_dir);

  if (!dir)
    return;

  while ((de = readdir (dir)))
    {
      char *tmp;

      if (de->d_name[0] == '.')
	continue;

      tmp = g_build_filename (dirname, de->d_name, NULL);
      update_icon_theme_dir (tmp);
      g_free (tmp);
    }
  closedir (dir);
}

static void
gnome_theme_info_init (void)
{
  static gboolean initted = FALSE;
  gchar *dir;
  GnomeVFSURI *uri;

  if (initted)
    return;
  initted = TRUE;

  theme_hash = g_hash_table_new (g_str_hash, g_str_equal);
  icon_theme_hash = g_hash_table_new (g_str_hash, g_str_equal);
  meta_theme_hash = g_hash_table_new (g_str_hash, g_str_equal);

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);

  /* Make sure ~/.themes exists */
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);

  themes_common_list_add_dir (dir);
  g_free (dir);

  dir = gtk_rc_get_theme_dir ();
  themes_common_list_add_dir (dir);
  g_free (dir);

  /* handle icon themes */
  dir = g_build_filename (g_get_home_dir (), ".icons", NULL);

  /* Make sure ~/.themes exists */
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);

  icon_themes_add_dir (dir);
  g_free (dir);

  dir = gtk_rc_get_theme_dir ();
  icon_themes_add_dir (dir);
  g_free (dir);

  /* Finally, the weird backup for icon themes */
  icon_themes_add_dir ("/usr/share/icons");
}


/* Public functions
 */

/* Generic Themes */
GnomeThemeInfo *
gnome_theme_info_new (void)
{
  GnomeThemeInfo *theme_info;

  theme_info = g_new0 (GnomeThemeInfo, 1);

  return theme_info;
}

void
gnome_theme_info_free (GnomeThemeInfo *theme_info)
{
  g_free (theme_info->path);
  g_free (theme_info->name);
  g_free (theme_info);
}

GnomeThemeInfo *
gnome_theme_info_find (const gchar *theme_name)
{
  gnome_theme_info_init ();

  return g_hash_table_lookup (theme_hash, theme_name);
}


struct GnomeThemeInfoHashData
{
  gconstpointer user_data;
  GList *list;
};

static void
gnome_theme_info_find_by_type_helper (gpointer key,
				      gpointer value,
				      gpointer user_data)
{
  GnomeThemeInfo *theme_info = value;
  struct GnomeThemeInfoHashData *hash_data = user_data;
  guint elements = GPOINTER_TO_INT (hash_data->user_data);
  gboolean add_theme = FALSE;

  if (elements & GNOME_THEME_METACITY &&
      theme_info->has_metacity)
    add_theme = TRUE;
  if (elements & GNOME_THEME_GTK_2 &&
      theme_info->has_gtk)
    add_theme = TRUE;
  if (elements & GNOME_THEME_GTK_2_KEYBINDING &&
      theme_info->has_keybinding)
    add_theme = TRUE;

  if (add_theme)
    hash_data->list = g_list_prepend (hash_data->list, theme_info);
}


static void
gnome_theme_info_find_by_dir_helper (gpointer key,
				     gpointer value,
				     gpointer user_data)
{
  GnomeThemeInfo *theme_info = value;
  struct GnomeThemeInfoHashData *hash_data = user_data;

  if (! strcmp (hash_data->user_data, theme_info->path))
    hash_data->list = g_list_prepend (hash_data->list, theme_info);
}

GList *
gnome_theme_info_find_by_type (guint elements)
{
  struct GnomeThemeInfoHashData data;
  data.user_data = GINT_TO_POINTER (elements);
  data.list = NULL;

  gnome_theme_info_init ();

  g_hash_table_foreach (theme_hash,
			gnome_theme_info_find_by_type_helper,
			&data);

  return data.list;
}


GnomeThemeInfo *
gnome_theme_info_find_by_dir (const gchar *theme_dir)
{
  struct GnomeThemeInfoHashData data;
  GnomeThemeInfo *retval = NULL;

  data.user_data = theme_dir;
  data.list = NULL;

  gnome_theme_info_init ();

  g_hash_table_foreach (theme_hash,
			gnome_theme_info_find_by_dir_helper,
			&data);

  if (data.list)
    {
      retval = data.list->data;
      g_list_free (data.list);
    }

  return retval;
}

/* Icon themes */
GnomeThemeIconInfo *
gnome_theme_icon_info_new (void)
{
  GnomeThemeIconInfo *icon_theme_info;

  icon_theme_info = g_new0 (GnomeThemeIconInfo, 1);

  return icon_theme_info;
}

void
gnome_theme_icon_info_free (GnomeThemeIconInfo *icon_theme_info)
{
  g_free (icon_theme_info);
}

GnomeThemeInfo *
gnome_theme_icon_info_find (const gchar *icon_theme_name)
{
  g_return_val_if_fail (icon_theme_name != NULL, NULL);

  gnome_theme_info_init ();

  return g_hash_table_lookup (icon_theme_hash, icon_theme_name);

}





static void
gnome_theme_icon_info_find_all_helper (gpointer key,
				       gpointer value,
				       gpointer user_data)
{
  GnomeThemeIconInfo *theme_info = value;
  struct GnomeThemeInfoHashData *hash_data = user_data;

  hash_data->list = g_list_prepend (hash_data->list, theme_info);
}

GList *
gnome_theme_icon_info_find_all (void)
{
  
  struct GnomeThemeInfoHashData data;
  data.list = NULL;

  gnome_theme_info_init ();

  g_hash_table_foreach (icon_theme_hash,
			gnome_theme_icon_info_find_all_helper,
			&data);

  return data.list;
}


/* Meta themes*/
GnomeThemeMetaInfo *
gnome_theme_meta_info_new (void)
{
  GnomeThemeMetaInfo *meta_theme_info;

  meta_theme_info = g_new0 (GnomeThemeMetaInfo, 1);

  return meta_theme_info;
}

void
gnome_theme_meta_info_free (GnomeThemeMetaInfo *meta_theme_info)
{
  g_free (meta_theme_info->path);
  g_free (meta_theme_info->name);
  g_free (meta_theme_info->comment);
  g_free (meta_theme_info->application_font);
  g_free (meta_theme_info->background_image);
  g_free (meta_theme_info->gtk_theme_name);
  g_free (meta_theme_info->icon_theme_name);
  g_free (meta_theme_info->metacity_theme_name);
  
  g_free (meta_theme_info);
}

GnomeThemeMetaInfo *
gnome_theme_meta_info_find (const char *meta_theme_name)
{
  g_return_val_if_fail (meta_theme_name != NULL, NULL);

  gnome_theme_info_init ();

  return g_hash_table_lookup (meta_theme_hash, meta_theme_name);
}




static void
gnome_theme_meta_info_find_all_helper (gpointer key,
				       gpointer value,
				       gpointer user_data)
{
  GnomeThemeMetaInfo *theme_info = value;
  struct GnomeThemeInfoHashData *hash_data = user_data;

  hash_data->list = g_list_prepend (hash_data->list, theme_info);
}

GList *
gnome_theme_meta_info_find_all (void)
{
  
  struct GnomeThemeInfoHashData data;
  data.list = NULL;

  gnome_theme_info_init ();

  g_hash_table_foreach (meta_theme_hash,
			gnome_theme_meta_info_find_all_helper,
			&data);

  return data.list;
}


void
gnome_theme_info_register_theme_change (GFunc    func,
					gpointer data)
{
  ThemeCallbackData *callback_data;

  g_return_if_fail (func != NULL);

  callback_data = g_new0 (ThemeCallbackData, 1);
  callback_data->func = func;
  callback_data->data = data;

  callbacks = g_list_prepend (callbacks, callback_data);
}

