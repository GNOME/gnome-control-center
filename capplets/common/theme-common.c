#include <config.h>

#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "theme-common.h"

static GList *
themes_common_list_add_dir (GList      *list,
			    const char *dirname)
{
  DIR *dir;
  struct dirent *de;
  const gchar *suffix = "gtk-2.0";
  const gchar *key_suffix = "gtk-2.0-key";

  g_return_val_if_fail (dirname != NULL, list);

  dir = opendir (dirname);
  if (!dir)
    return list;

  while ((de = readdir (dir)))
    {
      char *tmp;
      ThemeInfo *info = NULL;
		
      if (de->d_name[0] == '.')
	continue;

      tmp = g_build_filename (dirname, de->d_name, suffix, NULL);
      if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
	{
	  info = g_new0 (ThemeInfo, 1);
	  info->path = g_build_filename (dirname, de->d_name, NULL);
	  info->name = g_strdup (de->d_name);

	  info->has_gtk = TRUE;
	}
      g_free (tmp);

      tmp = g_build_filename (dirname, de->d_name, key_suffix, NULL);
      if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
	{
	  if (info == NULL)
	    {
	      info = g_new0 (ThemeInfo, 1);
	      info->path = g_build_filename (dirname, de->d_name, NULL);
	      info->name = g_strdup (de->d_name);
	    }
	  info->has_keybinding = TRUE;
	}
      g_free (tmp);
      if (info)
	list = g_list_prepend (list, info);
    }
  closedir (dir);

  return list;
}

GList *
theme_common_get_list (void)
{
  gchar *dir;
  GList *theme_list = NULL;

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
  theme_list = themes_common_list_add_dir (theme_list, dir);
  g_free (dir);

  dir = gtk_rc_get_theme_dir ();
  theme_list = themes_common_list_add_dir (theme_list, dir);
  g_free (dir);

  return theme_list;
}

void
theme_common_list_free (GList *list)
{
  if (list == NULL)
    return;

  g_list_foreach (list, g_free, NULL);
  g_list_free (list);
}
