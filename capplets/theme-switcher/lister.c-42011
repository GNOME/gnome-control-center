#include "da.h"
#include <sys/types.h>
#include <utime.h>
#define MARK_STRING "# -- THEME AUTO-WRITTEN DO NOT EDIT\n"
static void
print_standard_stuff(FILE *fout, gchar *theme, gchar *font)
{
      gchar *homedir;

      homedir = g_strconcat ("include \"",
			     gnome_util_user_home(),
			     "/.gtkrc.mine\"\n\n", NULL);
      fprintf(fout, MARK_STRING);
      fprintf(fout, "include \"%s\"\n\n", theme);
      if (font)
	      fprintf(fout, "style \"user-font\"\n{\n  font=\"%s\"\n}\nwidget_class \"*\" style \"user-font\"\n\n", font);
      fprintf(fout, homedir);
      g_free (homedir);
      fprintf(fout, MARK_STRING);
}
void
edit_file_to_use(gchar *file, gchar *theme, gchar *font)
{
  FILE *fin, *fout;
  gchar tmp[4096], buf[4096];
  gchar nextline = 0, hastheme = 0;
  
  srand(time(NULL));
  g_snprintf(tmp, sizeof(tmp), "/tmp/gtkrc_%i", rand());
  fout = fopen(tmp, "w");
  if (!fout)
    return;
  fin = fopen(file, "r");
  if (!fin)
    {
      print_standard_stuff (fout, theme, font);
      fclose(fout);
      cp(tmp, file);
      return;
    }
  while (fgets(buf, sizeof(buf), fin))
    { 
      if (!strcmp(MARK_STRING, buf))
	hastheme += 1;
    }
  rewind(fin);
  if (!hastheme)
    {
      print_standard_stuff (fout, theme, font);
      while (fgets(buf, sizeof(buf), fin))
	fprintf(fout, "%s", buf);
    }
  else if (hastheme == 1)
	  /* we keep this in for backwards compatability. */
    {
      nextline = 0;
      while (fgets(buf, sizeof(buf), fin))
	{
	  if (nextline == 1)
	    nextline = 0;
	  else if (!strcmp(MARK_STRING, buf))
	    {
	      print_standard_stuff (fout, theme, font);
	      nextline = 1;
	    }
	  else if (nextline == 0)
	    fprintf(fout, "%s", buf);
	}
    }
  else
    {
      nextline = 0;
      while (fgets(buf, sizeof(buf), fin))
	{
	  if (!strcmp(MARK_STRING, buf))
	    {
	      if (nextline == 0)
		{
		  nextline = 1;
		  print_standard_stuff (fout, theme, font);
		}
	      else
		{
		  nextline = 0;
		}
	    } else if (nextline == 0)
	      fprintf(fout, "%s", buf);
	}
    }
  fclose(fin);
  fclose(fout);
  cp(tmp, file);
  rm(tmp);
}

void 
set_tmp_rc(void)
{
  gchar s[4096], *home;
  
  home = g_get_home_dir ();
  if (!home)
    return;
  g_snprintf(s, sizeof(s), "%s/.gtkrc", home);
  srand(time(NULL));
  g_snprintf(gtkrc_tmp, sizeof(gtkrc_tmp), "/tmp/%i-gtkrc-%i", time(NULL), rand());
  cp(s, gtkrc_tmp);
}

void
use_theme(gchar *theme, gchar *font)
{
  gchar s[4096], *home;
  
  home = g_get_home_dir ();
  if (!home)
    return;
  g_snprintf(s, sizeof(s), "%s/.gtkrc", home);
  edit_file_to_use(s, theme, font);
}

void
test_theme(gchar *theme, gchar *font)
{
  static time_t last_written_time = 0;
  time_t current_time = time (NULL);
  struct utimbuf buf;

  edit_file_to_use(gtkrc_tmp, theme, font);

  if (last_written_time >= current_time)
    {
      current_time = last_written_time + 1;
      buf.actime = current_time;
      buf.modtime = current_time;
      utime (gtkrc_tmp, &buf);
    }

  last_written_time = current_time;
}

void
free_theme_list(ThemeEntry *list, gint number)
{
  gint i;
  
  for(i = 0; i < number; i++)
    {
      g_free(list[i].name);
      g_free(list[i].rc);
      g_free(list[i].readme);
      g_free(list[i].icon);
    }
  g_free(list);
}

ThemeEntry *
list_themes(gchar *dir, gint *number)
{
  gchar **dir_listing = NULL, tmp[4096];
  ThemeEntry *list = NULL;
  gint  i = 0, j = 0, num = 0;
  
  dir_listing = ls(dir, &num);
  for(i = 0; i < num; i++)
    {
      g_snprintf(tmp, sizeof(tmp), "%s/%s/gtk/gtkrc", dir, dir_listing[i]);
      if (isfile(tmp))
	{
	  list = g_realloc(list, sizeof(ThemeEntry) * ++j);
	  list[j - 1].name = g_strdup(dir_listing[i]);
	  list[j - 1].rc = g_strdup(tmp);
	  g_snprintf(tmp, sizeof(tmp), "%s/%s", dir, dir_listing[i]);
	  list[j - 1].dir = g_strdup(tmp);
	  g_snprintf(tmp, sizeof(tmp), "%s/%s/README.html", dir, dir_listing[i]);
	  list[j - 1].readme = g_strdup(tmp);
	  g_snprintf(tmp, sizeof(tmp), "%s/%s/ICON.png", dir, dir_listing[i]);
	  list[j - 1].icon = g_strdup(tmp);
	}
    }
  freestrlist(dir_listing, num);
  *number = j;
  return list;
}

ThemeEntry *
list_system_themes(gint *number)
{
  gchar *theme_dir = NULL;
  ThemeEntry *list  = NULL;
  
  theme_dir = gtk_rc_get_theme_dir();
  list = list_themes(theme_dir, number);
  g_free(theme_dir);
  return list;
}

ThemeEntry *
list_user_themes(gint *number)
{
  gchar *home = NULL;
  gchar *theme_dir = NULL;
  ThemeEntry *list  = NULL;
  
  home = g_get_home_dir ();
  if (!home)
    return NULL;

  if (!isdir(home))
    return NULL;
  
  theme_dir = g_malloc(strlen(home) + strlen("/.themes") + 1);
  sprintf(theme_dir, "%s%s", home, "/.themes");
  list = list_themes(theme_dir, number);
  g_free(theme_dir);
  return list;
}
