/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <sys/xattr.h>
#include <errno.h>
#include <gio/gio.h>

#include "cc-util.h"

/* Combining diacritical mark?
 *  Basic range: [0x0300,0x036F]
 *  Supplement:  [0x1DC0,0x1DFF]
 *  For Symbols: [0x20D0,0x20FF]
 *  Half marks:  [0xFE20,0xFE2F]
 */
#define IS_CDM_UCS4(c) (((c) >= 0x0300 && (c) <= 0x036F)  || \
                        ((c) >= 0x1DC0 && (c) <= 0x1DFF)  || \
                        ((c) >= 0x20D0 && (c) <= 0x20FF)  || \
                        ((c) >= 0xFE20 && (c) <= 0xFE2F))

#define IS_SOFT_HYPHEN(c) ((c) == 0x00AD)

/* Copied from tracker/src/libtracker-fts/tracker-parser-glib.c under the GPL
 * And then from gnome-shell/src/shell-util.c
 *
 * Originally written by Aleksander Morgado <aleksander@gnu.org>
 */
char *
cc_util_normalize_casefold_and_unaccent (const char *str)
{
  g_autofree gchar *normalized = NULL;
  gchar *tmp;
  int i = 0, j = 0, ilen;

  if (str == NULL)
    return NULL;

  normalized = g_utf8_normalize (str, -1, G_NORMALIZE_NFKD);
  tmp = g_utf8_casefold (normalized, -1);

  ilen = strlen (tmp);

  while (i < ilen)
    {
      gunichar unichar;
      gchar *next_utf8;
      gint utf8_len;

      /* Get next character of the word as UCS4 */
      unichar = g_utf8_get_char_validated (&tmp[i], -1);

      /* Invalid UTF-8 character or end of original string. */
      if (unichar == (gunichar) -1 ||
          unichar == (gunichar) -2)
        {
          break;
        }

      /* Find next UTF-8 character */
      next_utf8 = g_utf8_next_char (&tmp[i]);
      utf8_len = next_utf8 - &tmp[i];

      if (IS_CDM_UCS4 (unichar) || IS_SOFT_HYPHEN (unichar))
        {
          /* If the given unichar is a combining diacritical mark,
           * just update the original index, not the output one */
          i += utf8_len;
          continue;
        }

      /* If already found a previous combining
       * diacritical mark, indexes are different so
       * need to copy characters. As output and input
       * buffers may overlap, need to use memmove
       * instead of memcpy */
      if (i != j)
        {
          memmove (&tmp[j], &tmp[i], utf8_len);
        }

      /* Update both indexes */
      i += utf8_len;
      j += utf8_len;
    }

  /* Force proper string end */
  tmp[j] = '\0';

  return tmp;
}

char *
cc_util_get_smart_date (GDateTime *date)
{
        g_autoptr(GDateTime) today = NULL;
        g_autoptr(GDateTime) local = NULL;
        GTimeSpan span;

        /* Set today date */
        local = g_date_time_new_now_local ();
        today = g_date_time_new_local (g_date_time_get_year (local),
                                       g_date_time_get_month (local),
                                       g_date_time_get_day_of_month (local),
                                       0, 0, 0);

        span = g_date_time_difference (today, date);
        if (span <= 0)
          {
            return g_strdup (_("Today"));
          }
        else if (span <= G_TIME_SPAN_DAY)
          {
            return g_strdup (_("Yesterday"));
          }
        else
          {
            if (g_date_time_get_year (date) == g_date_time_get_year (today))
              {
                /* Translators: This is a date format string in the style of "Feb 24". */
                return g_date_time_format (date, _("%b %e"));
              }
            else
              {
                /* Translators: This is a date format string in the style of "Feb 24, 2013". */
                return g_date_time_format (date, _("%b %e, %Y"));
              }
          }
}

/* Copied from src/properties/bacon-video-widget-properties.c
 * in totem */
char *
cc_util_time_to_string_text (gint64 msecs)
{
  g_autofree gchar *hours = NULL;
  g_autofree gchar *mins = NULL;
  g_autofree gchar *secs = NULL;
  gint sec, min, hour, _time;

  _time = (int) (msecs / 1000);
  sec = _time % 60;
  _time = _time - sec;
  min = (_time % (60*60)) / 60;
  _time = _time - (min * 60);
  hour = _time / (60*60);

  hours = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d hour", "%d hours", hour), hour);
  mins = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d minute", "%d minutes", min), min);
  secs = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d second", "%d seconds", sec), sec);

  if (hour > 0)
    {
      /* 5 hours 2 minutes 12 seconds */
      return g_strdup_printf (C_("time", "%s %s %s"), hours, mins, secs);
    }
  else if (min > 0)
    {
      /* 2 minutes 12 seconds */
      return g_strdup_printf (C_("time", "%s %s"), mins, secs);
    }
  else if (sec > 0)
    {
      /* 10 seconds */
      return g_strdup (secs);
    }
  else
    {
      /* 0 seconds */
      return g_strdup (_("0 seconds"));
    }
}

// Endless specific utility functions

#define EOS_IMAGE_VERSION_XATTR "user.eos-image-version"

static gchar *
get_image_version (const gchar *path,
                   GError     **error)
{
  ssize_t attrsize;
  g_autofree gchar *value = NULL;

  g_return_val_if_fail (path != NULL, NULL);

  attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, NULL, 0);
  if (attrsize >= 0)
    {
      value = g_malloc (attrsize + 1);
      value[attrsize] = 0;

      attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, value,
                           attrsize);
    }

  if (attrsize >= 0)
    {
      return g_steal_pointer (&value);
    }
  else
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Error examining " EOS_IMAGE_VERSION_XATTR " on %s: %s",
                   path, g_strerror (errsv));
      return NULL;
    }
}

static char *
get_product_name (void)
{
  g_autoptr(GError) error_sysroot = NULL;
  g_autoptr(GError) error_root = NULL;
  gchar *hyphen_index = NULL;
  char *image_version =
    get_image_version ("/sysroot", &error_sysroot);

  if (image_version == NULL)
    image_version =
      get_image_version ("/", &error_root);

  if (image_version == NULL)
    {
      g_warning ("%s", error_sysroot->message);
      g_warning ("%s", error_root->message);
      return NULL;
    }

  hyphen_index = index (image_version, '-');
  if (hyphen_index == NULL)
    return NULL;

  return g_strndup (image_version, hyphen_index - image_version);
}

static char *
find_terms_document_for_language (const gchar *product_name,
                                  const gchar *language)
{
  const gchar * const * data_dirs;
  gchar *path = NULL;
  gint i;

  data_dirs = g_get_system_data_dirs ();

  for (i = 0; data_dirs[i] != NULL; i++)
    {
      path = g_build_filename (data_dirs[i],
                               "eos-license-service",
                               "terms",
                               product_name,
                               language,
                               "Terms-of-Use.pdf",
                               NULL);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

      g_free (path);
    }

  return NULL;
}

static char *
find_terms_document_for_languages (const gchar *product_name,
                                   const gchar * const *languages)
{
  int i;
  gchar *path = NULL;

  if (product_name == NULL)
    return NULL;

  for (i = 0; languages[i] != NULL; i++)
    {
      path = find_terms_document_for_language (product_name, languages[i]);

      if (path != NULL)
        return path;
    }

  return NULL;
}

gboolean
cc_util_show_endless_terms_of_use (GtkWidget *widget)
{
  g_autofree gchar *path = NULL;
  const gchar * const * languages;
  g_autofree gchar *pdf_uri = NULL;
  g_autofree gchar *product_name = NULL;
  g_autoptr(GError) error = NULL;

  languages = g_get_language_names ();
  product_name = get_product_name ();

  path = find_terms_document_for_languages (product_name, languages);
  if (path == NULL)
    path = find_terms_document_for_languages ("eos", languages);

  if (path == NULL)
    {
      g_warning ("Unable to find terms and conditions PDF on the system");
      return TRUE;
    }

  pdf_uri = g_filename_to_uri (path, NULL, &error);

  if (error)
    {
      g_warning ("Unable to construct terms and conditions PDF uri: %s", error->message);
      return TRUE;
    }

  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  gtk_show_uri_on_window (GTK_WINDOW (toplevel), pdf_uri,
                          gtk_get_current_event_time (), &error);

  if (error)
    g_warning ("Unable to display terms and conditions PDF: %s", error->message);

  return TRUE;
}
