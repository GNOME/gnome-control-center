#define _GNU_SOURCE		/* For getline() */

#include "da.h"
#include <sys/types.h>
#include <utime.h>
#include <errno.h>
#include <config.h>

#ifndef HAVE_GETLINE
/* The interface here is that of GNU libc's getline */
static ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
#define EXPAND_CHUNK 16

  int n_read = 0;
  char *line = *lineptr;

  g_return_val_if_fail (lineptr != NULL, -1);
  g_return_val_if_fail (n != NULL, -1);
  g_return_val_if_fail (stream != NULL, -1);
  g_return_val_if_fail (*lineptr != NULL || *n == 0, -1);
  
#ifdef HAVE_FLOCKFILE
  flockfile (stream);
#endif  
  
  while (1)
    {
      int c;
      
#ifdef HAVE_FLOCKFILE
      c = getc_unlocked (stream);
#else
      c = getc (stream);
#endif      

      if (c == EOF)
        {
          if (n_read > 0)
	    line[n_read] = '\0';
          break;
        }

      if (n_read + 2 >= *n)
        {
	  size_t new_size;

	  if (*n == 0)
	    new_size = 16;
	  else
	    new_size = *n * 2;

	  if (*n >= new_size)    /* Overflowed size_t */
	    line = NULL;
	  else
	    line = *lineptr ? realloc (*lineptr, new_size) : malloc (new_size);

	  if (line)
	    {
	      *lineptr = line;
	      *n = new_size;
	    }
	  else
	    {
	      if (*n > 0)
		{
		  (*lineptr)[*n - 1] = '\0';
		  n_read = *n - 2;
		}
	      break;
	    }
        }

      line[n_read] = c;
      n_read++;

      if (c == '\n')
        {
          line[n_read] = '\0';
          break;
        }
    }

#ifdef HAVE_FLOCKFILE
  funlockfile (stream);
#endif

  return n_read - 1;
}
#endif /* ! HAVE_GETLINE */

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
gboolean
edit_file_to_use (gchar *file, gchar *theme, gchar *font)
{
  FILE *fin = NULL;
  FILE *fout = NULL;
  char *errstring = NULL;
  int out_fd;
  char *lineptr = NULL;
  size_t linecount = 0;

  gchar *template = g_strconcat (file, "-XXXXXX", NULL);
  out_fd = mkstemp (template);

  if (out_fd < 0)
    {
      errstring = g_strdup_printf ("Error creating temporary file for preview: %s",
				   g_strerror (errno));
      g_free (template);
      template = NULL;
      
      goto error;
    }

  fout = fdopen (out_fd, "w");
  if (!fout)
    {
      errstring = g_strdup_printf ("Error creating temporary file for preview: %s",
				   g_strerror (errno));
      goto error;
    }

  fin = fopen(file, "r");
  if (!fin)
    {
      if (errno != ENOENT)
	{
	  errstring = g_strdup_printf ("Error reading %s: %s", file, g_strerror (errno));
	  goto error;
	}
      
      print_standard_stuff (fout, theme, font);
      if (ferror (fout))
	errstring = g_strdup_printf ("Error writing to RC file %s: %s",
				     template, g_strerror (errno));
    }
  else
    {
      int count;
      int marker_count = 0;

      while (!feof (fin))
	{
	  getline (&lineptr, &linecount, fin);
	  if (ferror(fin))
	    {
	      errstring = g_strdup_printf ("Error reading from RC file %s: %s",
					   file, g_strerror (errno));
	      goto error;
	    }
	  
	  if (!strcmp(MARK_STRING, lineptr))
	    marker_count += 1;
	}
      rewind(fin);
      if (!marker_count)
	{
	  print_standard_stuff (fout, theme, font);
	  if (ferror (fout))
	    {
	      errstring = g_strdup_printf ("Error writing to RC file %s: %s",
					   template, g_strerror (errno));
	      goto error;
	    }

	  /* We fall through to the marker_count > 1 case to write the rest of the file
	   */
	}

      if (marker_count == 1)
	/* The auto-written portion consists of the comment line and
	 * the line after.
	 * we keep this in for backwards compatability.
	 */
	{
	  gboolean hide_nextline = FALSE;
	  
	  while (!feof (fin) && !errstring)
	    {
	      count = getline (&lineptr, &linecount, fin);
	      if (count < 0)
		{
		  if (ferror(fin))
		    errstring = g_strdup_printf ("Error reading from RC file %s: %s",
						 file, g_strerror (errno));
		}
	      else
		{
		  if (hide_nextline)
		    hide_nextline = FALSE;
		  else if (!strcmp(MARK_STRING, lineptr))
		    {
		      print_standard_stuff (fout, theme, font);
		      hide_nextline = TRUE;
		    }
		  else if (!hide_nextline)
		    fwrite (lineptr, count, 1, fout);

		  if (ferror (fout))
		    errstring = g_strdup_printf ("Error writing to RC file %s: %s",
						 template, g_strerror (errno));
		}
	    }
	}
      else
	/* The comment line consists of the portion between two marker lines
	 */
	{
	  gboolean hide_output = FALSE;
	  
	  while (!feof (fin) && !errstring)
	    {
	      count = getline (&lineptr, &linecount, fin);
	      if (count < 0)
		{
		  if (ferror(fin))
		    errstring = g_strdup_printf ("Error reading from RC file %s: %s",
						 file, g_strerror (errno));
		}
	      else
		{
		  if (!strcmp(MARK_STRING, lineptr))
		    {
		      if (!hide_output)
			print_standard_stuff (fout, theme, font);
		      
		      hide_output = !hide_output;
		    }
		  else if (!hide_output)
		    fwrite (lineptr, count, 1, fout);
		}

	      if (ferror(fout))
		errstring = g_strdup_printf ("Error writing to RC file %s: %s",
					     template, g_strerror (errno));
	    }
	}
    }

 error:
  
  if (fin)
    fclose(fin);

  if (fout)
    fclose(fout);

  if (lineptr)
    free (lineptr);

  if (!errstring)
    {
      /* We've succesfully written the new contents into a temporary file.
       * move that atomically to the file we are editing
       */

      if (rename (template, file) < 0)
	{
	  errstring = g_strdup_printf ("Error moving %s to %s: %s\n",
				       template, file, g_strerror (errno));
	  unlink (template);
	}
    }

  if (errstring)
    {
      if (template)
	unlink (template);
      
      show_error (errstring, FALSE);
      g_free (errstring);
    }

  g_free (template);
  
  return errstring == NULL;
}

static gboolean
copy_fds (int in_fd, int out_fd)
{
#define BUFSIZE 4096  
  char buf[BUFSIZE];

  while (1)
    {
      int count = read (in_fd, buf, BUFSIZE);
      if (count > 0)
	{
	  char *p = buf;
	  int wrote_count;

	  while (count > 0)
	    {
	      wrote_count = write (out_fd, p, count);
	      if (wrote_count < 0)
		{
		  if (errno != EINTR)
		    return FALSE;
		}
	      count -= wrote_count;
	      p += wrote_count;
	    }
	}
      else if (count == 0)
	return TRUE;
      else
	{
	  if (errno != EINTR)
	    return FALSE;
	}
    }
}

char *
set_tmp_rc(void)
{
  gchar *origfile = NULL;
  gchar *home;
  gchar *template = NULL;
  int in_fd, out_fd;
  char *errstring = NULL;
  
  home = g_get_home_dir ();
  if (!home)
    {
      errstring = g_strdup ("Cannot get home directory");
      goto error;
    }

  origfile = g_concat_dir_and_file (home, ".gtkrc");

  template = g_concat_dir_and_file (g_get_tmp_dir (), "gtkrc-XXXXXX");
  out_fd = mkstemp (template);
  if (out_fd < 0)
    {
      errstring = g_strdup_printf ("Error creating temporary file for preview: %s",
				   g_strerror (errno));
      g_free (template);
      template = NULL;
      goto error;
    }
  
  in_fd = open (origfile, O_RDONLY);
  if (in_fd >= 0)
    {
      if (!copy_fds (in_fd, out_fd))
	{
	  errstring = g_strdup_printf ("Error copying %s to %s: %s", origfile, template,
				       g_strerror (errno));
	  goto error;
	}
    }
  else
    {
      if (errno != ENOENT)
	{
	  errstring = g_strdup_printf ("Error opening %s: %s", origfile, g_strerror (errno));
	  goto error;
	}
    }
  
  gtkrc_tmp = template;

 error:
  g_free (origfile);
  
  if (errstring)
    {
      if (template)
	{
	  unlink (template);
	  g_free (template);
	}
    }

  return errstring;
}

gboolean
use_theme(gchar *theme, gchar *font)
{
  gchar s[4096], *home;
  
  home = g_get_home_dir ();
  if (!home)
    return FALSE;
  g_snprintf(s, sizeof(s), "%s/.gtkrc", home);
  return edit_file_to_use(s, theme, font);
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
