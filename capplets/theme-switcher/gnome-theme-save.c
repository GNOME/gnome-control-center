#include "gnome-theme-info.h"
#include "gnome-theme-save.h"
#include "gnome-theme-manager.h"
#include "capplet-util.h"
#include <libgnomevfs/gnome-vfs-ops.h>

#include "gnome-theme-save-data.c"

static GQuark error_quark;
enum
{
  INVALID_THEME_NAME,
};

/* taken from gnome-desktop-item.c */
static char *
escape_string_and_dup (const char *s)
{
  char *return_value, *p;
  const char *q;
  int len = 0;

  if (s == NULL)
    return g_strdup("");
	
  q = s;
  while (*q)
    {
      len++;
      if (strchr ("\n\r\t\\", *q) != NULL)
	len++;
      q++;
    }
  return_value = p = (char *) g_malloc (len + 1);
  do
    {
      switch (*s)
	{
	case '\t':
	  *p++ = '\\';
	  *p++ = 't';
	  break;
	case '\n':
	  *p++ = '\\';
	  *p++ = 'n';
	  break;
	case '\r':
	  *p++ = '\\';
	  *p++ = 'r';
	  break;
	case '\\':
	  *p++ = '\\';
	  *p++ = '\\';
	  break;
	default:
	  *p++ = *s;
	}
    }
  while (*s++);
  return return_value;
}

static gboolean
check_theme_name (const gchar  *theme_name,
		  GError      **error)
{
  if (theme_name == NULL)
    {
      g_set_error (error,
		   error_quark,
		   INVALID_THEME_NAME,
		   _("Theme name must be present"));
      return FALSE;
    }
  return TRUE;
}

static gboolean
setup_directory_structure (const gchar  *theme_name,
			   GError      **error)
{
  gchar *dir;
  GnomeVFSURI *uri;

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);
  g_free (dir);
  
  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name, NULL);
  uri = gnome_vfs_uri_new (dir);
  if (!gnome_vfs_uri_exists (uri))
    gnome_vfs_make_directory_for_uri (uri, 0775);
  gnome_vfs_uri_unref (uri);
  g_free (dir);

  return TRUE;
}

static gboolean
write_theme_to_disk (GnomeThemeMetaInfo  *meta_theme_info,
		     const gchar         *theme_name,
		     const gchar         *theme_description,
		     GError             **error)
{
  gchar *dir;
  GnomeVFSURI *uri;
  GnomeVFSURI *target_uri;
  GnomeVFSHandle *handle = NULL;
  GnomeVFSFileSize bytes_written;
  gchar *str;

  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name, "index.theme~", NULL);
  uri = gnome_vfs_uri_new (dir);
  dir [strlen (dir) - 1] = '\000';
  target_uri = gnome_vfs_uri_new (dir);
  g_free (dir);
  gnome_vfs_create_uri (&handle, uri, GNOME_VFS_OPEN_READ | GNOME_VFS_OPEN_WRITE, FALSE, 0644);

  gnome_vfs_truncate_handle (handle, 0);

  /* start making the theme file */
  str = g_strdup_printf (theme_header, theme_name, theme_description);
  gnome_vfs_write (handle, str, strlen (str), &bytes_written);
  g_free (str);

  str = g_strdup_printf ("GtkTheme=%s\n", meta_theme_info->gtk_theme_name);
  gnome_vfs_write (handle, str, strlen (str), &bytes_written);
  g_free (str);

  str = g_strdup_printf ("MetacityTheme=%s\n", meta_theme_info->metacity_theme_name);
  gnome_vfs_write (handle, str, strlen (str), &bytes_written);
  g_free (str);

  str = g_strdup_printf ("IconTheme=%s\n", meta_theme_info->icon_theme_name);
  gnome_vfs_write (handle, str, strlen (str), &bytes_written);
  g_free (str);

  gnome_vfs_close (handle);

  
  gnome_vfs_move_uri (uri, target_uri, TRUE);
  gnome_vfs_uri_unref (uri);
  gnome_vfs_uri_unref (target_uri);

  return TRUE;
}

static gboolean
save_theme_to_disk (GnomeThemeMetaInfo  *meta_theme_info,
		    const gchar         *theme_name,
		    const gchar         *theme_description,
		    GError             **error)
{
  if (! check_theme_name (theme_name, error))
    return FALSE;

  if (! setup_directory_structure (theme_name, error))
    return FALSE;

  if (! write_theme_to_disk (meta_theme_info, theme_name, theme_description, error))
    return FALSE;
  
  return TRUE;
}

static void
save_dialog_response (GtkWidget *save_dialog,
		      gint       response_id,
		      gpointer   data)
{
  GnomeThemeMetaInfo *meta_theme_info;
  char *theme_description = NULL;
  char *theme_name = NULL;
  GError *error = NULL;

  if (response_id == GTK_RESPONSE_OK)
    {
      GladeXML *dialog;
      GtkWidget *entry;
      GtkWidget *text_view;
      GtkTextBuffer *buffer;
      GtkTextIter start_iter;
      GtkTextIter end_iter;
      gchar *buffer_text;
      
      dialog = gnome_theme_manager_get_theme_dialog ();
      entry = WID ("save_dialog_entry");
      theme_name = escape_string_and_dup (gtk_entry_get_text (GTK_ENTRY (entry)));
      
      text_view = WID ("save_dialog_textview");
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
      gtk_text_buffer_get_start_iter (buffer, &start_iter);
      gtk_text_buffer_get_end_iter (buffer, &end_iter);
      buffer_text = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
      theme_description = escape_string_and_dup (buffer_text);
      g_free (buffer_text);
      meta_theme_info = (GnomeThemeMetaInfo *) g_object_get_data (G_OBJECT (save_dialog), "meta-theme-info");
      if (! save_theme_to_disk (meta_theme_info, theme_name, theme_description, &error))
	{
	  goto out;
	}
    }

 out:
  g_clear_error (&error);
  gtk_widget_hide (save_dialog);
  g_free (theme_name);
  g_free (theme_description);
}

static inline gboolean
is_valid_theme_char (char c)
{
  static const gchar *invalid_chars = "/?'\"\\|*.";
  const char *p;

  for (p = invalid_chars; *p != '\000'; p++)
    if (c == *p) return FALSE;
  return TRUE;
}

static void
entry_text_filter (GtkEditable *editable,
		   const gchar *text,
		   gint         length,
		   gint        *position,
		   gpointer     data)
{
  gint i;

  for (i = 0; i < length; i ++)
    {
      if (! is_valid_theme_char (text[i]))
	{
	  g_signal_stop_emission_by_name (editable, "insert_text");
	  return;
	}
    }
}

static void
entry_text_changed (GtkEditable *editable,
		    gpointer     data)
{
  GladeXML *dialog = (GladeXML *) data;
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (editable));
  if (text != NULL && text[0] != '\000')
    gtk_widget_set_sensitive (WID ("save_dialog_save_button"), TRUE);
  else
    gtk_widget_set_sensitive (WID ("save_dialog_save_button"), FALSE);
}


void
gnome_theme_save_show_dialog (GtkWidget          *parent,
			      GnomeThemeMetaInfo *meta_theme_info)
{
  static GtkWidget *save_dialog = NULL;
  GladeXML *dialog;
  GtkWidget *entry;
  GtkWidget *text_view;
  GtkTextBuffer *text_buffer;

  dialog = gnome_theme_manager_get_theme_dialog ();
  entry = WID ("save_dialog_entry");
  text_view = WID ("save_dialog_textview");

  if (save_dialog == NULL)
    {
      save_dialog = WID ("save_dialog");
      g_assert (save_dialog);

      g_signal_connect (G_OBJECT (save_dialog), "response", G_CALLBACK (save_dialog_response), NULL);
      g_signal_connect (G_OBJECT (save_dialog), "delete-event", G_CALLBACK (gtk_true), NULL);
      g_signal_connect (G_OBJECT (entry), "insert_text", G_CALLBACK (entry_text_filter), NULL);
      g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_text_changed), dialog);

      error_quark = g_quark_from_string ("gnome-theme-save");
      gtk_widget_set_size_request (text_view, 300, 100);
    }

  gtk_entry_set_text (GTK_ENTRY (entry), "");
  entry_text_changed (GTK_EDITABLE (entry), dialog);
  gtk_widget_grab_focus (entry);

  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_buffer_set_text (text_buffer, "", strlen (""));
  g_object_set_data (G_OBJECT (save_dialog), "meta-theme-info", meta_theme_info);
  gtk_window_set_transient_for (GTK_WINDOW (save_dialog), GTK_WINDOW (parent));
  gtk_widget_show (save_dialog);
}

