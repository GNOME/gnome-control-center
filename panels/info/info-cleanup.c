/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>
#include "info-cleanup.h"

typedef struct
{
  char *regex;
  char *replacement;
} ReplaceStrings;

static char *
prettify_info (const char *info)
{
  char *pretty;
  int   i;
  static const ReplaceStrings rs[] = {
    { "Mesa DRI ", ""},
    { "Intel[(]R[)]", "Intel<sup>\302\256</sup>"},
    { "Core[(]TM[)]", "Core<sup>\342\204\242</sup>"},
    { "Atom[(]TM[)]", "Atom<sup>\342\204\242</sup>"},
    { "Graphics Controller", "Graphics"},
  };

  if (*info == '\0')
    return NULL;

  pretty = g_markup_escape_text (info, -1);
  pretty = g_strchug (g_strchomp (pretty));

  for (i = 0; i < G_N_ELEMENTS (rs); i++)
    {
      GError *error;
      GRegex *re;
      char   *new;

      error = NULL;

      re = g_regex_new (rs[i].regex, 0, 0, &error);
      if (re == NULL)
        {
          g_warning ("Error building regex: %s", error->message);
          g_error_free (error);
          continue;
        }

      new = g_regex_replace_literal (re,
                                     pretty,
                                     -1,
                                     0,
                                     rs[i].replacement,
                                     0,
                                     &error);

      g_regex_unref (re);

      if (error != NULL)
        {
          g_warning ("Error replacing %s: %s", rs[i].regex, error->message);
          g_error_free (error);
          continue;
        }

      g_free (pretty);
      pretty = new;
    }

  return pretty;
}

static char *
remove_duplicate_whitespace (const char *old)
{
  char   *new;
  GRegex *re;
  GError *error;

  error = NULL;
  re = g_regex_new ("[ \t\n\r]+", G_REGEX_MULTILINE, 0, &error);
  if (re == NULL)
    {
      g_warning ("Error building regex: %s", error->message);
      g_error_free (error);
      return g_strdup (old);
    }
  new = g_regex_replace (re,
                         old,
                         -1,
                         0,
                         " ",
                         0,
                         &error);
  g_regex_unref (re);
  if (new == NULL)
    {
      g_warning ("Error replacing string: %s", error->message);
      g_error_free (error);
      return g_strdup (old);
    }

  return new;
}

char *
info_cleanup (const char *input)
{
  char *pretty, *ret;

  pretty = prettify_info (input);
  ret = remove_duplicate_whitespace (pretty);
  g_free (pretty);

  return ret;
}
