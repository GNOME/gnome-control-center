#include "gnome-theme-info.h"
#include "gnome-theme-save.h"
#include "gnome-theme-manager.h"
#include "capplet-util.h"


static GQuark error_quark;
enum
{
  INVALID_THEME_NAME,
};


static gboolean
save_theme_to_disk (GnomeThemeMetaInfo  *meta_theme_info,
		    const gchar         *theme_name,
		    GError             **error)
{
  if (theme_name == NULL)
    {
      g_set_error (error,
		   error_quark,
		   INVALID_THEME_NAME,
		   "ff");
    }
  return TRUE;
}

static void
save_dialog_response (GtkWidget *save_dialog,
		      gint       response_id,
		      gpointer   data)
{
  GnomeThemeMetaInfo *meta_theme_info;
  GError *error = NULL;

  if (response_id == GTK_RESPONSE_OK)
    {
      GladeXML *dialog;
      GtkWidget *entry;
      const char *theme_name;

      dialog = gnome_theme_manager_get_theme_dialog ();
      entry = WID ("save_dialog_entry");

      theme_name = gtk_entry_get_text (GTK_ENTRY (entry));
      meta_theme_info = (GnomeThemeMetaInfo *) g_object_get_data (G_OBJECT (save_dialog), "meta_theme_info");
      if (! save_theme_to_disk (meta_theme_info, theme_name, &error))
	{
	  goto out;
	}
    }

 out:
  g_clear_error (&error);
  gtk_widget_hide (save_dialog);
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

