#include <config.h>
#include <gdk/gdkx.h>
#include <gconf/gconf.h>
#include <libgnome/gnome-i18n.h>
#include "gnome-settings-daemon.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>


static void
load_cursor (GConfClient *client)
{
  DIR *dir;
  gchar *font_dir_name;
  gchar *dir_name;
  struct dirent *file_dirent;
  gchar *cursor_font;
  gchar **font_path;
  gchar **new_font_path;
  gint n_fonts;
  gint new_n_fonts;
  gint i;
  gchar *mkfontdir_cmd;

  /* setting up the dir */
  font_dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome", NULL);
  if (! g_file_test (font_dir_name, G_FILE_TEST_EXISTS))
    mkdir (font_dir_name, 0755);
  g_free (font_dir_name);
  
  font_dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome/share", NULL);
  if (! g_file_test (font_dir_name, G_FILE_TEST_EXISTS))
    mkdir (font_dir_name, 0755);
  g_free (font_dir_name);

  font_dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome/share/fonts", NULL);
  if (! g_file_test (font_dir_name, G_FILE_TEST_EXISTS))
    mkdir (font_dir_name, 0755);

  if (! g_file_test (font_dir_name, G_FILE_TEST_IS_DIR))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL,
				       0,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "Cannot create the directory \"%s\".\n"\
				       "This is needed to allow changing cursors.",
				       font_dir_name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (font_dir_name);

      return;
    }

  dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome/share/cursor-fonts", NULL);
  if (! g_file_test (dir_name, G_FILE_TEST_EXISTS))
    mkdir (dir_name, 0755);

  if (! g_file_test (dir_name, G_FILE_TEST_IS_DIR))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL,
				       0,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       (_("Cannot create the directory \"%s\".\n"\
				       "This is needed to allow changing cursors.")),
				       dir_name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (dir_name);

      return;
    }

  dir = opendir (dir_name);
  
  while ((file_dirent = readdir (dir)) != NULL)
    {
      struct stat st;
      gchar *link_name;

      link_name = g_build_filename (dir_name, file_dirent->d_name, NULL);
      if (lstat (link_name, &st))
	{
	  g_free (link_name);
	  continue;
	}
	  
      if (S_ISLNK (st.st_mode))
	unlink (link_name);
    }

  closedir (dir);

  cursor_font = gconf_client_get_string (client,
					 "/desktop/gnome/peripherals/mouse/cursor_font",
					 NULL);

  if ((cursor_font != NULL) &&
      (g_file_test (cursor_font, G_FILE_TEST_IS_REGULAR)) &&
      (g_path_is_absolute (cursor_font)))
    {
      gchar *newpath;
      gchar *font_name;

      font_name = strrchr (cursor_font, G_DIR_SEPARATOR);
      newpath = g_build_filename (dir_name, font_name, NULL);
      symlink (cursor_font, newpath);
      g_free (newpath);
    }
  g_free (cursor_font);


  /* run mkfontdir */
  mkfontdir_cmd = g_strdup_printf ("mkfontdir %s %s", dir_name, font_dir_name);
  g_spawn_command_line_async (mkfontdir_cmd, NULL);
  g_free (mkfontdir_cmd);

  /* Set the font path */
  font_path = XGetFontPath (gdk_x11_get_default_xdisplay (), &n_fonts);
  new_n_fonts = n_fonts;
  if (n_fonts == 0 || strcmp (font_path[0], dir_name))
    new_n_fonts++;
  if (n_fonts == 0 || strcmp (font_path[n_fonts-1], font_dir_name))
    new_n_fonts++;

  new_font_path = g_new0 (gchar*, new_n_fonts);
  if (n_fonts == 0 || strcmp (font_path[0], dir_name))
    {
      new_font_path[0] = dir_name;
      for (i = 0; i < n_fonts; i++)
	new_font_path [i+1] = font_path [i];
    }
  else
    {
      for (i = 0; i < n_fonts; i++)
	new_font_path [i] = font_path [i];
    }

  if (n_fonts == 0 || strcmp (font_path[n_fonts-1], font_dir_name))
    {
      new_font_path[new_n_fonts-1] = font_dir_name;
    }

  gdk_error_trap_push ();
  XSetFontPath (gdk_display, new_font_path, new_n_fonts);
  gdk_flush ();
  gdk_error_trap_pop ();
 
  XFreeFontPath (font_path);

  g_free (new_font_path);
  g_free (font_dir_name);
  g_free (dir_name);
}

void
gnome_settings_font_init (GConfClient *client)
{
  load_cursor (client);
}

void
gnome_settings_font_load (GConfClient *client)
{

}
