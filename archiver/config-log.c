/* -*- mode: c; style: linux -*- */

/* config-log.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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
#  include <config.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "config-log.h"
#include "location.h"
#include "util.h"

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_LOCATION
};

typedef struct _ConfigLogEntry ConfigLogEntry;

struct _ConfigLogEntry 
{
	gint        id;
	struct tm  *date;
	gchar      *backend_id;
};

static void config_log_init (ConfigLog *config_log);
static void config_log_class_init (ConfigLogClass *klass);

static void config_log_set_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id);

static void config_log_get_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id);

static void config_log_destroy    (GtkObject *object);

static GList *find_config_log_entry_id      (ConfigLog *config_log,
					     GList *start, 
					     gint id);
static GList *find_config_log_entry_date    (ConfigLog *config_log,
					     GList *start, 
					     struct tm *date);
static GList *find_config_log_entry_backend (ConfigLog *config_log,
					     GList *start, 
					     gchar *backend_id);

static GList *load_next_log_entry  (ConfigLog *config_log, 
				    GList *last);

static gchar *get_line             (FILE *file);
static gboolean parse_line         (char *buffer, 
				    int *id, 
				    struct tm *time, 
				    char **backend_id);
static gboolean time_geq           (struct tm *time1, 
				    struct tm *time2);

static gboolean do_load            (ConfigLog *config_log);
static void do_unload              (ConfigLog *config_log);

static gint get_next_id            (ConfigLog *config_log);
static struct tm *get_current_date (void);
static void write_log              (FILE *output, 
				    ConfigLogEntry *entry);
static void dump_log               (ConfigLog *config_log);

static void config_log_entry_destroy (ConfigLogEntry *entry);

guint
config_log_get_type (void) 
{
	static guint config_log_type;

	if (!config_log_type) {
		GtkTypeInfo config_log_info = {
			"ConfigLog",
			sizeof (ConfigLog),
			sizeof (ConfigLogClass),
			(GtkClassInitFunc) config_log_class_init,
			(GtkObjectInitFunc) config_log_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		config_log_type = 
			gtk_type_unique (gtk_object_get_type (),
					 &config_log_info);
	}

	return config_log_type;
}

static void
config_log_init (ConfigLog *config_log) 
{
	config_log->location = NULL;
}

static void
config_log_class_init (ConfigLogClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = config_log_destroy;
	object_class->set_arg = config_log_set_arg;
	object_class->get_arg = config_log_get_arg;

	gtk_object_add_arg_type ("ConfigLog::location",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_LOCATION);

	parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
config_log_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ConfigLog *config_log;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_LOG (object));
	g_return_if_fail (arg != NULL);

	config_log = CONFIG_LOG (object);

	switch (arg_id) {
	case ARG_LOCATION:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) != NULL);
		g_return_if_fail (IS_LOCATION (GTK_VALUE_POINTER (*arg)));

		config_log->location = GTK_VALUE_POINTER (*arg);
		break;
	default:
		break;
	}
}

static void 
config_log_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ConfigLog *config_log;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_LOG (object));
	g_return_if_fail (arg != NULL);

	config_log = CONFIG_LOG (object);

	switch (arg_id) {
	case ARG_LOCATION:
		GTK_VALUE_POINTER (*arg) = config_log->location;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/* Loads a configuration log. Creates it if it has not been created
 * already 
 */

GtkObject *
config_log_open (Location *location) 
{
	GtkObject *object;

	object = gtk_object_new (config_log_get_type (),
				 "location", location,
				 NULL);

	do_load (CONFIG_LOG (object));

	return object;
}

/* Destroys a configuration log data structure and frees all memory
 * associated with it, dumping the existing log out to disk
 */

static void 
config_log_destroy (GtkObject *object) 
{
	ConfigLog *config_log;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_LOG (object));

	config_log = CONFIG_LOG (object);

	do_unload (config_log);

	GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (config_log));
}

/**
 * config_log_delete:
 * @config_log: 
 * 
 * Permanently destroy a config log, including its log file. Also destory the
 * object 
 **/

void
config_log_delete (ConfigLog *config_log)
{
	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	if (config_log->file != NULL) {
		fclose (config_log->file);
		config_log->file = NULL;
	}

	if (config_log->filename != NULL)
		unlink (config_log->filename);

	gtk_object_destroy (GTK_OBJECT (config_log));
}

/* Return the id number of the most recent data written by the given
 * backend prior to the given date. If date is NULL, it is assumed to
 * be today.
 */

gint 
config_log_get_rollback_id_for_date (ConfigLog *config_log,
				     struct tm *date,
				     gchar *backend_id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	if (config_log->log_data == NULL)
		config_log->log_data = 
			load_next_log_entry (config_log, NULL);

	if (date == NULL)
		node = config_log->log_data;
	else
		node = find_config_log_entry_date (config_log,
						   config_log->log_data,
						   date);

	node = find_config_log_entry_backend (config_log, node, backend_id);

	if (!node)
		return -1;
	else
		return ((ConfigLogEntry *) node->data)->id;
}

/* Given a linked list of backend ids and a date, return an array of
 * ids corresponding to the most recent data written by each of the
 * backends prior to the given date. The array is in the same order as 
 * the backends and should be freed when done. If date is NULL, it is
 * assumed to be today.
 *
 * FIXME: This should really sort the ids by date.
 */

gint *
config_log_get_rollback_ids_for_date (ConfigLog *config_log,
				      struct tm *date,
				      GList *backend_ids) 
{
	GList *start_node, *node;
	gint *id_array, i = 0;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (backend_ids != NULL, NULL);

	if (config_log->log_data == NULL)
		config_log->log_data = 
			load_next_log_entry (config_log, NULL);

	if (date == NULL)
		start_node = config_log->log_data;
	else
		start_node = find_config_log_entry_date (config_log,
							 config_log->log_data,
							 date);

	id_array = g_new (gint, g_list_length (backend_ids));

	for (; backend_ids; backend_ids = backend_ids->next) {
		node = find_config_log_entry_backend (config_log, 
						      start_node, 
						      backend_ids->data);
		if (!node)
			id_array[i] = -1;
		else
			id_array[i] = ((ConfigLogEntry *) node->data)->id;
		i++;
	}

	return id_array;
}

/* Return the rollback id that is the given number of steps back from the
 * current revision, or -1 if there is no such id
 */

gint
config_log_get_rollback_id_by_steps (ConfigLog *config_log,
				     guint steps, gchar *backend_id)
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	node = config_log->log_data;

	if (node == NULL)
		node = load_next_log_entry (config_log, node);

	while (node != NULL && steps-- > 0) {
		node = find_config_log_entry_backend
			(config_log, node, backend_id);

		if (steps > 0) {
			if (node->next == NULL)
				node = load_next_log_entry (config_log, node);
			else
				node = node->next;
		}
	}

	if (node != NULL)
		return ((ConfigLogEntry *) node->data)->id;
	else
		return -1;
}

/* Return the backend that generated the data with the given id */

gchar *
config_log_get_backend_id_for_id (ConfigLog *config_log, gint id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (id >= 0, NULL);

	if (config_log->log_data == NULL)
		config_log->log_data = 
			load_next_log_entry (config_log, NULL);

	node = find_config_log_entry_id (config_log,
					 config_log->log_data, id);

	if (!node) 
		return NULL;
	else
		return ((ConfigLogEntry *) node->data)->backend_id;
}

/* Return the date the data with the given id was written */

struct tm *
config_log_get_date_for_id (ConfigLog *config_log, gint id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (id >= 0, NULL);

	if (config_log->log_data == NULL)
		config_log->log_data = 
			load_next_log_entry (config_log, NULL);

	node = find_config_log_entry_id (config_log,
					 config_log->log_data, id);

	if (!node) 
		return NULL;
	else
		return ((ConfigLogEntry *) node->data)->date;
}

gint 
config_log_write_entry (ConfigLog *config_log, gchar *backend_id) 
{
	ConfigLogEntry *entry;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	entry = g_new0 (ConfigLogEntry, 1);
	entry->id = get_next_id (config_log);
	entry->date = get_current_date ();
	entry->backend_id = g_strdup (backend_id);

	config_log->log_data = g_list_prepend (config_log->log_data, entry);

	return entry->id;
}

/**
 * config_log_iterate:
 * @config_log: 
 * @callback: 
 * @data: 
 * 
 * Iterate through all log entries an invoke the given callback on each one,
 * passing the id, date created, and backend id to it
 **/

void
config_log_iterate (ConfigLog *config_log, ConfigLogIteratorCB callback,
		    gpointer data) 
{
	GList *node;
	ConfigLogEntry *entry;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));
	g_return_if_fail (callback != NULL);

	node = config_log->log_data;
	while (node != NULL) {
		entry = (ConfigLogEntry *) node->data;
		if (callback (config_log, entry->id, entry->backend_id,
			      entry->date, node->data)) break;

		if (node->next == NULL) 
			node = load_next_log_entry (config_log, node);
		else
			node = node->next;
	}
}

/**
 * config_log_reset_filenames:
 * @config_log: 
 * 
 * Rereads the log's location data to determine filenames
 **/

void
config_log_reset_filenames (ConfigLog *config_log) 
{
	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	if (config_log->filename != NULL)
		g_free (config_log->filename);

	config_log->filename = 
		g_concat_dir_and_file (location_get_path
				       (config_log->location),
				       "config.log");
	if (config_log->lock_filename != NULL)
		g_free (config_log->lock_filename);

	config_log->lock_filename = 
		g_concat_dir_and_file (location_get_path 
				       (config_log->location),
				       "config.log.lock");
}

/* Find the config log entry with the id given, starting at the given
 * node. Return a pointer to the node.
 */

static GList *
find_config_log_entry_id (ConfigLog *config_log, GList *start, gint id) 
{
	GList *last;
	ConfigLogEntry *entry;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (id >= 0, NULL);

	if (!start) return NULL;

	while (start != NULL) {
		last = start;
		entry = (ConfigLogEntry *) start->data;
		if (entry->id == id)
			return start;
		else if (entry->id < id)
			return NULL;
		start = start->next;
	}

	while (1) {
		start = load_next_log_entry (config_log, last);
		if (start == NULL) return NULL;
		entry = (ConfigLogEntry *) start->data;
		if (entry->id == id)
			return start;
		else if (entry->id < id)
			return NULL;
	}

	return NULL;
}

/* Find the first config log entry made prior to the given date,
 * starting at the given node. Return a pointer to the node.
 */

static GList *
find_config_log_entry_date (ConfigLog *config_log, GList *start, 
			    struct tm *date) 
{
	GList *last;
	ConfigLogEntry *entry;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (date != NULL, NULL);

	if (!start) return NULL;

	while (start != NULL) {
		last = start;
		entry = (ConfigLogEntry *) start->data;
		if (time_geq (date, entry->date))
			return start;
		start = start->next;
	}

	while (1) {
		start = load_next_log_entry (config_log, last);
		if (start == NULL) return NULL;
		entry = (ConfigLogEntry *) start->data;
		if (time_geq (date, entry->date))
			return start;
	}

	return NULL;
}

/* Find the first config log entry made by the given backend,
 * starting at the given node. Return a pointer to the node.
 */

static GList *
find_config_log_entry_backend (ConfigLog *config_log, GList *start, 
			       gchar *backend_id) 
{
	GList *last;
	ConfigLogEntry *entry;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (backend_id != NULL, NULL);

	if (!start) return NULL;

	while (start != NULL) {
		last = start;
		entry = (ConfigLogEntry *) start->data;
		if (!strcmp (entry->backend_id, backend_id))
			return start;
		start = start->next;
	}

	while (1) {
		start = load_next_log_entry (config_log, last);
		if (start == NULL) return NULL;
		entry = (ConfigLogEntry *) start->data;
		if (!strcmp (entry->backend_id, backend_id))
			return start;
	}

	return NULL;
}

static GList *
load_next_log_entry (ConfigLog *config_log, GList *last) 
{
	gchar *buffer, *backend_id;
	ConfigLogEntry *entry;
	gboolean success;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);

	if (!config_log->file || feof (config_log->file)) return NULL;

	buffer = get_line (config_log->file);

	entry = g_new0 (ConfigLogEntry, 1);
	entry->date = g_new0 (struct tm, 1);
	success = parse_line (buffer, &entry->id, 
			      entry->date, &backend_id);

	if (success) {
		entry->backend_id = g_strdup (backend_id);
		last = g_list_append (last, entry);

		if (!config_log->log_data) {
			config_log->log_data = last;
			config_log->first_old = last;
		}

		return g_list_find (last, entry);
	} else {
		g_free (entry);
		return NULL;
	}
}

/* Read an entire line from the given file, returning a pointer to an
 * allocated string. Strip the trailing newline from the line.
 */

static gchar *
get_line (FILE *file) 
{
	int buf_size = 0;
	char *buf = NULL, *tmp = NULL;
	size_t distance = 0, amt_read = 0;

	g_return_val_if_fail (file != NULL, NULL);

	while (amt_read == buf_size - distance) {
		distance = tmp - buf;

		if (distance >= buf_size) {
			if (buf == NULL) {
				buf_size = 1024;
				buf = g_new (char, buf_size);
			} else {
				buf_size *= 2;
				buf = g_renew (char, buf, buf_size);
			}

			tmp = buf + distance;
		}

		fgets (tmp, buf_size - distance, file);
		amt_read = strlen (tmp);
		tmp += amt_read;
	}

	if (tmp) *(tmp - 1) = '\0';

	return buf;
}

/* Parse a line from the log file. All pointers must be valid.
 *
 * Note: backend just points to somewhere in buffer, so it becomes
 * invalid the next time the buffer is overwritten. If there's a
 * trailing newline, it is not chopped off.
 *
 * Returns TRUE on success and FALSE on parse error; if FALSE is
 * returned, the values placed in the variables given are undefined.
 */

static gboolean
parse_line (char *buffer, int *id, struct tm *date, char **backend_id) 
{
	sscanf (buffer, "%x", id);

	while (isxdigit (*buffer)) buffer++;

	if (!isspace (*buffer) || !isdigit (*(buffer + 1))) return FALSE;
	buffer++;

	if (extract_number (&buffer, &date->tm_year, 4) == FALSE)
		return FALSE;
	if (extract_number (&buffer, &date->tm_mon, 2) == FALSE)
		return FALSE;
	if (extract_number (&buffer, &date->tm_mday, 2) == FALSE)
		return FALSE;

	date->tm_year -= 1900;
	date->tm_mon--;

	if (!isspace (*buffer) || !isdigit (*(buffer + 1))) return FALSE;
	buffer++;

	if (extract_number (&buffer, &date->tm_hour, 2) == FALSE)
		return FALSE;
	if (*buffer != ':') return FALSE; buffer++;
	if (extract_number (&buffer, &date->tm_min, 2) == FALSE)
		return FALSE;
	if (*buffer != ':') return FALSE; buffer++;
	if (extract_number (&buffer, &date->tm_sec, 2) == FALSE)
		return FALSE;

	if (!isspace (*buffer) || *(buffer + 1) == '\0') return FALSE;
	buffer++;

	*backend_id = buffer;

	return TRUE;
}

/* Return TRUE if the first given struct tm is greater than or equal
 * to the second given struct tm; FALSE otherwise
 */

static gboolean
time_geq (struct tm *time1, struct tm *time2) 
{
	if (time1->tm_year > time2->tm_year) return TRUE;
	if (time1->tm_year < time2->tm_year) return FALSE;
	if (time1->tm_mon > time2->tm_mon) return TRUE;
	if (time1->tm_mon < time2->tm_mon) return FALSE;
	if (time1->tm_mday > time2->tm_mday) return TRUE;
	if (time1->tm_mday < time2->tm_mday) return FALSE;
	if (time1->tm_hour > time2->tm_hour) return TRUE;
	if (time1->tm_hour < time2->tm_hour) return FALSE;
	if (time1->tm_min > time2->tm_min) return TRUE;
	if (time1->tm_min < time2->tm_min) return FALSE;
	if (time1->tm_sec >= time2->tm_sec) return TRUE;
	return FALSE;
}

/* Opens up a configuration log. Assumes all the structures are
 * already initialized. Creates the log if not already done.
 *
 * Returns TRUE on success and FALSE on failure (unable to open output
 * file or log is locked)
 */

static gboolean
do_load (ConfigLog *config_log) 
{
	FILE *lock_file;

	g_return_val_if_fail (config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), FALSE);
	g_return_val_if_fail (config_log->location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (config_log->location), FALSE);

	do_unload (config_log);
	config_log_reset_filenames (config_log);

	/* FIXME: Race condition here, plus lock handling should be
	 * better */

	if (g_file_test (config_log->lock_filename, G_FILE_TEST_ISFILE))
		return FALSE;

	lock_file = fopen (config_log->lock_filename, "w");
	fclose (lock_file);

	config_log->file = fopen (config_log->filename, "r");

	return TRUE;
}

/* Closes the input file for a given log and dumps and clears the
 * cache 
 */

static void
do_unload (ConfigLog *config_log) 
{
	GList *tmp;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	dump_log (config_log);

	if (config_log->file) {
		fclose (config_log->file);
		config_log->file = NULL;
	}

	if (config_log->filename) {
		g_free (config_log->filename);
		config_log->filename = NULL;
	}

	if (config_log->lock_filename) {
		unlink (config_log->lock_filename);
		g_free (config_log->lock_filename);
		config_log->lock_filename = NULL;
	}

	while (config_log->log_data) {
		tmp = config_log->log_data->next;
		config_log_entry_destroy
			((ConfigLogEntry *) config_log->log_data->data);
		g_list_free_1 (config_log->log_data);
		config_log->log_data = tmp;
	}
}

/* Returns the next id number in the sequence */

static gint
get_next_id (ConfigLog *config_log) 
{
	if (config_log->log_data == NULL) {
		if (load_next_log_entry (config_log, NULL) == NULL)
			return 0;
	}

	return ((ConfigLogEntry *) config_log->log_data->data)->id + 1;
}

/* Return a newly allocated struct tm with the current time */

static struct tm *
get_current_date (void) 
{
	time_t current_time;
	struct tm *time_1, *ret;

	current_time = time (NULL);
	time_1 = gmtime (&current_time);
	ret = g_new (struct tm, 1);
	memcpy (ret, time_1, sizeof (struct tm));
	return ret;
}

/* Write out a log entry */

static void
write_log (FILE *output, ConfigLogEntry *entry) 
{
	g_return_if_fail (output != NULL);
	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->id >= 0);
	g_return_if_fail (entry->date != NULL);
	g_return_if_fail (entry->backend_id != NULL);

	fprintf (output, "%08x %04d%02d%02d %02d:%02d:%02d %s\n",
		 entry->id, entry->date->tm_year + 1900, 
		 entry->date->tm_mon + 1, entry->date->tm_mday, 
		 entry->date->tm_hour, entry->date->tm_min, 
		 entry->date->tm_sec, entry->backend_id);
}

static void 
dump_log (ConfigLog *config_log) 
{
	char *filename_out;
	FILE *output;
	GList *first;
	char buffer[16384];
	size_t size;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));
	g_return_if_fail (config_log->location != NULL);
	g_return_if_fail (IS_LOCATION (config_log->location));
	g_return_if_fail (location_get_path (config_log->location) != NULL);

	filename_out = g_concat_dir_and_file (location_get_path
					      (config_log->location),
					      "config.log.out");

	output = fopen (filename_out, "w");

	if (!output) {
		g_warning ("Could not open output file: %s",
			   g_strerror (errno));
		return;
	}

	for (first = config_log->log_data; first != config_log->first_old; 
	     first = first->next)
		write_log (output, first->data);

	if (config_log->file) {
		rewind (config_log->file);

		while (!feof (config_log->file)) {
			size = fread (buffer, sizeof (char), 
				      16384, config_log->file);
			fwrite (buffer, sizeof (char), size, output);
		}
	}

	fclose (output);

	if (config_log->filename)
		rename (filename_out, config_log->filename);
}

static void 
config_log_entry_destroy (ConfigLogEntry *entry) 
{
	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->date != NULL);
	g_return_if_fail (entry->backend_id != NULL);

	g_free (entry->date);
	g_free (entry->backend_id);
	g_free (entry);
}
