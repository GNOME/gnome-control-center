/* -*- mode: c; style: linux -*- */

/* rc-parse.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __RC_PARSE_H
#define __RC_PARSE_H

#include <glib.h>

gboolean  parse_boolean_resource  (char *res);
int       parse_integer_resource  (char *res);
double    parse_float_resource    (char *res);
guint     parse_time_resource     (char *res, gboolean sec);
guint     parse_seconds_resource  (char *res);
gdouble   parse_minutes_resource  (char *res);
void      parse_screensaver_list  (GHashTable *savers_hash, char *list);

gchar    *write_boolean           (gboolean value);
gchar    *write_integer           (gint value);
gchar    *write_float             (gfloat value);
gchar    *write_time              (time_t value);
gchar    *write_seconds           (gint value);
gchar    *write_minutes           (gdouble value);
gchar    *write_screensaver_list  (GList *screensavers);

/* Internal; used by pref-file.c and rc-parse.c only */
int       string_columns          (const char *string, int length, int start);

GList    *get_screensaver_dir_list (void);

gboolean  rc_command_exists       (char *command);

#endif /* __RC_PARSE_H */
