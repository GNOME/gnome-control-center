#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <string.h>
#include <libgnome/gnome-desktop-item.h>
#include "gnome-theme-info.h"
#include "gtkrc-utils.h"

#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#define THEME_NAME "X-GNOME-Metatheme/Name"
#define THEME_COMMENT "X-GNOME-Metatheme/Comment"
#define GTK_THEME_KEY "X-GNOME-Metatheme/GtkTheme"
#define GTK_COLOR_SCHEME_KEY "X-GNOME-Metatheme/GtkColorScheme"
#define METACITY_THEME_KEY "X-GNOME-Metatheme/MetacityTheme"
#define ICON_THEME_KEY "X-GNOME-Metatheme/IconTheme"
#define CURSOR_THEME_KEY "X-GNOME-Metatheme/CursorTheme"
#define CURSOR_SIZE_KEY "X-GNOME-Metatheme/CursorSize"
#define SOUND_THEME_KEY "X-GNOME-Metatheme/SoundTheme"
#define APPLICATION_FONT_KEY "X-GNOME-Metatheme/ApplicationFont"
#define DESKTOP_FONT_KEY "X-GNOME-Metatheme/DesktopFont"
#define MONOSPACE_FONT_KEY "X-GNOME-Metatheme/MonospaceFont"
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
  ThemeChangedCallback func;
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
static GHashTable *cursor_theme_hash_by_uri;
static GHashTable *cursor_theme_hash_by_name;
static GHashTable *theme_hash_by_uri;
static GHashTable *theme_hash_by_name;
static gboolean initting = FALSE;

/* private functions */
static gint
safe_strcmp (const gchar *a_str,
	     const gchar *b_str)
{
  if (a_str && b_str)
    return strcmp (a_str, b_str);
  else
    return a_str - b_str;
}

static void
add_theme_to_hash_by_name (GHashTable *hash_table,
			   gpointer    data)
{
  GnomeThemeCommonInfo *info = data;
  GList *list;

  list = g_hash_table_lookup (hash_table, info->name);
  if (list == NULL)
    {
      list = g_list_append (list, info);
    }
  else
    {
      GList *list_ptr = list;
      gboolean added = FALSE;

      while (list_ptr)
	{
	  gint theme_priority;

	  theme_priority = ((GnomeThemeCommonInfo *) list_ptr->data)->priority;

	  if (theme_priority == info->priority)
	    {
	      /* Swap it in */
	      list_ptr->data = info;
	      added = TRUE;
	      break;
	    }
	  else if (theme_priority > info->priority)
	    {
	      list = g_list_insert_before (list, list_ptr, info);
	      added = TRUE;
	      break;
	    }
	  list_ptr = list_ptr->next;
	}
      if (!added)
	list = g_list_append (list, info);
    }
  g_hash_table_insert (hash_table, g_strdup (info->name), list);
}

static void
remove_theme_from_hash_by_name (GHashTable *hash_table,
				gpointer    data)
{
  GnomeThemeCommonInfo *info = data;
  GList *list;

  list = g_hash_table_lookup (hash_table, info->name);

  list = g_list_remove (list, info);
  if (list == NULL)
    g_hash_table_remove (hash_table, info->name);
  else
    g_hash_table_insert (hash_table, g_strdup (info->name), list);
}

static GnomeThemeCommonInfo *
get_theme_from_hash_by_name (GHashTable  *hash_table,
			     const gchar *name,
			     gint         priority)
{
  GList *list;

  list = g_hash_table_lookup (hash_table, name);

  /* -1 implies return the first one */
  if (priority == -1)
    {
      return list ? list->data : NULL;
    }

  while (list)
    {
      GnomeThemeCommonInfo *info = (GnomeThemeCommonInfo *) list->data;

      if (info->priority == priority)
	return info;

      list = list->next;
    }
  return NULL;
}

static gint
theme_compare (GnomeThemeCommonInfo *a,
	       GnomeThemeCommonInfo *b)
{
  gint cmp;

  g_return_val_if_fail (a->type == b->type, a->type - b->type);

  switch (a->type) {
  case GNOME_THEME_TYPE_METATHEME:
    cmp = gnome_theme_meta_info_compare (
    		(GnomeThemeMetaInfo *) a, (GnomeThemeMetaInfo *) b);
    break;
  case GNOME_THEME_TYPE_ICON:
    cmp = gnome_theme_icon_info_compare (
    		(GnomeThemeIconInfo *) a, (GnomeThemeIconInfo *) b);
    break;
  case GNOME_THEME_TYPE_CURSOR:
    cmp = gnome_theme_cursor_info_compare (
    		(GnomeThemeCursorInfo *) a, (GnomeThemeCursorInfo *) b);
    break;
  default:
    /* not supported at this time */
    g_assert_not_reached ();
  }

  return cmp;
}

static void
theme_free (GnomeThemeCommonInfo *info)
{
  switch (info->type) {
  case GNOME_THEME_TYPE_METATHEME:
    gnome_theme_meta_info_free ((GnomeThemeMetaInfo *) info);
    break;
  case GNOME_THEME_TYPE_ICON:
    gnome_theme_icon_info_free ((GnomeThemeIconInfo *) info);
    break;
  case GNOME_THEME_TYPE_REGULAR:
    gnome_theme_info_free ((GnomeThemeInfo *) info);
    break;
  case GNOME_THEME_TYPE_CURSOR:
    gnome_theme_cursor_info_free ((GnomeThemeCursorInfo *) info);
    break;
  default:
    g_assert_not_reached ();
  }
}

GnomeThemeMetaInfo *
gnome_theme_read_meta_theme (GnomeVFSURI *meta_theme_uri)
{
  GnomeThemeMetaInfo *meta_theme_info;
  GnomeVFSURI *common_theme_dir_uri;
  GnomeDesktopItem *meta_theme_ditem;
  gchar *meta_theme_file;
  const gchar *str;
  gchar *scheme;

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
  str = gnome_desktop_item_get_localestring (meta_theme_ditem, THEME_NAME);

  if (!str)
     {
     str = gnome_desktop_item_get_localestring (meta_theme_ditem, GNOME_DESKTOP_ITEM_NAME);
     if (!str) /* shouldn't reach */
       {
         gnome_theme_meta_info_free (meta_theme_info);
         return NULL;
       }
     }

  meta_theme_info->readable_name = g_strdup (str);

  str = gnome_desktop_item_get_localestring (meta_theme_ditem, THEME_COMMENT);
  if (str == NULL)
    str = gnome_desktop_item_get_localestring (meta_theme_ditem, GNOME_DESKTOP_ITEM_COMMENT);
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

  str = gnome_desktop_item_get_string (meta_theme_ditem, GTK_COLOR_SCHEME_KEY);
  if (str == NULL || !strcmp (str, ""))
    scheme = gtkrc_get_color_scheme_for_theme (meta_theme_info->gtk_theme_name);
  else
    scheme = g_strdup (str);

  if (scheme != NULL)
  {
    meta_theme_info->gtk_color_scheme = scheme;
    for (; *scheme != '\0'; scheme++)
      if (*scheme == ',')
        *scheme = '\n';
  }

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

  str = gnome_desktop_item_get_string (meta_theme_ditem, CURSOR_THEME_KEY);
  if (str != NULL) {
    meta_theme_info->cursor_theme_name = g_strdup (str);

    str = gnome_desktop_item_get_string (meta_theme_ditem, CURSOR_SIZE_KEY);
    if (str)
      meta_theme_info->cursor_size = (int) g_ascii_strtoll (str, NULL, 10);
    else
      meta_theme_info->cursor_size = 18;
  } else {
    meta_theme_info->cursor_theme_name = g_strdup ("default");
    meta_theme_info->cursor_size = 18;
  }

  str = gnome_desktop_item_get_string (meta_theme_ditem, APPLICATION_FONT_KEY);
  if (str != NULL)
    meta_theme_info->application_font = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, DESKTOP_FONT_KEY);
  if (str != NULL)
    meta_theme_info->desktop_font = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, MONOSPACE_FONT_KEY);
  if (str != NULL)
    meta_theme_info->monospace_font = g_strdup (str);

  str = gnome_desktop_item_get_string (meta_theme_ditem, BACKGROUND_IMAGE_KEY);
  if (str != NULL)
    meta_theme_info->background_image = g_strdup (str);

  gnome_desktop_item_unref (meta_theme_ditem);

  return meta_theme_info;
}

static GnomeThemeIconInfo *
read_icon_theme (GnomeVFSURI *icon_theme_uri)
{
  GnomeThemeIconInfo *icon_theme_info;
  GnomeDesktopItem *icon_theme_ditem;
  gchar *icon_theme_file;
  const gchar *name;
  const gchar *directories;
  const gchar *hidden_theme_icon;

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

  /* If index.theme has no Directories entry, it is only a cursor theme */
  directories = gnome_desktop_item_get_string (icon_theme_ditem, "Icon Theme/Directories");
  if (directories == NULL)
    {
      gnome_desktop_item_unref (icon_theme_ditem);
      g_free (icon_theme_file);
      return NULL;
    }

  hidden_theme_icon = gnome_desktop_item_get_string (icon_theme_ditem, "Icon Theme/Hidden");
  if (hidden_theme_icon == NULL ||
      strcmp (hidden_theme_icon, "false") == 0)
    {
      gchar *dir_name;
      icon_theme_info = gnome_theme_icon_info_new ();
      icon_theme_info->readable_name = g_strdup (name);
      icon_theme_info->path = icon_theme_file;
      dir_name = g_path_get_dirname (icon_theme_file);
      icon_theme_info->name = g_path_get_basename (dir_name);
      g_free (dir_name);
    }
  else
    {
      icon_theme_info = NULL;
      g_free (icon_theme_file);
    }

  gnome_desktop_item_unref (icon_theme_ditem);

  return icon_theme_info;
}

#ifdef HAVE_XCURSOR
static void
add_default_cursor_theme ()
{
  GnomeThemeCursorInfo *theme_info;

  theme_info = gnome_theme_cursor_info_new ();
  theme_info->path = g_strdup ("builtin");
  theme_info->name = g_strdup ("default");
  theme_info->readable_name = g_strdup (_("Default Pointer"));
  theme_info->sizes = g_array_sized_new (FALSE, FALSE, sizeof (gint), 0);

  g_hash_table_insert (cursor_theme_hash_by_uri, theme_info->path, theme_info);
  add_theme_to_hash_by_name (cursor_theme_hash_by_name, theme_info);
}

static GdkPixbuf *
gdk_pixbuf_from_xcursor_image (XcursorImage *cursor)
{
  GdkPixbuf *pixbuf;
#define BUF_SIZE sizeof(guint32) * cursor->width * cursor->height
  guchar *buf = g_malloc0 (BUF_SIZE);
  guchar *it;

  for (it = buf; it < (buf + BUF_SIZE); it += 4) {
    // can we get rid of this by using guint32 ?
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    // on little endianess it's BGRA to RGBA
    it[0] = ((guchar *) (cursor->pixels))[it - buf + 2];
    it[1] = ((guchar *) (cursor->pixels))[it - buf + 1];
    it[2] = ((guchar *) (cursor->pixels))[it - buf + 0];
    it[3] = ((guchar *) (cursor->pixels))[it - buf + 3];
#else
    // on big endianess it's ARGB to RGBA
    it[0] = ((guchar *) cursor->pixels)[it - buf + 1];
    it[1] = ((guchar *) cursor->pixels)[it - buf + 2];
    it[2] = ((guchar *) cursor->pixels)[it - buf + 3];
    it[3] = ((guchar *) cursor->pixels)[it - buf + 0];
#endif
  }

  pixbuf = gdk_pixbuf_new_from_data ((const guchar *) buf,
			GDK_COLORSPACE_RGB, TRUE, 8,
			cursor->width, cursor->height,
			cursor->width * 4,
			(GdkPixbufDestroyNotify) g_free,
			NULL);

  if (!pixbuf)
    g_free (buf);

  return pixbuf;
}

static GnomeThemeCursorInfo *
read_cursor_theme (GnomeVFSURI *cursor_theme_uri)
{
  GnomeThemeCursorInfo *cursor_theme_info = NULL;
  GnomeVFSURI *parent_uri, *cursors_uri;
  GnomeVFSFileInfo *file_info;
  GnomeVFSResult result;

  const gint filter_sizes[] = { 12, 16, 24, 32, 36, 40, 48, 64 };
  const gint num_sizes = G_N_ELEMENTS (filter_sizes);

  parent_uri = gnome_vfs_uri_get_parent (cursor_theme_uri);
  cursors_uri = gnome_vfs_uri_append_path (parent_uri, "cursors");

  file_info = gnome_vfs_file_info_new ();
  result = gnome_vfs_get_file_info_uri (cursors_uri, file_info,
					GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK &&
      file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
    GArray *sizes;
    XcursorImage *cursor;
    GdkPixbuf *thumbnail = NULL;
    gchar *name;
    gint i;

    name = gnome_vfs_uri_extract_short_name (parent_uri);

    sizes = g_array_sized_new (FALSE, FALSE, sizeof (gint), num_sizes);

    for (i = 0; i < num_sizes; ++i) {
      cursor = XcursorLibraryLoadImage ("left_ptr", name, filter_sizes[i]);

      if (cursor) {
        if (cursor->size == filter_sizes[i]) {
          g_array_append_val (sizes, filter_sizes[i]);

          if (thumbnail == NULL && i >= 1)
            thumbnail = gdk_pixbuf_from_xcursor_image (cursor);
        }

        XcursorImageDestroy (cursor);
      }
    }

    if (sizes->len == 0) {
      g_array_free (sizes, TRUE);
      g_free (name);
    } else {
      GnomeDesktopItem *cursor_theme_ditem;
      gchar *cursor_theme_file;

      if (!thumbnail) {
        cursor = XcursorLibraryLoadImage ("left_ptr", name,
					  g_array_index (sizes, gint, 0));
        if (cursor) {
          thumbnail = gdk_pixbuf_from_xcursor_image (cursor);
          XcursorImageDestroy (cursor);
        }
      }

      cursor_theme_info = gnome_theme_cursor_info_new ();
      cursor_theme_info->path = gnome_vfs_uri_to_string (parent_uri,
							 GNOME_VFS_URI_HIDE_NONE);
      cursor_theme_info->name = name;
      cursor_theme_info->sizes = sizes;
      cursor_theme_info->thumbnail = thumbnail;

      cursor_theme_file = gnome_vfs_uri_to_string (cursor_theme_uri,
						   GNOME_VFS_URI_HIDE_NONE);
      cursor_theme_ditem = gnome_desktop_item_new_from_file (cursor_theme_file, 0, NULL);
      g_free (cursor_theme_file);

      if (cursor_theme_ditem != NULL) {
        const gchar *readable_name;

        readable_name = gnome_desktop_item_get_string (cursor_theme_ditem,
						       "Icon Theme/Name");
        if (readable_name)
          cursor_theme_info->readable_name = g_strdup (readable_name);
        else
          cursor_theme_info->readable_name = g_strdup (name);

        gnome_desktop_item_unref (cursor_theme_ditem);
      } else {
        cursor_theme_info->readable_name = g_strdup (name);
      }
    }
  }

  gnome_vfs_file_info_unref (file_info);
  gnome_vfs_uri_unref (cursors_uri);
  gnome_vfs_uri_unref (parent_uri);

  return cursor_theme_info;
}

#else /* !HAVE_XCURSOR */

static gchar *
read_current_cursor_font (void)
{
  DIR *dir;
  gchar *dir_name;
  struct dirent *file_dirent;

  dir_name = g_build_filename (g_get_home_dir (), ".gnome2/share/cursor-fonts", NULL);
  if (! g_file_test (dir_name, G_FILE_TEST_EXISTS)) {
    g_free (dir_name);
    return NULL;
  }

  dir = opendir (dir_name);

  while ((file_dirent = readdir (dir)) != NULL) {
    struct stat st;
    gchar *link_name;

    link_name = g_build_filename (dir_name, file_dirent->d_name, NULL);
    if (lstat (link_name, &st)) {
      g_free (link_name);
      continue;
    }

    if (S_ISLNK (st.st_mode)) {
      gint length;
      gchar target[256];

      length = readlink (link_name, target, 255);
      if (length > 0) {
        gchar *retval;
        target[length] = '\0';
        retval = g_strdup (target);
        g_free (link_name);
        closedir (dir);
        return retval;
      }

    }
    g_free (link_name);
  }
  g_free (dir_name);
  closedir (dir);
  return NULL;
}

static void
read_cursor_fonts (void)
{
  gchar *cursor_font;
  gint i;

  const gchar *builtins[][4] = {
    {
      "gnome/cursor-fonts/cursor-normal.pcf",
      N_("Default Pointer"),
      N_("Default Pointer - Current"),
      "mouse-cursor-normal.png"
    }, {
      "gnome/cursor-fonts/cursor-white.pcf",
      N_("White Pointer"),
      N_("White Pointer - Current"),
      "mouse-cursor-white.png"
    }, {
      "gnome/cursor-fonts/cursor-large.pcf",
      N_("Large Pointer"),
      N_("Large Pointer - Current"),
      "mouse-cursor-normal-large.png"
    }, {
      "gnome/cursor-fonts/cursor-large-white.pcf",
      N_("Large White Pointer - Current"),
      N_("Large White Pointer"),
      "mouse-cursor-white-large.png"
    }
  };

  cursor_font = read_current_cursor_font();

  if (!cursor_font)
    cursor_font = g_strdup (builtins[0][0]);

  for (i = 0; i < G_N_ELEMENTS (builtins); i++) {
    GnomeThemeCursorInfo *theme_info;
    gchar *filename;

    theme_info = gnome_theme_cursor_info_new ();

    filename = g_build_filename (GNOMECC_DATA_DIR, "pixmaps", builtins[i][3], NULL);
    theme_info->thumbnail = gdk_pixbuf_new_from_file (filename, NULL);
    g_free (filename);

    theme_info->path = g_build_filename (GNOMECC_DATA_DIR, builtins[i][0], NULL);
    theme_info->name = g_strdup (theme_info->path);

    if (!strcmp (theme_info->path, cursor_font))
      theme_info->readable_name = g_strdup (_(builtins[i][2]));
    else
      theme_info->readable_name = g_strdup (_(builtins[i][1]));

    g_hash_table_insert (cursor_theme_hash_by_uri, theme_info->path, theme_info);
    add_theme_to_hash_by_name (cursor_theme_hash_by_name, theme_info);
  }

  g_free (cursor_font);
}
#endif /* HAVE_XCURSOR */

static void
handle_change_signal (gpointer             data,
		      GnomeThemeChangeType change_type)
{
#ifdef DEBUG
  gchar *type_str = NULL;
  gchar *change_str = NULL;
  gchar *element_str = NULL;
#endif
  GnomeThemeCommonInfo *theme = data;
  GList *list;

  if (initting)
    return;

  for (list = callbacks; list; list = list->next)
  {
    ThemeCallbackData *callback_data = list->data;
    (* callback_data->func) (theme, change_type, callback_data->data);
  }

#ifdef DEBUG
  if (theme->type == GNOME_THEME_TYPE_METATHEME)
    type_str = "meta";
  else if (theme->type == GNOME_THEME_TYPE_ICON)
    type_str = "icon";
  else if (theme->type == GNOME_THEME_TYPE_CURSOR)
    type_str = "cursor";
  else if (theme->type == GNOME_THEME_TYPE_REGULAR) {
    GnomeThemeInfo *rtheme = (GnomeThemeInfo *) theme;
    if (rtheme->has_gtk)
      element_str = "gtk-2";
    else if (rtheme->has_keybinding)
      element_str = "keybinding";
    else if (rtheme->has_metacity)
      element_str = "metacity";
  }

  if (change_type == GNOME_THEME_CHANGE_CREATED)
    change_str = "created";
  else if (change_type == GNOME_THEME_CHANGE_CHANGED)
    change_str = "changed";
  else if (change_type == GNOME_THEME_CHANGE_DELETED)
    change_str = "deleted";

  if (type == GNOME_THEME_TYPE_REGULAR)
    {
      g_print ("theme \"%s\" has a theme of type %s (priority %d) has been %s\n",
	       theme->name,
	       element_str,
	       theme->priority,
	       type_str);
    }
  else if (type_str != NULL)
    {
      g_print ("%s theme \"%s\" (priority %d) has been %s\n",
	       type_str,
	       theme->name,
	       theme->priority,
	       type_str);
    }
#endif
}

/* index_uri should point to the gtkrc file that was modified */
static void
update_theme_index (GnomeVFSURI       *index_uri,
		    GnomeThemeElement  key_element,
		    gint               priority)
{
  GnomeVFSFileInfo *file_info;
  GnomeVFSResult result;
  gboolean theme_exists;
  GnomeThemeInfo *theme_info;
  GnomeVFSURI *parent;
  GnomeVFSURI *common_theme_dir_uri;
  gchar *common_theme_dir;

  /* First, we determine the new state of the file.  We do no more
   * sophisticated a test than "files exists and is a file" */
  file_info = gnome_vfs_file_info_new ();
  result = gnome_vfs_get_file_info_uri (index_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  theme_exists = (result == GNOME_VFS_OK && file_info->type == GNOME_VFS_FILE_TYPE_REGULAR);
  gnome_vfs_file_info_unref (file_info);

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
	  theme_info->readable_name = g_strdup (theme_info->name);
	  theme_info->priority = priority;
 	  if (key_element & GNOME_THEME_GTK_2)
	    theme_info->has_gtk = TRUE;
	  else if (key_element & GNOME_THEME_GTK_2_KEYBINDING)
	    theme_info->has_keybinding = TRUE;
	  else if (key_element & GNOME_THEME_METACITY)
	    theme_info->has_metacity = TRUE;

	  g_hash_table_insert (theme_hash_by_uri, g_strdup (common_theme_dir), theme_info);
	  add_theme_to_hash_by_name (theme_hash_by_name, theme_info);
	  handle_change_signal (theme_info, GNOME_THEME_CHANGE_CREATED);
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
	  remove_theme_from_hash_by_name (theme_hash_by_name, theme_info);
	}

      if (theme_exists && theme_used_to_exist)
	{
	  handle_change_signal (theme_info, GNOME_THEME_CHANGE_CHANGED);
	}
      else if (theme_exists && !theme_used_to_exist)
	{
	  handle_change_signal (theme_info, GNOME_THEME_CHANGE_CREATED);
	}
      else if (!theme_exists && theme_used_to_exist)
	{
	  handle_change_signal (theme_info, GNOME_THEME_CHANGE_DELETED);
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
update_common_theme_dir_index (GnomeVFSURI   *theme_index_uri,
			       GnomeThemeType type,
			       gint           priority)
{
  gboolean theme_exists;
  GnomeThemeCommonInfo *theme_info;
  GnomeThemeCommonInfo *old_theme_info;
  GnomeVFSURI *common_theme_dir_uri;
  gchar *common_theme_dir;
  GHashTable *hash_by_uri;
  GHashTable *hash_by_name;

  if (type == GNOME_THEME_TYPE_ICON) {
    hash_by_uri = icon_theme_hash_by_uri;
    hash_by_name = icon_theme_hash_by_name;
  } else if (type == GNOME_THEME_TYPE_CURSOR) {
    hash_by_uri = cursor_theme_hash_by_uri;
    hash_by_name = cursor_theme_hash_by_name;
  } else {
    hash_by_uri = meta_theme_hash_by_uri;
    hash_by_name = meta_theme_hash_by_name;
  }

  if (type != GNOME_THEME_TYPE_CURSOR) {
    GnomeVFSFileInfo *file_info;
    GnomeVFSResult result;

    /* First, we determine the new state of the file. */
    file_info = gnome_vfs_file_info_new ();
    result = gnome_vfs_get_file_info_uri (theme_index_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
    if (result == GNOME_VFS_OK &&
	file_info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
      /* It's an interesting file. Let's try to load it. */
      if (type == GNOME_THEME_TYPE_ICON)
	theme_info = (GnomeThemeCommonInfo *) read_icon_theme (theme_index_uri);
      else
	theme_info = (GnomeThemeCommonInfo *) gnome_theme_read_meta_theme (theme_index_uri);
    } else {
      theme_info = NULL;
    }

    gnome_vfs_file_info_unref (file_info);
  }
#ifdef HAVE_XCURSOR
  /* cursor themes don't necessarily have an index file, so try those in any case */
  else {
    theme_info = (GnomeThemeCommonInfo *) read_cursor_theme (theme_index_uri);
  }
#endif

  if (theme_info) {
    theme_info->priority = priority;
    theme_exists = TRUE;
  } else {
    theme_exists = FALSE;
  }

  /* Next, we see what currently exists */
  common_theme_dir_uri = gnome_vfs_uri_get_parent (theme_index_uri);
  common_theme_dir = gnome_vfs_uri_to_string (common_theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);

  old_theme_info = (GnomeThemeCommonInfo *) g_hash_table_lookup (hash_by_uri, common_theme_dir);

  if (old_theme_info == NULL)
    {
      if (theme_exists)
	{
	  g_hash_table_insert (hash_by_uri, g_strdup (common_theme_dir), theme_info);
	  add_theme_to_hash_by_name (hash_by_name, theme_info);
	  handle_change_signal (theme_info, GNOME_THEME_CHANGE_CREATED);
	}
    }
  else
    {
      if (theme_exists)
        {
	  if (theme_compare (theme_info, old_theme_info) != 0)
	    {
	      /* Remove old theme */
	      g_hash_table_remove (hash_by_uri, common_theme_dir);
   	      remove_theme_from_hash_by_name (hash_by_name, old_theme_info);
	      g_hash_table_insert (hash_by_uri, g_strdup (common_theme_dir), theme_info);
	      add_theme_to_hash_by_name (hash_by_name, theme_info);
	      handle_change_signal (theme_info, GNOME_THEME_CHANGE_CHANGED);
	      theme_free (old_theme_info);
	    }
	  else
	    {
	      theme_free (theme_info);
	    }
	}
      else
	{
	  g_hash_table_remove (hash_by_uri, common_theme_dir);
   	  remove_theme_from_hash_by_name (hash_by_name, old_theme_info);

	  handle_change_signal (old_theme_info, GNOME_THEME_CHANGE_DELETED);
	  theme_free (old_theme_info);
	}
    }

  g_free (common_theme_dir);
  gnome_vfs_uri_unref (common_theme_dir_uri);
}

static void
update_meta_theme_index (GnomeVFSURI *meta_theme_index_uri,
			 gint         priority)
{
  update_common_theme_dir_index (meta_theme_index_uri, GNOME_THEME_TYPE_METATHEME, priority);
}

static void
update_icon_theme_index (GnomeVFSURI *icon_theme_index_uri,
			 gint         priority)
{
  update_common_theme_dir_index (icon_theme_index_uri, GNOME_THEME_TYPE_ICON, priority);
}

static void
update_cursor_theme_index (GnomeVFSURI *cursor_theme_index_uri,
			   gint         priority)
{
#ifdef HAVE_XCURSOR
  update_common_theme_dir_index (cursor_theme_index_uri, GNOME_THEME_TYPE_CURSOR, priority);
#endif
}

static void
gtk2_dir_changed (GnomeVFSMonitorHandle *handle,
		  const gchar *monitor_uri,
		  const gchar *info_uri,
		  GnomeVFSMonitorEventType event_type,
		  CommonThemeDirMonitorData *monitor_data)
{
  GnomeVFSURI *gtk2_dir_uri;
  gchar *affected_file;

  gtk2_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (gtk2_dir_uri);

  /* The only file we care about is gtkrc */
  if (!strcmp (affected_file, "gtkrc"))
    update_gtk2_index (gtk2_dir_uri, monitor_data->priority);

  g_free (affected_file);
  gnome_vfs_uri_unref (gtk2_dir_uri);
}

static void
keybinding_dir_changed (GnomeVFSMonitorHandle *handle,
			const gchar *monitor_uri,
			const gchar *info_uri,
			GnomeVFSMonitorEventType event_type,
			CommonThemeDirMonitorData *monitor_data)
{
  GnomeVFSURI *keybinding_dir_uri;
  gchar *affected_file;

  keybinding_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (keybinding_dir_uri);

  /* The only file we care about is gtkrc */
  if (!strcmp (affected_file, "gtkrc"))
    {
      update_keybinding_index (keybinding_dir_uri, monitor_data->priority);
    }

  g_free (affected_file);
  gnome_vfs_uri_unref (keybinding_dir_uri);
}

static void
metacity_dir_changed (GnomeVFSMonitorHandle *handle,
		      const gchar *monitor_uri,
		      const gchar *info_uri,
		      GnomeVFSMonitorEventType event_type,
		      CommonThemeDirMonitorData *monitor_data)
{
  GnomeVFSURI *metacity_dir_uri;
  gchar *affected_file;

  metacity_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (metacity_dir_uri);

  /* The only file we care about is metacity-theme-1.xml */
  if (!strcmp (affected_file, "metacity-theme-1.xml"))
    {
      update_metacity_index (metacity_dir_uri, monitor_data->priority);
    }

  g_free (affected_file);
  gnome_vfs_uri_unref (metacity_dir_uri);
}

static void
common_theme_dir_changed (GnomeVFSMonitorHandle *handle,
			  const gchar *monitor_uri,
			  const gchar *info_uri,
			  GnomeVFSMonitorEventType event_type,
			  CommonThemeDirMonitorData *monitor_data)
{
  GnomeVFSURI *meta_theme_dir_uri;
  gchar *affected_file;

  meta_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (meta_theme_dir_uri);

  /* The only file we care about is index.theme */
  if (!strcmp (affected_file, "index.theme"))
    {
      update_meta_theme_index (meta_theme_dir_uri, monitor_data->priority);
    }

  g_free (affected_file);
  gnome_vfs_uri_unref (meta_theme_dir_uri);
}

static void
common_icon_theme_dir_changed (GnomeVFSMonitorHandle *handle,
			       const gchar *monitor_uri,
			       const gchar *info_uri,
			       GnomeVFSMonitorEventType event_type,
			       CommonIconThemeDirMonitorData *monitor_data)
{
  GnomeVFSURI *icon_theme_dir_uri;
  gchar *affected_file;

  icon_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  affected_file = gnome_vfs_uri_extract_short_name (icon_theme_dir_uri);

  /* The only file we care about is index.theme */
  if (!strcmp (affected_file, "index.theme"))
    {
      update_icon_theme_index (icon_theme_dir_uri, monitor_data->priority);
      update_cursor_theme_index (icon_theme_dir_uri, monitor_data->priority);
    }
  /* and the cursors subdir for cursor themes */
  else if (!strcmp (affected_file, "cursors"))
    {
      /* always call update_cursor_theme_index with the index.theme URI */
      GnomeVFSURI *parent_uri, *index_uri;

      parent_uri = gnome_vfs_uri_get_parent (icon_theme_dir_uri);
      index_uri = gnome_vfs_uri_append_path (parent_uri, "index.theme");
      gnome_vfs_uri_unref (parent_uri);

      update_cursor_theme_index (index_uri, monitor_data->priority);

      gnome_vfs_uri_unref (index_uri);
    }

  g_free (affected_file);
  gnome_vfs_uri_unref (icon_theme_dir_uri);
}

/* Add a monitor to a common_theme_dir. */
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
  GnomeVFSFileInfo *file_info;

  index_uri = gnome_vfs_uri_append_file_name (theme_dir_uri, "index.theme");
  update_meta_theme_index (index_uri, monitor_data->priority);
  gnome_vfs_uri_unref (index_uri);

  /* Add the handle for this directory */
  uri_string = gnome_vfs_uri_to_string (theme_dir_uri, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->common_theme_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) common_theme_dir_changed,
				  monitor_data);
  g_free (uri_string);

  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  else if (result != GNOME_VFS_OK)
    return result;

  /* gtk-2 theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "gtk-2.0");
  file_info = gnome_vfs_file_info_new ();
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "gtkrc");
      update_gtk2_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->gtk2_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) gtk2_dir_changed,
				  monitor_data);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  g_free (uri_string);
  gnome_vfs_uri_unref (subdir);

  /* keybinding theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "gtk-2.0-key");
  gnome_vfs_file_info_clear (file_info);
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (result == GNOME_VFS_OK && file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "gtkrc");
      update_keybinding_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->keybinding_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) keybinding_dir_changed,
				  monitor_data);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  g_free (uri_string);
  gnome_vfs_uri_unref (subdir);

  /* metacity theme subdir */
  subdir = gnome_vfs_uri_append_path (theme_dir_uri, "metacity-1");
  gnome_vfs_file_info_clear (file_info);
  result = gnome_vfs_get_file_info_uri (theme_dir_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
      index_uri = gnome_vfs_uri_append_file_name (subdir, "metacity-theme-1.xml");
      update_metacity_index (index_uri, monitor_data->priority);
      gnome_vfs_uri_unref (index_uri);
    }
  uri_string = gnome_vfs_uri_to_string (subdir, GNOME_VFS_URI_HIDE_NONE);
  result = gnome_vfs_monitor_add (& (monitor_data->metacity_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) metacity_dir_changed,
				  monitor_data);
  g_free (uri_string);
  if (result == GNOME_VFS_ERROR_NOT_SUPPORTED)
    real_monitor_not_added = TRUE;
  gnome_vfs_file_info_unref (file_info);
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

  uri_string = gnome_vfs_uri_to_string (theme_dir_uri,
                                        GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);

  /* Add the handle for this directory */
  index_uri = gnome_vfs_uri_append_file_name (theme_dir_uri, "index.theme");
  update_icon_theme_index (index_uri, monitor_data->priority);
  update_cursor_theme_index (index_uri, monitor_data->priority);
  gnome_vfs_uri_unref (index_uri);

  result = gnome_vfs_monitor_add (& (monitor_data->common_icon_theme_dir_handle),
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) common_icon_theme_dir_changed,
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

static void
top_theme_dir_changed (GnomeVFSMonitorHandle *handle,
		       const gchar *monitor_uri,
		       const gchar *info_uri,
		       GnomeVFSMonitorEventType event_type,
		       CallbackTuple *tuple)
{
  GnomeVFSResult result;
  GHashTable *handle_hash;
  CommonThemeDirMonitorData *monitor_data;
  GnomeVFSURI *common_theme_dir_uri;
  gint priority;

  common_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  handle_hash = tuple->handle_hash;
  priority = tuple->priority;

  if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
    {
      GnomeVFSFileInfo *file_info;

      file_info = gnome_vfs_file_info_new ();
      result = gnome_vfs_get_file_info_uri (common_theme_dir_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
      if (result == GNOME_VFS_OK && file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
          monitor_data = g_new0 (CommonThemeDirMonitorData, 1);
          monitor_data->priority = priority;
	  add_common_theme_dir_monitor (common_theme_dir_uri, NULL, monitor_data, NULL);
	  g_hash_table_insert (handle_hash, g_strdup (file_info->name), monitor_data);
	}
      gnome_vfs_file_info_unref (file_info);
    }
  else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
    {
      gchar *name;

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
			    CallbackTuple            *tuple)
{
  GnomeVFSResult result;
  GHashTable *handle_hash;
  CommonIconThemeDirMonitorData *monitor_data;
  GnomeVFSURI *common_icon_theme_dir_uri;
  gint priority;

  common_icon_theme_dir_uri = gnome_vfs_uri_new (info_uri);
  handle_hash = tuple->handle_hash;
  priority = tuple->priority;

  if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
    {
      GnomeVFSFileInfo *file_info;

      file_info = gnome_vfs_file_info_new ();
      result = gnome_vfs_get_file_info_uri (common_icon_theme_dir_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
      if (result == GNOME_VFS_OK && file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
          monitor_data = g_new0 (CommonIconThemeDirMonitorData, 1);
          monitor_data->priority = priority;
	  add_common_icon_theme_dir_monitor (common_icon_theme_dir_uri, NULL, monitor_data, NULL);
	  g_hash_table_insert (handle_hash, g_strdup (file_info->name), monitor_data);
	}
      gnome_vfs_file_info_unref (file_info);
    }
  else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
    {
      gchar *name;

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
  GnomeVFSFileInfo *file_info;
  gchar *uri_string;
  CallbackTuple *tuple;

  /* handle_hash is a hash of common_theme_dir names to their monitor_data.  We
   * use it to remove the monitor handles when a dir is removed.
   */
  tuple = g_new (CallbackTuple, 1);
  tuple->handle_hash = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);
  tuple->priority = priority;

  /* Check the URI */
  file_info = gnome_vfs_file_info_new ();
  gnome_vfs_get_file_info_uri (uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
  if (file_info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
    gnome_vfs_file_info_unref (file_info);
    return GNOME_VFS_ERROR_NOT_A_DIRECTORY;
  }
  gnome_vfs_file_info_unref (file_info);
  /* Monitor the top directory */
  uri_string = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

  result = gnome_vfs_monitor_add (&monitor_handle,
				  uri_string,
				  GNOME_VFS_MONITOR_DIRECTORY,
				  (GnomeVFSMonitorCallback) (icon_theme?top_icon_theme_dir_changed:top_theme_dir_changed),
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

  file_info = gnome_vfs_file_info_new ();
  while (gnome_vfs_directory_read_next (directory_handle, file_info) == GNOME_VFS_OK)
    {
      GnomeVFSURI *theme_dir_uri;
      gpointer monitor;

      if (!(file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY ||
            file_info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) ||
          file_info->name[0] == '.') {
	gnome_vfs_file_info_clear (file_info);
	continue;
      }

      /* Add the directory */
      theme_dir_uri = gnome_vfs_uri_append_path (uri, file_info->name);
      if (icon_theme)
	{
	  CommonIconThemeDirMonitorData *monitor_data;
	  monitor_data = g_new0 (CommonIconThemeDirMonitorData, 1);
	  monitor_data->priority = priority;
	  add_common_icon_theme_dir_monitor (theme_dir_uri, monitor_not_added, monitor_data, error);
	  monitor = monitor_data;
	}
      else
	{
	  CommonThemeDirMonitorData *monitor_data;
	  monitor_data = g_new0 (CommonThemeDirMonitorData, 1);
	  monitor_data->priority = priority;
	  add_common_theme_dir_monitor (theme_dir_uri, monitor_not_added, monitor_data, error);
	  monitor = monitor_data;
	}

      g_hash_table_insert (tuple->handle_hash, g_strdup (file_info->name), monitor);
      gnome_vfs_file_info_clear (file_info);
      gnome_vfs_uri_unref (theme_dir_uri);
    }

  gnome_vfs_file_info_unref (file_info);
  gnome_vfs_directory_close (directory_handle);
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

/* GTK/Metacity/keybinding Themes */
GnomeThemeInfo *
gnome_theme_info_new (void)
{
  GnomeThemeInfo *theme_info;

  theme_info = g_new0 (GnomeThemeInfo, 1);
  theme_info->type = GNOME_THEME_TYPE_REGULAR;

  return theme_info;
}

void
gnome_theme_info_free (GnomeThemeInfo *theme_info)
{
  g_free (theme_info->path);
  g_free (theme_info->name);
  g_free (theme_info->readable_name);
  g_free (theme_info);
}

GnomeThemeInfo *
gnome_theme_info_find (const gchar *theme_name)
{
  return (GnomeThemeInfo *)
	 get_theme_from_hash_by_name (theme_hash_by_name, theme_name, -1);
}

struct GnomeThemeInfoHashData
{
  gconstpointer user_data;
  GList *list;
};

static void
gnome_theme_info_find_by_type_helper (gpointer key,
				      GList *list,
				      struct GnomeThemeInfoHashData *hash_data)
{
  guint elements = GPOINTER_TO_INT (hash_data->user_data);

  do {
    GnomeThemeInfo *theme_info = list->data;

    if ((elements & GNOME_THEME_METACITY && theme_info->has_metacity) ||
        (elements & GNOME_THEME_GTK_2 && theme_info->has_gtk) ||
        (elements & GNOME_THEME_GTK_2_KEYBINDING && theme_info->has_keybinding)) {
      hash_data->list = g_list_prepend (hash_data->list, theme_info);
      return;
    }

    list = list->next;
  } while (list);
}

GList *
gnome_theme_info_find_by_type (guint elements)
{
  struct GnomeThemeInfoHashData data;
  data.user_data = GINT_TO_POINTER (elements);
  data.list = NULL;

  g_hash_table_foreach (theme_hash_by_name,
			(GHFunc) gnome_theme_info_find_by_type_helper,
			&data);

  return data.list;
}

static void
gnome_theme_info_find_all_helper (const gchar *key,
				  GList *list,
				  GList **themes)
{
  *themes = g_list_prepend (*themes, list->data);
}

/* Icon themes */
GnomeThemeIconInfo *
gnome_theme_icon_info_new (void)
{
  GnomeThemeIconInfo *icon_theme_info;

  icon_theme_info = g_new0 (GnomeThemeIconInfo, 1);
  icon_theme_info->type = GNOME_THEME_TYPE_ICON;

  return icon_theme_info;
}

void
gnome_theme_icon_info_free (GnomeThemeIconInfo *icon_theme_info)
{
  g_free (icon_theme_info->name);
  g_free (icon_theme_info->readable_name);
  g_free (icon_theme_info->path);
  g_free (icon_theme_info);
}

GnomeThemeIconInfo *
gnome_theme_icon_info_find (const gchar *icon_theme_name)
{
  g_return_val_if_fail (icon_theme_name != NULL, NULL);

  return (GnomeThemeIconInfo *)
	 get_theme_from_hash_by_name (icon_theme_hash_by_name, icon_theme_name, -1);
}

GList *
gnome_theme_icon_info_find_all (void)
{
  GList *list = NULL;

  g_hash_table_foreach (icon_theme_hash_by_name,
			(GHFunc) gnome_theme_info_find_all_helper,
			&list);

  return list;
}

gint
gnome_theme_icon_info_compare (GnomeThemeIconInfo *a,
			       GnomeThemeIconInfo *b)
{
  gint cmp;

  cmp = safe_strcmp (a->path, b->path);
  if (cmp != 0) return cmp;

  return safe_strcmp (a->name, b->name);
}

/* Cursor themes */
GnomeThemeCursorInfo *
gnome_theme_cursor_info_new (void)
{
  GnomeThemeCursorInfo *theme_info;

  theme_info = g_new0 (GnomeThemeCursorInfo, 1);
  theme_info->type = GNOME_THEME_TYPE_CURSOR;

  return theme_info;
}

void
gnome_theme_cursor_info_free (GnomeThemeCursorInfo *cursor_theme_info)
{
  g_free (cursor_theme_info->name);
  g_free (cursor_theme_info->readable_name);
  g_free (cursor_theme_info->path);
  g_array_free (cursor_theme_info->sizes, TRUE);
  if (cursor_theme_info->thumbnail != NULL)
    g_object_unref (cursor_theme_info->thumbnail);
  g_free (cursor_theme_info);
}

GnomeThemeCursorInfo *
gnome_theme_cursor_info_find (const gchar *cursor_theme_name)
{
  g_return_val_if_fail (cursor_theme_name != NULL, NULL);

  return (GnomeThemeCursorInfo *)
	 get_theme_from_hash_by_name (cursor_theme_hash_by_name, cursor_theme_name, -1);
}

GList *
gnome_theme_cursor_info_find_all (void)
{
  GList *list = NULL;

  g_hash_table_foreach (cursor_theme_hash_by_name,
			(GHFunc) gnome_theme_info_find_all_helper,
			&list);

  return list;
}

gint
gnome_theme_cursor_info_compare (GnomeThemeCursorInfo *a,
				 GnomeThemeCursorInfo *b)
{
  gint cmp;

  cmp = safe_strcmp (a->path, b->path);
  if (cmp != 0) return cmp;

  return safe_strcmp (a->name, b->name);
}

/* Meta themes */
GnomeThemeMetaInfo *
gnome_theme_meta_info_new (void)
{
  GnomeThemeMetaInfo *theme_info;

  theme_info = g_new0 (GnomeThemeMetaInfo, 1);
  theme_info->type = GNOME_THEME_TYPE_METATHEME;

  return theme_info;
}

void
gnome_theme_meta_info_free (GnomeThemeMetaInfo *meta_theme_info)
{
  g_free (meta_theme_info->path);
  g_free (meta_theme_info->readable_name);
  g_free (meta_theme_info->name);
  g_free (meta_theme_info->comment);
  g_free (meta_theme_info->application_font);
  g_free (meta_theme_info->desktop_font);
  g_free (meta_theme_info->monospace_font);
  g_free (meta_theme_info->background_image);
  g_free (meta_theme_info->gtk_theme_name);
  g_free (meta_theme_info->gtk_color_scheme);
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
  g_print ("gtk_color_scheme: %s\n", meta_theme_info->gtk_color_scheme);
  g_print ("metacity_theme_name: %s\n", meta_theme_info->metacity_theme_name);
  g_print ("icon_theme_name: %s\n", meta_theme_info->icon_theme_name);
  g_print ("sound_theme_name: %s\n", meta_theme_info->sound_theme_name);
  g_print ("application_font: %s\n", meta_theme_info->application_font);
  g_print ("desktop_font: %s\n", meta_theme_info->desktop_font);
  g_print ("monospace_font: %s\n", meta_theme_info->monospace_font);
  g_print ("background_image: %s\n", meta_theme_info->background_image);
}

GnomeThemeMetaInfo *
gnome_theme_meta_info_find (const char *meta_theme_name)
{
  g_return_val_if_fail (meta_theme_name != NULL, NULL);

  return (GnomeThemeMetaInfo *)
	 get_theme_from_hash_by_name (meta_theme_hash_by_name, meta_theme_name, -1);
}

GList *
gnome_theme_meta_info_find_all (void)
{
  GList *list = NULL;

  g_hash_table_foreach (meta_theme_hash_by_name,
			(GHFunc) gnome_theme_info_find_all_helper,
			&list);

  return list;
}

gint
gnome_theme_meta_info_compare (GnomeThemeMetaInfo *a,
			       GnomeThemeMetaInfo *b)
{
  gint cmp;

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

  cmp = safe_strcmp (a->gtk_color_scheme, b->gtk_color_scheme);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->metacity_theme_name, b->metacity_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->icon_theme_name, b->icon_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->sound_theme_name, b->sound_theme_name);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->application_font, b->application_font);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->desktop_font, b->desktop_font);
  if (cmp != 0) return cmp;

  cmp = safe_strcmp (a->monospace_font, b->monospace_font);
  if (cmp != 0) return cmp;

  return safe_strcmp (a->background_image, b->background_image);
}

void
gnome_theme_info_register_theme_change (ThemeChangedCallback func,
					gpointer data)
{
  ThemeCallbackData *callback_data;

  g_return_if_fail (func != NULL);

  callback_data = g_new (ThemeCallbackData, 1);
  callback_data->func = func;
  callback_data->data = data;

  callbacks = g_list_prepend (callbacks, callback_data);
}

gboolean
gnome_theme_color_scheme_parse (const gchar *scheme, GdkColor *colors)
{
  gchar **color_scheme_strings, **color_scheme_pair, *current_string;
  gint i;

  if (!scheme || !strcmp (scheme, ""))
    return FALSE;

  /* initialise the array */
  for (i = 0; i < NUM_SYMBOLIC_COLORS; i++)
    colors[i].red = colors[i].green = colors[i].blue = 0;

  /* The color scheme string consists of name:color pairs, separated by
   * newlines, so first we split the string up by new line */

  color_scheme_strings = g_strsplit (scheme, "\n", 0);

  /* loop through the name:color pairs, and save the color if we recognise the name */
  i = 0;
  while ((current_string = color_scheme_strings[i++])) {
    color_scheme_pair = g_strsplit (current_string, ":", 0);

    if (color_scheme_pair[0] != NULL && color_scheme_pair[1] != NULL) {
      g_strstrip (color_scheme_pair[0]);
      g_strstrip (color_scheme_pair[1]);

      if (!strcmp ("fg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_FG]);
      else if (!strcmp ("bg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_BG]);
      else if (!strcmp ("text_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_TEXT]);
      else if (!strcmp ("base_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_BASE]);
      else if (!strcmp ("selected_fg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_SELECTED_FG]);
      else if (!strcmp ("selected_bg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_SELECTED_BG]);
      else if (!strcmp ("tooltip_fg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_TOOLTIP_FG]);
      else if (!strcmp ("tooltip_bg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &colors[COLOR_TOOLTIP_BG]);
    }

    g_strfreev (color_scheme_pair);
  }

  g_strfreev (color_scheme_strings);

  return TRUE;
}

gboolean
gnome_theme_color_scheme_equal (const gchar *s1, const gchar *s2)
{
  GdkColor c1[NUM_SYMBOLIC_COLORS], c2[NUM_SYMBOLIC_COLORS];
  int i;

  if (!gnome_theme_color_scheme_parse (s1, c1) ||
      !gnome_theme_color_scheme_parse (s2, c2))
    return FALSE;

  for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
    if (!gdk_color_equal (&c1[i], &c2[i]))
      return FALSE;
  }

  return TRUE;
}

void
gnome_theme_init (gboolean *monitor_not_added)
{
  GnomeVFSURI *top_theme_dir_uri;
  gchar *top_theme_dir_string;
  gboolean real_monitor_not_added = FALSE;
  static gboolean initted = FALSE;
  const gchar *gtk_data_dir;
  if (initted)
    return;

  initting = TRUE;

  meta_theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  meta_theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  icon_theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  icon_theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  cursor_theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  cursor_theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  theme_hash_by_uri = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  theme_hash_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Add all the toplevel theme dirs. */
  /* $datadir/themes */
  top_theme_dir_string = gtk_rc_get_theme_dir ();
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  add_top_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 1, NULL);
  g_free (top_theme_dir_string);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* ~/.themes */
  top_theme_dir_string  = g_build_filename (g_get_home_dir (), ".themes", NULL);
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);
  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  add_top_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 0, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* The weird /usr/share/icons */
  top_theme_dir_uri = gnome_vfs_uri_new ("/usr/share/icons");
  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 2, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* $datadir/icons */
  gtk_data_dir = g_getenv ("GTK_DATA_PREFIX");
  if (gtk_data_dir)
    top_theme_dir_string = g_build_filename (gtk_data_dir, "share", "icons", NULL);
  else
    top_theme_dir_string = g_build_filename (INSTALL_PREFIX, "share", "icons", NULL);

#ifdef XCURSOR_ICONDIR
  /* if there's a separate xcursors dir, add that as well */
  if (strcmp (XCURSOR_ICONDIR, top_theme_dir_string) &&
      strcmp (XCURSOR_ICONDIR, "/usr/share/icons")) {
    top_theme_dir_uri = gnome_vfs_uri_new (XCURSOR_ICONDIR);
    if (gnome_vfs_uri_exists (top_theme_dir_uri))
      add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 1, NULL);
    gnome_vfs_uri_unref (top_theme_dir_uri);
  }
#endif

  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);

  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 1, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

  /* ~/.icons */
  top_theme_dir_string = g_build_filename (g_get_home_dir (), ".icons", NULL);
  top_theme_dir_uri = gnome_vfs_uri_new (top_theme_dir_string);
  g_free (top_theme_dir_string);

  if (!gnome_vfs_uri_exists (top_theme_dir_uri))
    gnome_vfs_make_directory_for_uri (top_theme_dir_uri, 0775);
  add_top_icon_theme_dir_monitor (top_theme_dir_uri, &real_monitor_not_added, 0, NULL);
  gnome_vfs_uri_unref (top_theme_dir_uri);

#ifdef HAVE_XCURSOR
  /* make sure we have the default theme */
  if (!gnome_theme_cursor_info_find ("default"))
    add_default_cursor_theme ();
#else
  /* If we don't have Xcursor, use the built-in cursor fonts instead */
  read_cursor_fonts ();
#endif

  /* done */
  initted = TRUE;
  initting = FALSE;

  if (monitor_not_added)
    *monitor_not_added = real_monitor_not_added;
}
