/* -*- mode: c; style: linux -*- */

/* rc-parse.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <gnome.h>

#include "rc-parse.h"
#include "preferences.h"
#include "resources.h"

/* Adapted from xscreensaver 3.24 utils/resources.c line 57 ... */

gboolean 
parse_boolean_resource (char *res)
{
	if (!g_strncasecmp (res, "on", 2) || 
	    !g_strncasecmp (res, "true", 4) || 
	    !g_strncasecmp (res, "yes", 3))
		return TRUE;

	if (!g_strncasecmp (res, "off", 3) || 
	    !g_strncasecmp (res, "false", 5) || 
	    !g_strncasecmp (res, "no", 2))
		return FALSE;

	g_warning ("invalid boolean value: %s", res);
	return FALSE;
}

int 
parse_integer_resource (char *res)
{
	int val;
	char c;

	while (*res && *res <= ' ') res++;		/* skip whitespace */

	if (res[0] == '0' && (res[1] == 'x' || res[1] == 'X')) {
		if (sscanf (res + 2, "%x %c", &val, &c) == 1)
			return val;
	} else {
		if (sscanf (res, "%d %c", &val, &c) == 1)
			return val;
	}

	g_warning ("invalid integer value: %s", res);

	return 0;
}

double
parse_float_resource (char *res)
{
	double val;
	char c;

	if (sscanf (res, " %lf %c", &val, &c) == 1)
		return val;

	g_warning ("invalid float value: %s", res);

	return 0.0;
}

static int
parse_time (const char *string, gboolean seconds_default_p, gboolean silent_p)
{
	unsigned int h, m, s;
	char c;

	if (3 == sscanf (string,   " %u : %2u : %2u %c", &h, &m, &s, &c))
		;
	else if (2 == sscanf (string, " : %2u : %2u %c", &m, &s, &c) ||
		 2 == sscanf (string,    " %u : %2u %c", &m, &s, &c))
		h = 0;
	else if (1 == sscanf (string,       " : %2u %c", &s, &c))
		h = m = 0;
	else if (1 == sscanf (string,          " %u %c",
			      (seconds_default_p ? &s : &m), &c)) 
	{
		h = 0;
		if (seconds_default_p) m = 0;
		else s = 0;
	} else {
		if (! silent_p)
			g_warning ("invalid time interval specification \"%s\".\n",
				   string);
		return -1;
	}
	if (s >= 60 && (h != 0 || m != 0)) {
		if (! silent_p)
			g_warning ("seconds > 59 in \"%s\".\n", string);
		return -1;
	}
	if (m >= 60 && h > 0) {
		if (! silent_p)
			g_warning ("minutes > 59 in \"%s\".\n", string);
		return -1;
	}

	return ((h * 60 * 60) + (m * 60) + s);
}

guint
parse_time_resource (char *res, gboolean sec_p)
{
	int val;

	val = parse_time (res, sec_p, FALSE);

	return (val < 0 ? 0 : val);
}

guint
parse_seconds_resource (char *res)
{
	return parse_time_resource (res, TRUE);
}

gdouble
parse_minutes_resource (char *res)
{
	return (gdouble) parse_time_resource (res, FALSE) / 60.0;
}

/*****************************************************************************/
/*                   Parsing the screensaver resource                        */
/*****************************************************************************/

/* Adapted from xscreensaver 3.24 driver/demo-Gtk.c line 944 ... */

static char *
get_settings_name (const char *command_line) 
{
	char *s = g_strdup (command_line);
	char *s2;

	for (s2 = s; *s2; s2++)       /* truncate at first whitespace */
		if (isspace (*s2)) {
			*s2 = 0;
			break;
		}

	s2 = g_basename (s);

	if (strlen (s2) > 50)     /* Truncate after 50 characters */
		s2[50] = '\0';

	return s2;
}

/* WARNING: Looking at the following code is likely to cause seizures ... */

static gchar *
strip_whitespace (const gchar *line) 
{
	gchar *line2, *s;

	s = line2 = g_new0 (char, strlen (line));

	while (*line) {
		if (!isspace (*line)) 
			*line2++ = *line++;
		else if (*++line && !isspace (*line))
			*line2++ = ' ';
	}
	
	return s;
}

/* Get a list of directories where screensavers could be found */

GList *
get_screensaver_dir_list (void) 
{
	static GList *screensaver_dir_list = NULL;
	char buffer[1024];
	char *xss_name, *strings_name, *grep_name, *command;
	FILE *in;
	GList *list_tail = NULL;

	if (screensaver_dir_list != NULL)
		return screensaver_dir_list;

	xss_name = gnome_is_program_in_path ("xscreensaver");
	strings_name = gnome_is_program_in_path ("strings");
	grep_name = gnome_is_program_in_path ("grep");

	if (!xss_name || !strings_name || !grep_name) {
		/* No grep or strings, so it's hopeless... */
		screensaver_dir_list = 
			g_list_append (NULL, "/usr/X11R6/lib/xscreensaver");
		return screensaver_dir_list;
	}

	command = g_strconcat (strings_name, " ", xss_name, " | ",
			       grep_name, " -G \"^/\"", NULL);
	in = popen (command, "r");

	while (fgets (buffer, 1024, in)) {
		buffer[strlen(buffer) - 1] = '\0';

		if (g_file_test (buffer, G_FILE_TEST_ISDIR)) {
			list_tail = g_list_append (NULL, g_strdup (buffer));
			if (screensaver_dir_list == NULL)
				screensaver_dir_list = list_tail;
			else
				list_tail = list_tail->next;
		}
	}

	return screensaver_dir_list;
}

/* rc_command_exists
 *
 * Given a command line, determines if the command may be executed
 */

gboolean
rc_command_exists (char *command) 
{
	GList *screensaver_dir_list;
	GList *node;
	char *program, *fullpath;
	int i;
	gboolean ret;

	program = g_strdup (command);

	/* Truncate at first whitespace */
	for (i = 0; program[i] && !isspace (program[i]); i++);
	program[i] = '\0';

	/* If this is a complete path, then just stat it */
	if (strchr (program, '/')) {
		if (g_file_test (program, G_FILE_TEST_ISFILE))
			ret = TRUE;
		else
			ret = FALSE;

		g_free (program);

		return ret;
	}

	/* Check the directories where screensavers are installed... */
	screensaver_dir_list = get_screensaver_dir_list ();

	for (node = screensaver_dir_list; node; node = node->next) {
		fullpath = g_concat_dir_and_file ((gchar *) node->data,
						  program);
		if (g_file_test (fullpath, G_FILE_TEST_ISFILE)) {
			g_free (program);
			g_free (fullpath);
			return TRUE;
		}
		g_free (fullpath);
	}

	fullpath = gnome_is_program_in_path (program);
	if (fullpath)
		ret = TRUE;
	else
		ret = FALSE;

	if (fullpath) g_free (fullpath);
	g_free (program);

	return ret;
}

/* Adapted from xscreensaver 3.24 driver/prefs.c line 900 ...
 *
 * Parsing the programs resource.
 */

static Screensaver *
parse_screensaver (const char *line)
{
	Screensaver *h;
	const char *s;

	h = screensaver_new ();
	
	while (isspace(*line)) line++;                /* skip whitespace */
	if (*line == '-') {                           /* handle "-" */
		h->enabled = FALSE;
		line++;
		while (isspace(*line)) line++;            /* skip whitespace */
	}

	s = line;                                     /* handle "visual:" */
	while (*line && *line != ':' && *line != '"' && !isspace(*line))
		line++;

	if (*line != ':')
		line = s;
	else {
		h->visual = g_strndup (s, line - s);
		if (*line == ':') line++;                 /* skip ":" */
		while (isspace(*line)) line++;            /* skip whitespace */
	}

	if (*line == '"') {                           /* handle "name" */
		line++;
		s = line;
		while (*line && *line != '"') line++;
		h->label = g_strndup (s, line - s);
		if (*line == '"') line++;                 /* skip "\"" */
		while (isspace(*line)) line++;      /* skip whitespace */
	}

	h->command_line = strip_whitespace (line);
	if (!rc_command_exists (h->command_line)) {
		screensaver_destroy (h);
		return NULL;
	}

	h->name = get_settings_name (h->command_line);
	if (!h->label) h->label = screensaver_get_label (h->name);

	return h;
}

/* Adapted from xscreensaver 3.24 driver/prefs.c line 1076 ... */

void
parse_screensaver_list (GHashTable *savers_hash, char *list)
{
	int start = 0;
	int end = 0;
	int size = 0;
	int count = 0;

	int number_enabled = 0;
	int total = 0;

	GList *list_head = NULL, *list_tail = NULL;
	Screensaver *saver;

	g_return_if_fail (savers_hash != NULL);
	g_return_if_fail (list != NULL);

	size = strlen (list);

	/* Iterate over the lines in `d' (the string with newlines)
	   and make new strings to stuff into the `screenhacks' array.
	*/

	while (start < size) {
		/* skip forward over whitespace. */
		while (list[start] == ' ' || 
		       list[start] == '\t' || 
		       list[start] == '\n')
			start++;

		/* skip forward to newline or end of string. */
		end = start;
		while (list[end] != 0 && list[end] != '\n')
			end++;

		/* null terminate. */
		list[end] = 0;

		saver = parse_screensaver (list + start);
		if (saver) {
			if (saver->enabled)
			{
				Screensaver *real_saver;
				real_saver = g_hash_table_lookup (savers_hash,
								  saver->name);
				if (real_saver)
				{
					real_saver->enabled = TRUE;
					real_saver->command_line =
						g_strdup (saver->command_line);
				}
			}
			screensaver_destroy (saver);
		}

		start = end + 1;
	}
}

gchar *
write_boolean (gboolean value) 
{
	return value ? g_strdup ("True") : g_strdup ("False");
}

gchar *
write_integer (gint value) 
{
	return g_strdup_printf ("%d", value);
}

gchar *
write_float (gfloat value) 
{
	return g_strdup_printf ("%f", value);
}

gchar *
write_time (time_t value) 
{
	return g_strdup_printf ("%u:%02u:%02u", 
				(guint) value / 60 / 60,
				(guint) (value / 60) % 60,
				(guint) value % 60);
}

gchar *
write_seconds (gint value) 
{
	return write_time ((time_t) value);
}

gchar *
write_minutes (gdouble value) 
{
	return write_time ((time_t) (value * 60.0));
}


/* From xscreensaver 3.24 driver/prefs.c line 963 ... */

static char *
format_command (const char *cmd, gboolean wrap_p)
{
	int tab = 30;
	int col = tab;
	char *cmd2 = g_new0 (char, 2 * (strlen (cmd) + 1));
	const char *in = cmd;
	char *out = cmd2;

	while (*in) {
		/* shrink all whitespace to one space, for the benefit
		 * of the "demo" mode display.  We only do this when
		 * we can easily tell that the whitespace is not
		 * significant (no shell metachars).
		 */
		switch (*in) {
		case '\'': case '"': case '`': case '\\':
			/* Metachars are scary.  Copy the rest of the
			 * line unchanged. */
			while (*in)
				*out++ = *in++, col++;
			break;

		case ' ': case '\t':
			/* Squeeze all other whitespace down to one space. */
			while (*in == ' ' || *in == '\t')
				in++;
			*out++ = ' ', col++;
			break;

		default:
			/* Copy other chars unchanged. */
			*out++ = *in++, col++;
			break;
		}
	}

	*out = 0;

	/* Strip trailing whitespace */
	while (out > cmd2 && isspace (out[-1]))
		*(--out) = 0;

	return cmd2;
}

/* From xscreensaver 3.24 driver/prefs.c line 415 ... */

static char *
stab_to (char *out, int from, int to)
{
	int tab_width = 8;
	int to_mod = (to / tab_width) * tab_width;

	while (from < to_mod) {
		*out++ = '\t';
		from = (((from / tab_width) + 1) * tab_width);
	}

	while (from < to) {
		*out++ = ' ';
		from++;
	}

	return out;
}

int
string_columns (const char *string, int length, int start)
{
	int tab_width = 8;
	int col = start;
	const char *end = string + length;

	while (string < end) {
		if (*string == '\n')
			col = 0;
		else if (*string == '\t')
			col = (((col / tab_width) + 1) * tab_width);
		else
			col++;
		string++;
	}

	return col;
}

/* From xscreensaver 3.24 driver/prefs.c line 1009 ... */

static char *
format_hack (Screensaver *saver, gboolean wrap_p)
{
	int tab = 32;
	int size = (2 * (strlen(saver->command_line) +
			 (saver->visual ? strlen(saver->visual) : 0) +
			 (saver->name ? strlen(saver->name) : 0) +
			 tab));
	char *h2 = g_new (char, size);
	char *out = h2;
	char *s;
	int col = 0;

	if (!saver->enabled) *out++ = '-';         /* write disabled flag */

	if (saver->visual && *saver->visual) {         /* write visual name */
		if (saver->enabled) *out++ = ' ';
		*out++ = ' ';
		strcpy (out, saver->visual);
		out += strlen (saver->visual);
		*out++ = ':';
		*out++ = ' ';
	}

	*out = 0;
	col = string_columns (h2, strlen (h2), 0);

	if (saver->label && *saver->label) {          /* write pretty name */
		int L = (strlen (saver->label) + 2);
		if (L + col < tab)
			out = stab_to (out, col, tab - L - 2);
		else
			*out++ = ' ';
		*out++ = '"';
		strcpy (out, saver->label);
		out += strlen (saver->label);
		*out++ = '"';
		*out = 0;

		col = string_columns (h2, strlen (h2), 0);
		if (wrap_p && col >= tab) {
			out = stab_to (out, col, 77);
			*out += strlen(out);
		}
		else
			*out++ = ' ';

		if (out >= h2+size) abort();
	}

	*out = 0;
	col = string_columns (h2, strlen (h2), 0);
	out = stab_to (out, col, tab);                /* indent */

	if (out >= h2+size) abort();
	s = format_command (saver->command_line, wrap_p);
	strcpy (out, s);
	out += strlen (s);
	g_free (s);
	*out = 0;

	return h2;
}

gchar *
write_screensaver_list (GList *screensavers) 
{
	GList *node;
	Screensaver *saver;
	GString *hack_string;
	gchar *str;

	hack_string = g_string_new (NULL);

	for (node = screensavers; node; node = node->next) {
		saver = SCREENSAVER (node->data);
		if (!saver->command_line) continue;
		str = format_hack (saver, TRUE);
		g_string_append (hack_string, str);
		g_string_append (hack_string, "\n");
		g_free (str);
	}

	str = hack_string->str;
	g_string_free (hack_string, FALSE);
	return str;
}
