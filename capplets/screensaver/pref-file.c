/* -*- mode: c; style: linux -*- */

/* pref-file.c
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

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include <gnome.h>

#include "pref-file.h"
#include "rc-parse.h"
#include "daemon.h"

#define START_BUF_SIZE 1024
#define MIN_FREE_BUF 128

/* From xscreensaver 3.24 driver/prefs.c line 80 ... */

static char *
chase_symlinks (const char *file)
{
# ifdef HAVE_REALPATH
	char buf[2048], *msg;

	if (file) {
		if (realpath (file, buf))
			return g_strdup (buf);

		msg = g_strdup_printf ("realpath: %s", g_strerror (errno));
		g_warning (msg);
	}
# endif /* HAVE_REALPATH */

	return 0;
}

static const char *
init_file_name (void)
{
	static char *file = 0;

	if (!file)
		file = g_concat_dir_and_file (g_get_home_dir (), 
					      ".xscreensaver");

	if (file && *file)
		return file;
	else
		return NULL;
}

static const char *
init_file_tmp_name (void)
{
	static char *file = 0;
	if (!file)
	{
		const char *name = init_file_name();
		const char *suffix = ".tmp";

		char *n2 = chase_symlinks (name);
		if (n2) name = n2;

		if (!name || !*name)
			file = "";
		else
			file = g_strconcat (name, suffix, NULL);

		if (n2) g_free (n2);
	}

	if (file && *file)
		return file;
	else
		return 0;
}

/* From xscreensaver 3.24 driver/prefs.c line 211 ... */

static char *
get_line (FILE *file, int *line_no) 
{
	char *buf, *start;
	int buf_size = START_BUF_SIZE, free_buf = START_BUF_SIZE;
	int len, l;

	buf = start = g_new (char, buf_size);

	(*line_no)++;

	if (!fgets (start, buf_size, file)) {
		g_free (start);
		return NULL;
	}

	len = strlen (start);

	while (buf[len-1] != '\n' || buf[len-2] == '\\') {
		free_buf -= len;
		buf += len;

		if (free_buf < MIN_FREE_BUF) {
			free_buf += buf_size;
			buf_size *= 2;
			l = buf - start;
			start = g_renew (char, start, buf_size);
			buf = start + l;
		}

		if (buf[-2] == '\\') (*line_no)++;

		if (!fgets (buf, free_buf - 1, file))
			return start;

		len = strlen (buf);
	}

	return start;
}

static char *
transform_line (char *line) 
{
	char *line1;
	int i, i1 = 0, len;

	len = strlen (line);
	line1 = g_new (char, len + 1);

	for (i = 0; i <= len; i++) {
		if (line[i] != '\\') {
			line1[i1++] = line[i];
		} else {
			i++;
			switch (line[i]) {
			case 'n':
				line1[i1++] = '\n';
				break;
			case 'r':
				line1[i1++] = '\r';
				break;
			case 't':
				line1[i1++] = '\t';
				break;
			case '\\':
				line1[i1++] = '\\';
				break;
			case '\n':
				break;
			default:
				line1[i1++] = line[i];
				break;
			}
		}
	}

	return line1;
}

/* Adapted from xscreensaver 3.24 driver/prefs.c line 257 ... */

static GTree *
parse_config_file (const char *file_name) 
{
	struct stat st;
	FILE *in;
	char *buf;
	GTree *config_db;
	gint line_no;
	char *line, *key, *value;

	if (!g_file_test (file_name, G_FILE_TEST_ISFILE))
		return NULL;

	in = fopen (file_name, "r");

	if (!in) {
		g_warning ("error reading \"%s\": %s",
			   file_name, g_strerror (errno));
		return NULL;
	}

	if (fstat (fileno(in), &st) != 0) {
		g_warning ("couldn't re-stat \"%s\": %s",
			   file_name, g_strerror (errno));
		return NULL;
	}

	config_db = g_tree_new ((GCompareFunc) strcmp);
	line_no = 0;

	while ((buf = get_line (in, &line_no))) {
		line = transform_line (buf); g_free (buf);
		key = g_strstrip (line);

		if (*key == '#' || *key == '!' || *key == ';' ||
		    *key == '\n' || *key == 0)
			continue;

		value = strchr (key, ':');

		if (!value) {
			g_warning("%s:%d: unparsable line: %s\n",
				  file_name, line_no, key);
			continue;
		} else {
			*value++ = 0;
			value = g_strstrip (value);
		}

		g_tree_insert (config_db, g_strdup (key), g_strdup (value));
	}

	fclose (in);

	return config_db;
}

gboolean
preferences_load_from_file (Preferences *prefs) 
{
	prefs->config_db = parse_config_file (init_file_name ());

	if (!prefs->config_db) 
		return FALSE;
	else
		return TRUE;
}

/* Writing
 */

/* From xscreensaver 3.24 driver/prefs.c line 168 ... */

static const char * const pref_names[] = {
	"timeout",
	"cycle",
	"lock",
	"lockVTs",
	"lockTimeout",
	"passwdTimeout",
	"visualID",
	"installColormap",
	"verbose",
	"timestamp",
	"splashDuration",
	"demoCommand",
	"prefsCommand",
	"helpURL",
	"loadURL",
	"nice",
	"fade",
	"unfade",
	"fadeSeconds",
	"fadeTicks",
	"captureStderr",
	"font",
	"",
	"programs",
	"",
	"pointerPollTime",
	"windowCreationTimeout",
	"initialDelay",
	"sgiSaverExtension",
	"mitSaverExtension",
	"xidleExtension",
	"procInterrupts",
	"overlayStderr",
	0
};

/* From xscreensaver 3.24 driver/prefs.c line 397 ... */

static int
tab_to (FILE *out, int from, int to)
{
	int tab_width = 8;
	int to_mod = (to / tab_width) * tab_width;

	while (from < to_mod) {
		fprintf(out, "\t");
		from = (((from / tab_width) + 1) * tab_width);
	}

	while (from < to) {
		fprintf(out, " ");
		from++;
	}

	return from;
}

/* From xscreensaver 3.24 driver/prefs.c line 453 ... */

static void
write_entry (FILE *out, const char *key, const char *value)
{
	char *v = g_strdup(value ? value : "");
	char *v2 = v;
	char *nl = 0;
	int col;
	gboolean programs_p = (!strcmp(key, "programs"));
	int tab = (programs_p ? 32 : 16);
	gboolean first = TRUE;

	fprintf (out, "%s:", key);
	col = strlen(key) + 1;

	while (1) {
		if (!programs_p)
			v2 = g_strstrip(v2);
		nl = strchr(v2, '\n');
		if (nl)
			*nl = 0;

		if (first && programs_p) {
			col = tab_to (out, col, 77);
			fprintf (out, " \\\n");
			col = 0;
		}

		if (first)
			first = FALSE;
		else {
			col = tab_to (out, col, 75);
			fprintf (out, " \\n\\\n");
			col = 0;
		}

		if (!programs_p)
			col = tab_to (out, col, tab);

		if (programs_p &&
		    string_columns(v2, strlen (v2), col) + col > 75)
		{
			int L = strlen (v2);
			int start = 0;
			int end = start;
			while (start < L) {
				while (v2[end] == ' ' || v2[end] == '\t')
					end++;
				while (v2[end] != ' ' && v2[end] != '\t' &&
				       v2[end] != '\n' && v2[end] != 0)
					end++;
				if (string_columns (v2 + start, 
						    (end - start), col) >= 74)
				{
					col = tab_to (out, col, 75);
					fprintf(out, "   \\\n");
					col = tab_to (out, 0, tab + 2);
					while (v2[start] == ' ' || 
					       v2[start] == '\t')
						start++;
				}

				col = string_columns (v2 + start, 
						      (end - start), col);
				while (start < end)
					fputc(v2[start++], out);
			}
		} else {
			fprintf (out, "%s", v2);
			col += string_columns(v2, strlen (v2), col);
		}

		if (nl)
			v2 = nl + 1;
		else
			break;
	}

	fprintf(out, "\n");
	g_free(v);
}

static int
write_preference_cb (gchar *key, gchar *value, FILE *out) 
{
	write_entry (out, key, value);
	return FALSE;
}

static gchar*
print_aligned_row (const gchar *lbl, const gchar *cmd, gboolean enabled)
{
	int len;
	int ntab, nspc;
	gchar *tab, *spc;
	gchar *buf;

	len = strlen (lbl) + 2; /* Quotes */
	ntab = (30 - len) / 8;
	nspc = (30 - len) % 8;
	tab = g_strnfill (ntab, '\t');
	spc = g_strnfill (nspc, ' ');
	buf = g_strdup_printf ("%s%s%s\"%s\"  %s\n",
			       (enabled) ? "" : "-",
			       tab, spc,
			       lbl, cmd);
	g_free (tab);
	g_free (spc);

	return buf;
}

static gchar*
print_list_to_str (Preferences *prefs)
{
	gchar *lbl, *cmd, *buf;
	gchar *ret;
	Screensaver *saver;
	GList *l, *fake;
	GString *s;
	gboolean enabled;

	s = g_string_new ("");

	for (l = prefs->screensavers; l != NULL; l = l->next)
	{
		saver = l->data;

		if (saver->command_line)
			cmd = saver->command_line;
		else if (saver->compat_command_line)
			cmd = saver->compat_command_line;
		else
			continue;

		if (prefs->selection_mode == SM_CHOOSE_RANDOMLY)
		{
			/* I wouldn't want these running unless I'd
			 * explicitly enabled them */
			if (!strcmp (saver->name, "webcollage")
			 || !strcmp (saver->name, "vidwhacker"))
				enabled = FALSE;
			else
				enabled = TRUE;
		}
		else 
			enabled = saver->enabled;
		if (saver->label)
			lbl = saver->label;
		else
			lbl = saver->name;

		buf = print_aligned_row (lbl, cmd, enabled);
		g_string_append (s, buf);
		g_free (buf);

		for (fake = saver->fakes; fake != NULL; fake = fake->next)
		{
			buf = print_aligned_row (fake->data, cmd, FALSE);
			g_string_append (s, buf);
			g_free (buf);
		}
	}
	
	for (l = prefs->invalidsavers; l != NULL; l = l->next)
	{	
		saver = l->data;

		if (saver->command_line)
			cmd = saver->command_line;
		else if (saver->compat_command_line)
			cmd = saver->compat_command_line;
		else
			cmd = "/bin/true";

		if (saver->label)
			lbl = saver->label;
		else
			lbl = saver->name;

		buf = print_aligned_row (lbl, cmd, FALSE);
		g_string_append (s, buf);
		g_free (buf);

		for (fake = saver->fakes; fake != NULL; fake = fake->next)
		{
			buf = print_aligned_row (fake->data, cmd, FALSE);
			g_string_append (s, buf);
			g_free (buf);
		}
	}

	ret = s->str;
	g_string_free (s, FALSE);
	
	return ret;
}

/* Adapted from xscreensaver 3.24 driver/prefs.c line 537 ... */

gint 
preferences_save_to_file (Preferences *prefs) 
{
	int status = -1;
	const char *name = init_file_name();
	const char *tmp_name = init_file_tmp_name();
	char *n2 = chase_symlinks (name);
	struct stat st;
	gchar *progs, *oldprogs;

	FILE *out;

	if (!name) goto END;

	if (n2) name = n2;

	if (prefs->verbose)
		g_message ("writing \"%s\".\n", name);

	unlink (tmp_name);
	out = fopen(tmp_name, "w");
	if (!out)
	{
		g_warning ("error writing \"%s\": %s", name, 
			   g_strerror (errno));
		goto END;
	}

	/* Give the new .xscreensaver file the same permissions as the old one;
	   except ensure that it is readable and writable by owner, and not
	   executable.  Extra hack: if we're running as root, make the file
	   be world-readable (so that the daemon, running as "nobody", will
	   still be able to read it.)
	*/
	if (g_file_test (name, G_FILE_TEST_ISFILE)) {
		mode_t mode = st.st_mode;
		mode |= S_IRUSR | S_IWUSR;                /* read/write by user */
		mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);   /* executable by none */

		if (getuid() == (uid_t) 0)                /* read by group/other */
			mode |= S_IRGRP | S_IROTH;

		if (fchmod (fileno(out), mode) != 0)
		{
			g_warning ("error fchmodding \"%s\" to 0%o: %s", 
				   tmp_name, (guint) mode, g_strerror (errno));
			goto END;
		}
	}

	{
		struct passwd *pw = getpwuid (getuid ());
		char *whoami = (pw && pw->pw_name && *pw->pw_name
				? pw->pw_name
				: "<unknown>");
		time_t now = time ((time_t *) 0);
		char *timestr = (char *) ctime (&now);
		char *nl = (char *) strchr (timestr, '\n');
		if (nl) *nl = 0;
		fprintf (out,
			 "# %s Preferences File\n"
			 "# Written by %s %s for %s on %s.\n"
			 "# http://www.jwz.org/xscreensaver/\n"
			 "\n",
			 "XScreenSaver", "xscreensaver", "3.24", whoami, timestr);
	}

	/* Ok this is lame, but it is easier than rewriting stuff */
	progs = print_list_to_str (prefs);
	oldprogs = g_tree_lookup (prefs->config_db, "programs");
	/* Is this the right way of doing it? */
	g_tree_remove (prefs->config_db, "programs");
	if (oldprogs)
		g_free (oldprogs);
	g_tree_insert (prefs->config_db, "programs", progs);

	g_tree_traverse (prefs->config_db, 
			 (GTraverseFunc) write_preference_cb,
			 G_IN_ORDER, out);

	fprintf(out, "\n");

	if (fclose(out) == 0)
	{
		time_t write_date = 0;

		if (stat (tmp_name, &st) == 0)
		{
			write_date = st.st_mtime;
		}
		else
		{
			g_warning ("couldn't stat \"%s\": %s",
				   tmp_name, g_strerror (errno));
			unlink (tmp_name);
			goto END;
		}

		if (rename (tmp_name, name) != 0) {
			g_warning ("error renaming \"%s\" to \"%s\": %s",
				   tmp_name, name, g_strerror (errno));
			unlink (tmp_name);
			goto END;
		} else {
			prefs->init_file_date = write_date;

			/* Since the .xscreensaver file is used for
			 * IPC, let's try and make sure that the bits
			 * actually land on the disk right away. 
			 */
			sync ();

			restart_xscreensaver ();

			status = 0;    /* wrote and renamed successfully! */
		}
	}

	else
	{
		g_warning ("error closing \"%s\": %s", 
			   name, g_strerror (errno));
		unlink (tmp_name);
		goto END;
	}

 END:
	if (n2) g_free (n2);
	return status;
}
