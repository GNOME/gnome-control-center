#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <string.h>
#include <libgnome/gnome-desktop-item.h>
#include "gnome-theme-info.h"

#define GTK_THEME_KEY "X-GNOME-Metatheme/GtkTheme"
#define METACITY_THEME_KEY "X-GNOME-Metatheme/MetacityTheme"
#define SAWFISH_THEME_KEY "X-GNOME-Metatheme/SawfishTheme"
#define ICON_THEME_KEY "X-GNOME-Metatheme/IconTheme"
#define SOUND_THEME_KEY "X-GNOME-Metatheme/SoundTheme"
#define APPLICATION_FONT_KEY "X-GNOME-Metatheme/ApplicationFont"
#define BACKGROUND_IMAGE_KEY "X-GNOME-Metatheme/BackgroundImage"

/* Terminology used in this lib:
 *
 * /usr/share/themes, ~/.themes   -- top_theme_dir
 * top_theme_dir/theme_name/      -- common_theme_dir
 * /usr/share/icons, ~/.icons     -- top_icon_theme_dir
 * top_icon_theme_dir/theme_name/ -- icon_common_theme_dir
 *
 */

typedef struct _ThemeCallbackData
{
  GFunc func;
  gpointer data;
} ThemeCallbackData;

typedef struct {
  GnomeVFSMonitorHandle *common_theme_dir_handle;
  GnomeVFSMonitorHandle *gtk2_dir_handle;
  GnomeVFSMonitorHandle *keybinding_dir_handle;
  GnomeVFSMonitorHandle *metacity_dir_handle;
  gint priority;
} CommonThemeDirMonitorData;

typedef struct {
  GnomeVFSMonitorHandle *common_icon_theme_dir_handle;
  gint priority;
} CommonIconThemeDirMonitorData;

typedef struct {
  GHashTable *handle_hash;
  gint priority;
} CallbackTuple;
  

/* Hash tables */

/* The hashes_by_dir are indexed by an escaped uri of the common_theme_dir that
 * that particular theme is part of.  The data pointed to by them is a
 * GnomeTheme{Meta,Icon,}Info struct.  Note that the uri is of the form
 * "file:///home/username/.themes/foo", and not "/home/username/.themes/foo"
 */

/* The hashes_by_name are hashed by the index of the theme.  The data pointed to
 * by them is a GList whose data elements are GnomeTheme{Meta,Icon,}Info
 * structs.  This is because a theme can be found both in the users ~/.theme as
 * well as globally in $prefix.  All access to them must be done via helper
 * functions.
 */
static GList *callbacks = NULL;

static GHashTable *meta_theme_hash_by_uri;
static GHashTable *meta_theme_hash_by_name;
static GHashTable *icon_theme_hash_by_uri;
static GHashTable *icon_theme_hash_by_name;
static GHashTable *theme_hash_by_uri;
static GHashTable *theme_hash_by_name;
static gboolean initting = FALSE;

/* prototypes */
static gint                safe_strcmp                          (gchar                          *a_str,
								 gchar                          *b_str);
static gint                get_priority_from_data_by_hash       (GHashTable                     *hash_table,
								 gpointer                        data);
static void                add_data_to_hash_by_name             (GHashTable                     *hash_table,
								 gchar                          *name,
								 gpointer                        data);
static void                remove_data_from_hash_by_name        (GHashTable                     *hash_table,
								 const gchar                    *name,
								 gpointer                        data);
static gpointer            get_data_from_hash_by_name           (GHashTable                     *hash_table,
								 const gchar                    *name,
								 gint                            priority);
static GnomeThemeMetaInfo *read_meta_theme                      (GnomeVFSURI                    *meta_theme_uri);
static GnomeThemeIconInfo *read_icon_theme                      (GnomeVFSURI                    *icon_theme_uri);
static void                handle_change_signal                 (GnomeThemeType                  type,
								 gpointer                        theme,
								 GnomeThemeChangeType            change_type,
								 GnomeThemeElement               element);
static void                update_theme_index                   (GnomeVFSURI                    *index_uri,
								 GnomeThemeElement               key_element,
								 gint                            priority);
static void                update_gtk2_index                    (GnomeVFSURI                    *gtk2_index_uri,
								 gint                            priority);
static void                update_keybinding_index              (GnomeVFSURI                    *keybinding_index_uri,
								 gint                            priority);
static void                update_metacity_index                (GnomeVFSURI                    *metacity_index_uri,
								 gint                            priority);
static void                update_common_theme_dir_index        (GnomeVFSURI                    *theme_index_uri,
								 gboolean                        icon_theme,
								 gint                            priority);
static void                update_meta_theme_index              (GnomeVFSURI                    *meta_theme_index_uri,
								 gint                            priority);
static void                update_icon_theme_index              (GnomeVFSURI                    *icon_theme_index_uri,
								 gint                            priority);
static void                gtk2_dir_changed                     (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                keybinding_dir_changed               (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                metacity_dir_changed                 (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                common_theme_dir_changed             (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                common_icon_theme_dir_changed        (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                top_theme_dir_changed                (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static void                top_icon_theme_dir_changed           (GnomeVFSMonitorHandle          *handle,
								 const gchar                    *monitor_uri,
								 const gchar                    *info_uri,
								 GnomeVFSMonitorEventType        event_type,
								 gpointer                        user_data);
static GnomeVFSResult      add_common_theme_dir_monitor         (GnomeVFSURI                    *theme_dir_uri,
								 gboolean                       *monitor_not_added,
								 CommonThemeDirMonitorData      *monitor_data,
								 GError                        **error);
static GnomeVFSResult      add_common_icon_theme_dir_monitor    (GnomeVFSURI                    *theme_dir_uri,
								 gboolean                       *monitor_not_added,
								 CommonIconThemeDirMonitorData  *monitor_data,
								 GError                        **error);
static void                remove_common_theme_dir_monitor      (CommonThemeDirMonitorData      *monitor_data);
static void                remove_common_icon_theme_dir_monitor (CommonIconThemeDirMonitorData  *monitor_data);
static GnomeVFSResult      real_add_top_theme_dir_monitor       (GnomeVFSURI                    *uri,
								 gboolean                       *monitor_not_added,
								 gint                            priority,
								 gboolean                        icon_theme,
								 GError                        **error);
static GnomeVFSResult      add_top_theme_dir_monitor            (GnomeVFSURI                    *uri,
								 gboolean                       *monitor_not_added,
								 gint                            priority,
								 GError                        **error);
static GnomeVFSResult      add_top_icon_theme_dir_monitor       (GnomeVFSURI                    *uri,
								 gboolean                       *monitor_not_added,
								 gint                            priority,
								 GError                        **error);

/* private functions */
static gint
safe_strcmp (gchar *a_str,
	     gchar *b_str)
{
  if (a_str == NULL && b_str != NULL)
    return -1;
  if (a_str != NULL && b_str == NULL)
    return 1;
  if (a_str == NULL && b_str == NULL)
    return 0;
  return strcmp (a_str, b_str);
}

static gint
get_priority_from_data_by_hash (GHashTable *hash_table,
				gpointer    data)
{
  gint theme_priority = 0;
  if (hash_table == meta_theme_hash_by_name)
    theme_priority = ((GnomeThemeMetaInfo *)data)->priority;
  else if (hash_table == icon_theme_hash_by_name)
    theme_priority = ((GnomeThemeIconInfo *)data)->priority;
  else if (hash_table == theme_hash_by_name)
    theme_priority = ((GnomeThemeInfo *)data)->priority;
  else
    g_assert_not_reached ();

  return theme_priority;
}
     

static void
add_data_to_hash_by_name (GHashTable *hash_table,
			  gchar      *name,
			  gpointer    data)
{
  GList *list;

  list = g_hash_table_lookup (hash_table, name);
  if (list == NULL)
    {
      list = g_list_append (list, data);
    }
  else
    {
      GList *list_ptr = list;
      gboolean added = FALSE;
      gint priority;

      priority = get_priority_from_data_by_hash (hash_table, data);
      while (list_ptr)
	{
	  gint theme_priority;

	  theme_priority = get_priority_from_data_by_hash (hash_table, list_ptr->data);

	  if (theme_priority == priority)
	    {
	      /* Swap it in */
	      list_ptr->data = data;
	      added = TRUE;
	      break;
	    }
	  if (theme_priority > priority)
	    {
	      list = g_list_insert_before (list, list_ptr, data);
	      added = TRUE;
	      break;
	    }
	  list_ptr = list_ptr->next;
	}
      if (! added)
	list = g_list_append (list, data);
    }
  g_hash_table_insert (hash_table, g_strdup (name), list);
}

static void
remove_data_from_hash_by_name (GHashTable  *hash_table,
			       const gchar *name,
			       gpointer     data)
{
  GList *list;

  list = g_hash_table_lookup (hash_table, name);

  list = g_list_remove (list, data);
  if (list == NULL)
    g_hash_table_remove (hash_table, name);
  else
    g_hash_table_insert (hash_table, g_strdup (name), list);
    
}

static gpointer
get_data_from_hash_by_name (GHashTable  *hash_table,
			    const gchar *name,
			    gint         priority)
{
  GList *list;

  list = g_hash_table_lookup (hash_table, name);

  /* -1 implies return the first one */
  if (priority == -1)
    {
      if (list)
	return list->data;

      return NULL;
    }
  while (list)
    {
      gint theme_priority ;

      theme_priority = get_priority_from_data_by_hash (hash_table, list->data);

      if (theme_priority == priority)
	return list->data;

      list = list->next;
    }
  return NULL;
}  
  
static GnomeThemeMetaInfo *
read_meta_theme (GnomeVFSURI *meta_theme_uri)
{
  GnomeThemeMetaInfo *meta_theme_info;
  GnomeVFSURI *common_theme_dir_uri;
  GnomeDesktopItem *meta_theme_ditem;
  gchar *meta_theme_file;
  const gchar *str;

  meta_theme_file = gnome_vfs_uri_to_string (meta_theme_uri, GNOME_VFS_URI_HIDE_NONE);
  meta_theme_ditem = gnome_desktop_item_new_from_uri (meta_theme_file, 0, NULL);
  if (meta_theme_ditem == NULL)
    {
      g_free (meta_theme_file);
      return NULL;
    }
  common_theme_dir_uri = gnome_vfs_uri_get_parent (meta_theme_uri);
  
  meta_theme_info = gnome_theme_meta_info_new ();
  meta_theme_info->path = meta_theme_file;
  meta_theme_info->name = gnome_vfs_uri_extract_short_name (common_theme_dir_uri);
  gnome_vfs_uri_unref (common_theme_dir_uri);

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
read_icon_theme (GnomeVFSURI *icon_theme_uri)
{
  GnomeThemeIconInfo *icon_theme_info;
  GnomeDesktopItem *icon_theme_ditem;
  char *icon_theme_file;
  const gchar *name;

  icon_theme_file = gnome_vfs_uri_to_string (icon_theme_uri, GNOME_VFS_URI_HIDE_NONE);
  icon_theme_ditem = gnome_desktop_item_new_from_uri (icon_theme_file, 0, NULL);
  if (icon_theme_ditem == NULL)
    {
      g_free (icon_theme_file);
      return NULL;
    }

  name = gnome_desktop_item_get_string (icon_theme_ditem, "Icon Theme/Name");
  if (name == NULL)
    {
      gnome_desktop_item_unref (icon_theme_ditem);
      g_free (icon_theme_file);
      return NULL;
    }

  icon_theme_info = gnome_theme_icon_info_new ();
  icon_theme_info->name = g_strdup (name);
  icon_theme_info->path = icon_theme_file;

  gnome_desktop_item_unref (icon_theme_ditem);

  return icon_theme_info;
}

static void
handle_change_signal (GnomeThemeType       type,
		      gpointer             theme,
		      GnomeThemeChangeType change_type,
		      GnomeThemeElement    element)
{
#if DEBUG
  gchar *type_str = NULL;
  gchar *element_str = NULL;
#endif
  gchar *uri = NULL;
  GList *list;

  if (initting)
    return;

  if (type == GNOME_THEME_TYPE_REGULAR)
    uri = g_strdup (((GnomeThemeInfo *)theme)->path);
  else if (type == GNOME_THEME_TYPE_METATHEME)
    uri = g_strdup (((GnomeThemeMetaInfo *)theme)->path);
  else if (type == GNOME_THEME_TYPE_ICON)
    uri = g_strdup (((GnomeThemeIconInfo *)theme)->path);
  
  for (list = callbacks; list; list = list->next)
  {
    ThemeCallbackData *callback_data = list->data;
    (* callback_data->func) (uri, callback_data->data);
  }

#if DEBUG
  if (change_type == GNOME_THEME_CHANGE_CREATED)
    type_str = "created";
  else if (change_type == GNOME_THEME_CHANGE_CHANGED)
    type_str = "changed";
  else if (change_type == GNOME_THEME_CHANGE_DELETED)
    type_str = "deleted";

  if (element & GNOME_THEME_GTK_2)
    element_str = "gtk-2";
  else if (element & GNOME_THEME_GTK_2_KEYBINDING)
    element_str = "keybinding";
  if (element & GNOME_THEME_METACITY)
    element_str = "metacity";
  
  if (type == GNOME_THEME_TYPE_REGULAR)
    {
      g_print ("theme \"%s\" has a theme of type %s (priority %d) has been %s\n",
	       ((GnomeThemeInfo *) theme)->name,
	       element_str,
	       ((GnomeThemeInfo *) theme)->priority,
	       type_str);
    }
  else if (type == GNOME_THEME_TYPE_METATHEME)
    {
      g_print ("meta theme \"%s\" (priority %d) has been %s\n",
	       ((GnomeThemeMetaInfo *) theme)->name,
	       ((GnomeThemeMetaInfo *) theme)->priority,
	       type_str);
    }
  else if (type == GNOME_THEME_TYPE_ICON)
    {
      g_print ("icon theme \"%s\" (priority %d) has been %s\n",
	       ((GnomeThemeIconInfo *) theme)->name,
	       ((GnomeThemeIconInfo *) theme)->priority,
	       type_str);
    }
#endif
}

/* gtk2_index_uri should point to the gtkrc file that was modified */
static void
update_theme_index (GnomeVFSURI       *index_uri,
		    GnomeThemeElement  key_element,
		    gint               priority)
{
  GnomeVFSFileInfo file_info = {0,};
  GnomeVFSResult result;
  gboolean theme_exists;
  GnomeThemeInfo *theme_info;
  GnomeVFSURI *parent;
  GnomeVFSURI *common_theme_dir_uri;
  gchar *common_theme_dir;

  /* First, we determine the new state of the file.  We do no more
   * sophisticated a test than "files exists and is a file" */
  result = gnome_vfs_get_file_info_uri (index_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_REGULAR)
    theme_exists = TRUE;
  else
    theme_exists = FALSE;

  /* Next, we see what currently exists */
  parent = gnome_vfs_uri_get_parent (index_uri);
  common_theme_dir_uri = gnome_vfs_uri_get_parent (parent);
  common_theme_dir = gnome_vfs_uri_to_string (common_theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);

  theme_info = g_hash_table_lookup (theme_hash_by_uri, common_theme_dir);
  if (theme_info == NULL)
    {
      if (theme_exists)
	{
	  theme_info = gnome_theme_info_new ();
	  theme_info->path = g_strdup (common_theme_dir);
	  theme_info->name = gnome_vfs_uri_extract_short_name (common_theme_dir_uri);
	  theme_info->priority = priority;
 	  if (key_element & GNOME_THEME_GTK_2)
	    theme_info->has_gtk = TRUE;
	  else if (key_element & GNOME_THEME_GTK_2_KEYBINDING)
	    theme_info->has_keybinding = TRUE;
	  else if (key_element & GNOME_THEME_METACITY)
	    theme_info->has_metacity = TRUE;

	  g_hash_table_insert (theme_hash_by_uri, g_strdup (common_theme_dir), theme_info);
	  add_data_to_hash_by_name (theme_hash_by_name, theme_info->name, theme_info);
	  handle_change_signal (GNOME_THEME_TYPE_REGULAR, theme_info, GNOME_THEME_CHANGE_CREATED, key_element);
	}
    }
  else
    {
      gboolean theme_used_to_exist = FALSE;
      
      if (key_element & GNOME_THEME_GTK_2)
	{
	  theme_used_to_exist = theme_info->has_gtk;
	  theme_info->has_gtk = theme_exists;
	}
      else if (key_element & GNOME_THEME_GTK_2_KEYBINDING)
	{
	  theme_used_to_exist = theme_info->has_keybinding;
	  theme_info->has_keybinding = theme_exists;
	}
      else if (key_element & GNOME_THEME_METACITY)
	{
	  theme_used_to_exist = theme_info->has_metacity;
	  theme_info->has_metacity = theme_exists;
	}

      if (!theme_info->has_metacity && !theme_info->has_keybinding && !theme_info->has_gtk)
	{
	  g_hash_table_remove (theme_hash_by_uri, common_theme_dir);
	  remove_data_from_hash_by_name (theme_hash_by_name, theme_info->name, theme_info);
	}

      if (theme_exists && theme_used_to_exist)
	{
	  handle_change_signal (GNOME_THEME_TYPE_REGULAR, theme_info, GNOME_THEME_CHANGE_CHANGED, key_element);
	}
      else if (theme_exists && !theme_used_to_exist)
	{
	  handle_change_signal (GNOME_THEME_TYPE_REGULAR, theme_info, GNOME_THEME_CHANGE_CREATED, key_element);
	}
      else if (! theme_exists && theme_used_to_exist)
	{
	  handle_change_signal (GNOME_THEME_TYPE_REGULAR, theme_info, GNOME_THEME_CHANGE_DELETED, key_element);
	}

      if (!theme_info->has_metacity && !theme_info->has_keybinding && !theme_info->has_gtk)
	{
	  gnome_theme_info_free (theme_info);
	}
    }

  g_free (common_theme_dir);
  gnome_vfs_uri_unref (parent);
  gnome_vfs_uri_unref (common_theme_dir_uri);
}


static void
update_gtk2_index (GnomeVFSURI *gtk2_index_uri,
		   gint         priority)
{
  update_theme_index (gtk2_index_uri, GNOME_THEME_GTK_2, priority);
}

static void
update_keybinding_index (GnomeVFSURI *keybinding_index_uri,
			 gint         priority)
{
  update_theme_index (keybinding_index_uri, GNOME_THEME_GTK_2_KEYBINDING, priority);
}

static void
update_metacity_index (GnomeVFSURI *metacity_index_uri,
		       gint         priority)
{
  update_theme_index (metacity_index_uri, GNOME_THEME_METACITY, priority);
}

static void
update_common_theme_dir_index (GnomeVFSURI *theme_index_uri,
			       gboolean     icon_theme,
			       gint         priority)
{
  GnomeVFSFileInfo file_info = {0,};
  GnomeVFSResult result;
  gboolean theme_exists;
  gpointer theme_info;
  gpointer old_theme_info;
  GnomeVFSURI *common_theme_dir_uri;
  gchar *common_theme_dir;
  GHashTable *hash_by_uri;
  GHashTable *hash_by_name;
  gchar *name = NULL;

  if (icon_theme)
    {
      hash_by_uri = icon_theme_hash_by_uri;
      hash_by_name = icon_theme_hash_by_name;
    }
  else
    {
      hash_by_uri = meta_theme_hash_by_uri;
      hash_by_name = meta_theme_hash_by_name;
    }
  /* First, we determine the new state of the file. */
  result = gnome_vfs_get_file_info_uri (theme_index_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_REGULAR)
    {
      /* It's an interesting file.  Lets try to load it. */
      if (icon_theme)
	{
	  theme_info = read_icon_theme (theme_index_uri);
	  if (theme_info)
	    {
	      ((GnomeThemeIconInfo *) theme_info)->priority = priority;
	      theme_exists = TRUE;
	    }
	  else
	    {
	      theme_exists = FALSE;
	    }
	}
      else
	{
	  theme_info = read_meta_theme (theme_index_uri);
	  if (theme_info)
	    {
	      ((GnomeThemeMetaInfo *) theme_info)->priority = priority;
	      theme_exists = TRUE;
	    }
	  else
	    {
	      theme_exists = FALSE;
	    }
	}
    }
  else
    {
      theme_info = NULL;
      theme_exists = FALSE;
    }

  /* Next, we see what currently exists */
  common_theme_dir_uri = gnome_vfs_uri_get_parent (theme_index_uri);
  common_theme_dir = gnome_vfs_uri_to_string (common_theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);

  old_theme_info = g_hash_table_lookup (hash_by_uri, common_theme_dir);

  if (theme_exists)
    {
      if (icon_theme)
	name = ((GnomeThemeIconInfo *)theme_info)->name;
      else
	name = ((GnomeThemeMetaInfo *)theme_info)->name;
    }
  if (old_theme_info == NULL)
    {
      if (theme_exists)
	{
	  g_hash_table_insert (hash_by_uri, g_strdup (common_theme_dir), theme_info);
	  add_data_to_hash_by_name (hash_by_name, g_strdup (name), theme_info);
	  handle_change_signal (icon_theme?GNOME_THEME_TYPE_ICON:GNOME_THEME_TYPE_METATHEME,
				theme_info, GNOME_THEME_CHANGE_CREATED, 0);
	}
    }
  else
    {
      if (theme_exists)
	{
	  gint cmp;

	  if (icon_theme)
	    cmp = gnome_theme_icon_info_compare (theme_info, old_theme_info);
	  else
	    cmp = gnome_theme_meta_info_compare (theme_info, old_theme_info);
	  if (cmp != 0)
	    {
	      g_hash_table_insert (hash_by_uri, g_strdup (common_theme_dir), theme_info);
	      add_data_to_hash_by_name (hash_by_name, g_strdup (name), theme_info);
	      handle_change_signal (icon_theme?GNOME_THEME_TYPE_ICON:GNOME_THEME_TYPE_METATHEME,
				    theme_info, GNOME_THEME_CHANGE_CHANGED, 0);
	      if (icon_theme)
		gnome_theme_icon_info_free (old_theme_info);
	      else
		gnome_theme_meta_info_free (old_theme_info);
	    }
	  else
	    {
	      if (icon_theme)
		gnome_theme_icon_info_free (theme_info);
	      else
		gnome_theme_meta_info_free (theme_info);
	    }
	}
      else
	{
	  if (icon_theme)
	    name = ((GnomeThemeIconInfo *)old_theme_info)->name;
	  else
	    name = ((GnomeThemeMetaInfo *)old_theme_info)->name;

	  g_hash_table_remove (hash_by_uri, common_theme_dir);
	  remove_data_from_hash_by_name (hash_by_name, name, old_theme_info);
	  handle_change_signal (icon_theme?GNOME_THEME_TYPE_ICON:GNOME_THEME_TYPE_METATHEME,
				old_theme_info, GNOME_THEME_CHANGE_DELETED, 0);
	  if (icon_theme)
	    gnome_theme_icon_info_free (old_theme_info);
	  else
	    gnome_theme_meta_info_free (old_theme_info);
	}
    }

  g_free (common_theme_dir);
  gnome_vfs_uri_unref (common_theme_dir_uri);
}

static void
update_meta_theme_index (GnomeVFSURI *meta_theme_index_uri,
			 gint         priority)
{
  update_common_theme_dir_index (meta_theme_index_uri, FALSE, priority);
}
static void
update_icon_theme_index (GnomeVFSURI *icon_theme_index_uri,
			 gint         priority)
{
  update_common_theme_dir_index (icon_theme_index_uri, TRUE, priority);
}

static void
gtk2_dir_changed (GnomeVFSMonitorHandle *handle,
		  const gchar *monitor_uri,
		  const gchar *info_uri,
		  GnomeVFSMonitorEventType event_type,
		  gpointer user_data)
{
  GnomeVFSURI *gtk2_dir_uri;
  gchar *affected_file;
  CommonThemeDirMonitorData *monitor_data;

  monitor_data = user_data;

  gtk2_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (gtk2_dir_uri);

  /* The only file we care about is gtkrc */
  if (strcmp (affected_file, "gtkrc"))
    {
      g_free (affected_file);
      gnome_vfs_uri_unref (gtk2_dir_uri);
      return;
    }

  update_gtk2_index (gtk2_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (gtk2_dir_uri);
}


static void
keybinding_dir_changed (GnomeVFSMonitorHandle *handle,
			const gchar *monitor_uri,
			const gchar *info_uri,
			GnomeVFSMonitorEventType event_type,
			gpointer user_data)
{
  GnomeVFSURI *keybinding_dir_uri;
  gchar *affected_file;
  CommonThemeDirMonitorData *monitor_data;

  monitor_data = user_data;

  keybinding_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (keybinding_dir_uri);

  /* The only file we care about is gtkrc */
  if (strcmp (affected_file, "gtkrc"))
    {
      g_free (affected_file);
      gnome_vfs_uri_unref (keybinding_dir_uri);
      return;
    }

  update_keybinding_index (keybinding_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (keybinding_dir_uri);
}

static void
metacity_dir_changed (GnomeVFSMonitorHandle *handle,
		      const gchar *monitor_uri,
		      const gchar *info_uri,
		      GnomeVFSMonitorEventType event_type,
		      gpointer user_data)
{
  GnomeVFSURI *metacity_dir_uri;
  gchar *affected_file;
  CommonThemeDirMonitorData *monitor_data;

  monitor_data = user_data;
  
  metacity_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (metacity_dir_uri);

  /* The only file we care about is gtkrc */
  if (strcmp (affected_file, "metacity-theme-1.xml"))
    {
      g_free (affected_file);
      gnome_vfs_uri_unref (metacity_dir_uri);
      return;
    }

  update_metacity_index (metacity_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (metacity_dir_uri);
}

static void
common_theme_dir_changed (GnomeVFSMonitorHandle *handle,
			  const gchar *monitor_uri,
			  const gchar *info_uri,
			  GnomeVFSMonitorEventType event_type,
			  gpointer user_data)
{
  GnomeVFSURI *meta_theme_dir_uri;
  gchar *affected_file;
  CommonThemeDirMonitorData *monitor_data;

  monitor_data = user_data;

  meta_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (meta_theme_dir_uri);

  /* The only file we care about is index.theme */
  if (strcmp (affected_file, "index.theme"))
    {
      gnome_vfs_uri_unref (meta_theme_dir_uri);
      g_free (affected_file);
      return;
    }

  update_meta_theme_index (meta_theme_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (meta_theme_dir_uri);
}

static void
common_icon_theme_dir_changed (GnomeVFSMonitorHandle *handle,
			       const gchar *monitor_uri,
			       const gchar *info_uri,
			       GnomeVFSMonitorEventType event_type,
			       gpointer user_data)
{
  GnomeVFSURI *icon_theme_dir_uri;
  gchar *affected_file;
  CommonIconThemeDirMonitorData *monitor_data;

  monitor_data = user_data;

  icon_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (icon_theme_dir_uri);

  /* The only file we care about is index.theme*/
  if (strcmp (affected_file, "index.theme"))
    {
      gnome_vfs_uri_unref (icon_theme_dir_uri);
      g_free (affected_file);
      return;
    }
  update_icon_theme_index (icon_theme_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (icon_theme_dir_uri);
}

static void
top_theme_dir_changed (GnomeVFSMonitorHandle *handle,
		       const gchar *monitor_uri,
		       const gchar *info_uri,
		       GnomeVFSMonitorEventType event_type,
		       gpointer user_data)
{
  GnomeVFSResult result;
  CallbackTuple *tuple;
  GHashTable *handle_hash;
  CommonThemeDirMonitorData *monitor_data;
  GnomeVFSURI *common_theme_dir_uri;
  gint priority;

  common_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  tuple = user_data;
  handle_hash = tuple->handle_hash;
  priority = tuple->priority;

  if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
    {
      GnomeVFSFileInfo file_info = {0,};

      monitor_data = g_new0 (CommonThemeDirMonitorData, 1);
      monitor_data->priority = priority;
      result = gnome_vfs_get_file_info_uri (common_theme_dir_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
      if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
	  add_common_theme_dir_monitor (common_theme_dir_uri, NULL, monitor_data, NULL);
	  g_hash_table_insert (handle_hash, file_info.name, monitor_data);
	}
    }
  else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
    {
      gchar *name;
      CommonThemeDirMonitorData *monitor_data;

      name = gnome_vfs_uri_extract_short_name (common_theme_dir_uri);
      monitor_data = g_hash_table_lookup (handle_hash, name);
      if (monitor_data != NULL)
	{
	  remove_common_theme_dir_monitor (monitor_data);
	  g_hash_table_remove (handle_hash, name);
	  g_free (monitor_data);
	}
      g_free (name);
    }
  gnome_vfs_uri_unref (common_theme_dir_uri);
}

static void
top_icon_theme_dir_changed (GnomeVFSMonitorHandle    *handle,
			    const gchar              *monitor_uri,
			    const gchar              *info_uri,
			    GnomeVFSMonitorEventType  event_type,
			    gpointer                  user_data)
{
  GnomeVFSResult result;
  GHashTable *handle_hash;
  CallbackTuple *tuple;
  CommonIconThemeDirMonitorData *monitor_data;
  GnomeVFSURI *common_icon_theme_dir_uri;
  gint priority;

  common_icon_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  tuple = user_data;
  handle_hash = tuple->handle_hash;
  priority = tuple->priority;

  if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
    {
      GnomeVFSFileInfo file_info = {0,};

      monitor_data = g_new0 (CommonIconThemeDirMonitorData, 1);
      monitor_data->priority = priority;
      result = gnome_vfs_get_file_info_uri (common_icon_theme_dir_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
      if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
	  add_common_icon_theme_dir_monitor (common_icon_theme_dir_uri, NULL, monitor_data, NULL);
	  g_hash_table_insert (handle_hash, file_info.name, monitor_data);
	}
    }
  else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
    {
      gchar *name;
      CommonIconThemeDirMonitorData *monitor_data;

      name = gnome_vfs_uri_extract_short_name (common_icon_theme_dir_uri);
      monitor_data = g_hash_table_lookup (handle_hash, name);
      if (monitor_data != NULL)
	{
	  remove_common_icon_theme_dir_monitor (monitor_data);
	  g_hash_table_remove (handle_hash, name);
	  g_free (monitor_data);
	}
      g_free (name);
    }
  gnome_vfs_uri_unref (common_icon_theme_dir_uri);
}

/* Add a monitor to a common_theme_dir.
 */
static GnomeVFSResult
add_common_theme_dir_monitor (GnomeVFSURI                *theme_dir_uri,
			      gboolean                   *monitor_not_added,
			      CommonThemeDirMonitorData  *monitor_data,
			      GError                    **error)
{
  GnomeVFSResult result;
  gchar *uri_string;
  gboolean real_monitor_not_added = FALSE;
  GnomeVFSURI *subdir;
  GnomeVFSURI *index_uri;
  GnomeVFSFileInfo file_info = {0,};

  index_uri = gnome_vfs_uri_append_file_name (theme_dir_uri, "index.theme");
  update_meta_theme_index (index_uri, monitor_data->priority);
  gnome_vfs_uri_unref (index_uri);

  /* Add the handle for this directory */
  uri_string = gnome_vfs_uri_to_string (theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->common_theme_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  common_theme_dir_changed,
				  monitor_data);
  g_free (uri_string);

  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  else if (result != GNOME_VFS_OK)
    return result;

  /* gtk-2 theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "gtk-2.0");
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "gtkrc");
      update_gtk2_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->gtk2_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  gtk2_dir_changed,
				  monitor_data);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  g_free (uri_string);
  gnome_vfs_uri_unref (subdir);

  /* keybinding theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "gtk-2.0-key");
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "gtkrc");
      update_keybinding_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->keybinding_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  keybinding_dir_changed,
				  monitor_data);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  g_free (uri_string);
  gnome_vfs_uri_unref (subdir);

  /* metacity theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "metacity-1");
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (file_info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "metacity-theme-1.xml");
      update_metacity_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->metacity_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  metacity_dir_changed,
				  monitor_data);
  g_free (uri_string);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  gnome_vfs_uri_unref (subdir);

  if (monitor_not_added)
    *monitor_not_added = real_monitor_not_added;

  return GNOME_VFS_OK;
}

static GnomeVFSResult
add_common_icon_theme_dir_monitor (GnomeVFSURI                    *theme_dir_uri,
				   gboolean                       *monitor_not_added,
				   CommonIconThemeDirMonitorData  *monitor_data,
				   GError                        **error)
{
  GnomeVFSResult result;
  gchar *uri_string;
  gboolean real_monitor_not_added = FALSE;
  GnomeVFSURI *index_uri;

  /* Add the handle for this directory */
  index_uri = gnome_vfs_uri_append_file_name (theme_dir_uri, "index.theme");
  update_icon_theme_index (index_uri, monitor_data->priority);
  gnome_vfs_uri_unref (index_uri);

  uri_string = gnome_vfs_uri_to_string (theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->common_icon_theme_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  common_icon_theme_dir_changed,
				  monitor_data);
  g_free (uri_string);

  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  else if (result != GNOME_VFS_OK)
    return result;

  if (monitor_not_added)
    *monitor_not_added = real_monitor_not_added;

  return GNOME_VFS_OK;
}

static void
remove_common_theme_dir_monitor (CommonThemeDirMonitorData *monitor_data)
{
  /* None of the possible errors here are interesting */
  gnome_vfs_monitor_cancel (monitor_data->common_theme_dir_handle);
  gnome_vfs_monitor_cancel (monitor_data->gtk2_dir_handle);
  gnome_vfs_monitor_cancel (monitor_data->keybinding_dir_handle);
  gnome_vfs_monitor_cancel (monitor_data->metacity_dir_handle);

}

static void
remove_common_icon_theme_dir_monitor (CommonIconThemeDirMonitorData *monitor_data)
{
  /* None of the possible errors here are interesting */
  gnome_vfs_monitor_cancel (monitor_data->common_icon_theme_dir_handle);
}

/* Add a monitor to a top dir.  These monitors persist for the duration of the
 * lib.
 */
static GnomeVFSResult
real_add_top_theme_dir_monitor (GnomeVFSURI  *uri,
				gboolean     *monitor_not_added,
				gint          priority,
				gboolean      icon_theme,
				GError      **error)
{
  GnomeVFSMonitorHandle *monitor_handle = NULL;
  GnomeVFSDirectoryHandle *directory_handle = NULL;
  GnomeVFSResult result;
  GnomeVFSFileInfo file_info = {0,};
  gchar *uri_string;
  CallbackTuple *tuple;

  /* handle_hash is a hash of common_theme_dir names to their monitor_data.  We
   * use it to remove the monitor handles when a dir is removed.
   */
  tuple = g_new0 (CallbackTuple, 1);
  tuple->handle_hash = g_hash_table_new (g_str_hash, g_str_equal);
  tuple->priority = priority;

  /* Check the URI */
  gnome_vfs_get_file_info_uri (uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (file_info.type != GNOME_VFS_FILE_TYPE_DIRECTORY)
    return GNOME_VFS_ERROR_NOT_A_DIRECTORY;
  /* Monitor the top directory */
  uri_string = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

  result = gnome_vfs_monitor_add (&monitor_handle,
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  icon_theme?top_icon_theme_dir_changed:top_theme_dir_changed,
				  tuple);

  g_free (uri_string);

  /* We can deal with NOT_SUPPORTED manually */
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    *monitor_not_added = TRUE;
  else if (result != GNOME_VFS_OK)
    return result;

  /* Go through the directory to add monitoring */
  result = gnome_vfs_directory_open_from_uri (&directory_handle, uri, GNOME_VFS_FILE_INFO_DEFAULT);
  if (result != GNOME_VFS_OK)
    return result;

  while (gnome_vfs_directory_read_next (directory_handle, &file_info) == GNOME_VFS_OK)
    {
      GnomeVFSURI *theme_dir_uri;
      gpointer monitor_data;

      if (file_info.type != GNOME_VFS_FILE_TYPE_DIRECTORY)
	continue;
      if (file_info.name[0] == '.')
	continue;

      /* Add the directory */
      theme_dir_uri = gnome_vfs_uri_append_path (uri, file_info.name);
      if (icon_theme)
	{
	  monitor_data = g_new0 (CommonIconThemeDirMonitorData, 1);
	  ((CommonIconThemeDirMonitorData *)monitor_data)->priority = priority;
	  add_common_icon_theme_dir_monitor (theme_dir_uri, monitor_not_added, monitor_data, error);
	}
      else
	{
	  monitor_data = g_new0 (CommonThemeDirMonitorData, 1);
	  ((CommonThemeDirMonitorData *)monitor_data)->priority = priority;
	  add_common_theme_dir_monitor (theme_dir_uri, monitor_not_added, monitor_data, error);
	}


      g_hash_table_insert (tuple->handle_hash, file_info.name, monitor_data);
      gnome_vfs_uri_unref (theme_dir_uri);
    }

  if (result != GNOME_VFS_ERROR_EOF)
    return result;

  return GNOME_VFS_OK;
}

static GnomeVFSResult
add_top_theme_dir_monitor (GnomeVFSURI  *uri,
			   gboolean     *monitor_not_added,
			   gint          priority,
			   GError      **error)
{
  return real_add_top_theme_dir_monitor (uri, monitor_not_added, priority, FALSE, error);
}

static GnomeVFSResult
add_top_icon_theme_dir_monitor (GnomeVFSURI  *uri,
				gboolean     *monitor_not_added,
				gint          priority,
				GError      **error)
{
  return real_add_top_theme_dir_monitor (uri, monitor_not_added, priority, TRUE, error);
}

/* Public functions */


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
  return get_data_from_hash_by_name (theme_hash_by_name, theme_name, -1);
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
  GList *list;
  GnomeThemeInfo *theme_info;
  struct GnomeThemeInfoHashData *hash_data = user_data;
  guint elements = GPOINTER_TO_INT (hash_data->user_data);
  gboolean add_theme = FALSE;

  list = value;
  theme_info = list->data;
  
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

GList *
gnome_theme_info_find_by_type (guint elements)
{
  struct GnomeThemeInfoHashData data;
  data.user_data = GINT_TO_POINTER (elements);
  data.list = NULL;

  g_hash_table_foreach (theme_hash_by_name,
			gnome_theme_info_find_by_type_helper,
			&data);

  return data.list;
}

GnomeThemeInfo *
gnome_theme_info_find_by_uri (const gchar *theme_uri)
{
  g_return_val_if_fail (theme_uri != NULL, NULL);

  return g_hash_table_lookup (theme_hash_by_uri, theme_uri);
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

  return get_data_from_hash_by_name (icon_theme_hash_by_name, icon_theme_name, -1);

}

static void
gnome_theme_icon_info_find_all_helper (gpointer key,
				       gpointer value,
				       gpointer user_data)
{
  GList *list = value;
  struct GnomeThemeInfoHashData *hash_data;

  list = value;
  hash_data = user_data;

  hash_data->list = g_list_prepend (hash_data->list, list->data);
}

GList *
gnome_theme_icon_info_find_all (void)
{
  
  struct GnomeThemeInfoHashData data;
  data.list = NULL;

  g_hash_table_foreach (icon_theme_hash_by_name,
			gnome_theme_icon_info_find_all_helper,
			&data);

  return data.list;
}


gint
gnome_theme_icon_info_compare (GnomeThemeIconInfo *a,
			       GnomeThemeIconInfo *b)
{
  gint cmp = 0;

  cmp = safe_strcmp (a->path, b->path);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->name, b->name);
  return cmp;
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
  g_free (meta_theme_info->readable_name);
  g_free (meta_theme_info->name);
  g_free (meta_theme_info->comment);
  g_free (meta_theme_info->application_font);
  g_free (meta_theme_info->background_image);
  g_free (meta_theme_info->gtk_theme_name);
  g_free (meta_theme_info->icon_theme_name);
  g_free (meta_theme_info->metacity_theme_name);
  
  g_free (meta_theme_info);
}

void
gnome_theme_meta_info_print (GnomeThemeMetaInfo *meta_theme_info)
{
  g_print ("path: %s\n", meta_theme_info->path);
  g_print ("readable_name: %s\n", meta_theme_info->readable_name);
  g_print ("name: %s\n", meta_theme_info->name);
  g_print ("comment: %s\n", meta_theme_info->comment);
  g_print ("icon_file: %s\n", meta_theme_info->icon_file);
  g_print ("gtk_theme_name: %s\n", meta_theme_info->gtk_theme_name);
  g_print ("metacity_theme_name: %s\n", meta_theme_info->metacity_theme_name);
  g_print ("icon_theme_name: %s\n", meta_theme_info->icon_theme_name);
  g_print ("sawfish_theme_name: %s\n", meta_theme_info->sawfish_theme_name);
  g_print ("sound_theme_name: %s\n", meta_theme_info->sound_theme_name);
  g_print ("application_font: %s\n", meta_theme_info->application_font);
  g_print ("background_image: %s\n", meta_theme_info->background_image);
}

GnomeThemeMetaInfo *
gnome_theme_meta_info_find (const char *meta_theme_name)
{
  g_return_val_if_fail (meta_theme_name != NULL, NULL);

  return get_data_from_hash_by_name (meta_theme_hash_by_name, meta_theme_name, -1);
}

GnomeThemeMetaInfo *
gnome_theme_meta_info_find_by_uri (const char *theme_uri)
{
  g_return_val_if_fail (theme_uri != NULL, NULL);

  return g_hash_table_lookup (meta_theme_hash_by_uri, theme_uri);
}

static void
gnome_theme_meta_info_find_all_helper (gpointer key,
				       gpointer value,
				       gpointer user_data)
{
  GList *list = value;
  struct GnomeThemeInfoHashData *hash_data = user_data;

  hash_data->list = g_list_prepend (hash_data->list, list->data);
}

GList *
gnome_theme_meta_info_find_all (void)
{
  
  struct GnomeThemeInfoHashData data;
  data.list = NULL;

  g_hash_table_foreach (meta_theme_hash_by_name,
			gnome_theme_meta_info_find_all_helper,
			&data);

  return data.list;
}

gint
gnome_theme_meta_info_compare (GnomeThemeMetaInfo *a,
			       GnomeThemeMetaInfo *b)
{
  gint cmp = 0;

  cmp = safe_strcmp (a->path, b->path);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->readable_name, b->readable_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->name, b->name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->comment, b->comment);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->icon_file, b->icon_file);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->gtk_theme_name, b->gtk_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->metacity_theme_name, b->metacity_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->icon_theme_name, b->icon_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->sawfish_theme_name, b->sawfish_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->sound_theme_name, b->sound_theme_name);
  if (cmp != 0) return cmp;
  
  cmp = safe_strcmp (a->application_font, b->application_font);
  if (cmp != 0) return cmp;
  
  cmp = safe_strcmp (a->background_image, b->background_image);
  return cmp;
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

void
gnome_theme_init (gboolean *monitor_not_added)
{
  GnomeVFSURI *top_theme_dir_uri;
  gchar *top_theme_dir_string;
  gboolean real_monitor_not_added = FALSE;
  static gboolean initted = FALSE;
  GnomeVFSResult result;
  const gchar *gtk_data_dir;
  if (initted)
    return;

  initting = TRUE;

  meta_theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  meta_theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  icon_theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  icon_theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);


  /* Add all the toplevel theme dirs. */
  /* $datadir/themes */
  top_theme_dir_string = gtk_rc_get_theme_dir ();
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  result = add_top_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 1, NULL);
  g_free (top_theme_dir_string);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* ~/.themes */
  top_theme_dir_string  = g_build_filename (g_get_home_dir (), ".themes", NULL);
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);
  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  result = add_top_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 0, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* The weird /usr/share/icons */
  top_theme_dir_uri = gnome_vfs_uri_new ("/usr/share/icons");
  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  result = add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 2, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* $datadir/icons */
  gtk_data_dir = g_getenv ("GTK_DATA_PREFIX");
  if (gtk_data_dir)
    {
      top_theme_dir_string = g_build_filename (gtk_data_dir, "share", "icons", NULL);
    }
  else
    {
      top_theme_dir_string = g_build_filename (INSTALL_PREFIX, "share", "icons", NULL);
    }
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);

  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  result = add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 1, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* ~/.icons */
  top_theme_dir_string  = g_build_filename (g_get_home_dir (), ".icons", NULL);
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);

  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  result = add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 0, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* done */
  initted = TRUE;
  initting = FALSE;

  if (monitor_not_added)
    *monitor_not_added = real_monitor_not_added;
}

#if 0
int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);
  gnome_vfs_init ();
  gboolean monitor_not_added = FALSE;

  gnome_theme_init (&monitor_not_added);

  gtk_main ();

  return 0;
}
#endif
