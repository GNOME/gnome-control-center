#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <string.h>
#include <libgnome/gnome-desktop-item.h>
#include "gnome-theme-info.h"

int
main (int argc, char *argv[])
{
  gboolean monitor_not_added = FALSE;
  GList *themes, *list;

  gtk_init (&argc, &argv);
  gnome_vfs_init ();
  gnome_theme_init (&monitor_not_added);
  
  themes = gnome_theme_meta_info_find_all ();
  if (themes == NULL)
    {
      g_print ("No meta themes were found.\n");
    }
  else
    {
      g_print ("%d meta themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  GnomeThemeMetaInfo *meta_theme_info;
	  
	  meta_theme_info = list->data;
	  g_print ("\t%s\n", meta_theme_info->readable_name);
	}
    }
  g_list_free (themes);

  themes = gnome_theme_icon_info_find_all ();
  if (themes == NULL)
    {
      g_print ("No icon themes were found.\n");
    }
  else
    {
      g_print ("%d icon themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  GnomeThemeIconInfo *icon_theme_info;
	  
	  icon_theme_info = list->data;
	  g_print ("\t%s\n", icon_theme_info->name);
	}
    }
  g_list_free (themes);

  themes = gnome_theme_info_find_by_type (GNOME_THEME_METACITY);
  if (themes == NULL)
    {
      g_print ("No metacity themes were found.\n");
    }
  else
    {
      g_print ("%d metacity themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  GnomeThemeInfo *theme_info;
	  
	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  themes = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
  if (themes == NULL)
    {
      gchar *str;

      g_print ("No gtk-2 themes were found.  The following directories were tested:\n");
      str = gtk_rc_get_theme_dir ();
      g_print ("\t%s\n", str);
      g_free (str);
      str = g_build_filename (g_get_home_dir (), ".themes", NULL);
      g_print ("\t%s\n", str);
      g_free (str);
    }
  else
    {
      g_print ("%d gtk-2 themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  GnomeThemeInfo *theme_info;
	  
	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  themes = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2_KEYBINDING);
  if (themes == NULL)
    {
      g_print ("No keybinding themes were found.\n");
    }
  else
    {
      g_print ("%d keybinding themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  GnomeThemeInfo *theme_info;
	  
	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  gtk_main ();

  return 0;
}

