/* -*- mode: c; style: linux -*- */

/* config-log.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen (hovinen@ximian.com)
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
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "config-log.h"
#include "location.h"
#include "util.h"

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_LOCATION
};

typedef struct _ConfigLogEntry ConfigLogEntry;
typedef struct _Slave Slave;
typedef struct _IOBuffer IOBuffer;

struct _ConfigLogEntry 
{
	gint        id;
	struct tm  *date;
	gchar      *backend_id;
};

struct _ConfigLogPrivate 
{
	Location    *location;

	FILE        *file_stream;
	char        *filename;
	gboolean     deleted;

	GList       *log_data;
	GList       *first_old;
};

static void       config_log_init               (ConfigLog       *config_log);
static void       config_log_class_init         (ConfigLogClass  *klass);

static void       config_log_set_arg            (GtkObject       *object,
						 GtkArg          *arg,
						 guint            arg_id);

static void       config_log_get_arg            (GtkObject       *object,
						 GtkArg          *arg,
						 guint            arg_id);

static void       config_log_destroy            (GtkObject       *object);
static void       config_log_finalize           (GtkObject       *object);

static GList     *find_config_log_entry_id      (ConfigLog       *config_log,
						 GList           *start, 
						 gint             id);
static GList     *find_config_log_entry_date    (ConfigLog       *config_log,
						 GList           *start, 
						 struct tm       *date);
static GList     *find_config_log_entry_backend (ConfigLog       *config_log,
						 GList           *start, 
						 const gchar     *backend_id);

static GList     *load_log_entry                (ConfigLog       *config_log,
						 GList           *last);

static gboolean   parse_line                    (char            *buffer, 
						 int             *id, 
						 struct tm       *time, 
						 char           **backend_id);
static gboolean   time_geq                      (struct tm       *time1, 
						 struct tm       *time2);

static gboolean   do_load                       (ConfigLog       *config_log);
static void       do_unload                     (ConfigLog       *config_log,
						 gboolean         write_log);

static gint       get_next_id                   (ConfigLog       *config_log);
static struct tm *get_beginning_of_time         (void);
static struct tm *get_current_date              (void);
static void       write_log                     (FILE            *output, 
						 ConfigLogEntry  *entry);
static void       dump_log                      (ConfigLog       *config_log);
static gboolean   has_nondefaults               (ConfigLog       *config_log);

static void       config_log_entry_destroy      (ConfigLogEntry  *entry);

static void       dump_file                     (FILE            *input,
						 FILE            *output);

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
	config_log->p = g_new0 (ConfigLogPrivate, 1);
}

static void
config_log_class_init (ConfigLogClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = config_log_destroy;
	object_class->finalize = config_log_finalize;
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

		config_log->p->location = GTK_VALUE_POINTER (*arg);
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
		GTK_VALUE_POINTER (*arg) = config_log->p->location;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
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

	do_unload (config_log, !config_log->p->deleted);

	GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (config_log));
}

/* Deallocates memory associated with a config log structure */

static void
config_log_finalize (GtkObject *object) 
{
	ConfigLog *config_log;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_LOG (object));

	config_log = CONFIG_LOG (object);

	g_free (config_log->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (GTK_OBJECT (config_log));
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

	config_log_reset_filenames (CONFIG_LOG (object));
	do_load (CONFIG_LOG (object));

	return object;
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

	if (config_log->p->filename != NULL)
		unlink (config_log->p->filename);

	do_unload (config_log, FALSE);

	config_log->p->deleted = TRUE;
	gtk_object_destroy (GTK_OBJECT (config_log));
}

/* Return the id number of the most recent data written by the given
 * backend prior to the given date. If date is NULL, it is assumed to
 * be today.
 */

gint 
config_log_get_rollback_id_for_date (ConfigLog *config_log,
				     struct tm *date,
				     const gchar *backend_id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, NULL);

	if (date == NULL)
		node = config_log->p->log_data;
	else
		node = find_config_log_entry_date (config_log,
						   config_log->p->log_data,
						   date);

	node = find_config_log_entry_backend (config_log, node, backend_id);

	if (!node)
		return -1;
	else
		return ((ConfigLogEntry *) node->data)->id;
}

/* Return the rollback id that is the given number of steps back from the
 * current revision, or -1 if there is no such id
 */

gint
config_log_get_rollback_id_by_steps (ConfigLog *config_log,
				     guint steps,
				     const gchar *backend_id)
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	node = config_log->p->log_data;

	if (node == NULL)
		node = load_log_entry (config_log, node);

	while (node != NULL && steps-- > 0) {
		node = find_config_log_entry_backend
			(config_log, node, backend_id);

		if (((ConfigLogEntry *) node->data)->date->tm_year == 0)
			return ((ConfigLogEntry *) node->data)->id;

		if (steps > 0) {
			if (node->next == NULL)
				node = load_log_entry
					(config_log, node);
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

const gchar *
config_log_get_backend_id_for_id (ConfigLog *config_log, gint id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (id >= 0, NULL);

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, NULL);

	node = find_config_log_entry_id (config_log,
					 config_log->p->log_data, id);

	if (!node) 
		return NULL;
	else
		return ((ConfigLogEntry *) node->data)->backend_id;
}

/* Return the date the data with the given id was written */

const struct tm *
config_log_get_date_for_id (ConfigLog *config_log, gint id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);
	g_return_val_if_fail (id >= 0, NULL);

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, NULL);

	node = find_config_log_entry_id (config_log,
					 config_log->p->log_data, id);

	if (!node) 
		return NULL;
	else
		return ((ConfigLogEntry *) node->data)->date;
}

/**
 * config_log_write_entry:
 * @config_log:
 * @backend_id: Backend id for the log entry to write
 * @is_default_data: TRUE iff the corresponding data are to be considered
 * "factory defaults" for the purpose of rollback
 *
 * Writes a new log entry to the config log
 *
 * Returns the id number of the entry on success or -1 on failure
 **/

gint 
config_log_write_entry (ConfigLog   *config_log,
			const gchar *backend_id,
			gboolean     is_default_data) 
{
	ConfigLogEntry *entry;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	if (is_default_data && has_nondefaults (config_log))
		return -1;

	entry             = g_new0 (ConfigLogEntry, 1);
	entry->id         = get_next_id (config_log);
	entry->backend_id = g_strdup (backend_id);

	if (is_default_data)
		entry->date = get_beginning_of_time ();
	else
		entry->date = get_current_date ();

	config_log->p->log_data =
		g_list_prepend (config_log->p->log_data, entry);

	dump_log (config_log);

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

	node = config_log->p->log_data;
	while (node != NULL) {
		entry = (ConfigLogEntry *) node->data;
		if (callback (config_log, entry->id, entry->backend_id,
			      entry->date, data)) break;

		if (node->next == NULL) 
			node = load_log_entry (config_log, node);
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

	if (config_log->p->filename != NULL)
		g_free (config_log->p->filename);

	config_log->p->filename = 
		g_concat_dir_and_file (location_get_path
				       (config_log->p->location),
				       "config.log");
}

/**
 * config_log_reload:
 * @config_log:
 *
 * Reloads the entire config log, throwing out any newly created entries
 **/

void
config_log_reload (ConfigLog *config_log) 
{
	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	do_unload (config_log, FALSE);
	config_log_reset_filenames (config_log);
	do_load (config_log);
}

/**
 * config_log_garbage_collect:
 * @config_log:
 * @backend_id: Backend on which to iterate
 * @callback: Callback to issue on any log entry to be culled
 * @data: Arbitrary data to pass to callback
 *
 * Iterates through the configuration log and culls excess entries for the given
 * backend.
 *
 * The algorithm we use is the following: We scan entries in temporal order. For
 * each consecutive pair of entries, let t1 be the time at which the former was
 * made and t2 be the time of the latter. If K_CONST * (t2 - t1) < t2, then we
 * delete the former entry and issue the callback. We select K_CONST
 * appropriately, i.e. a user will likely not want to keep entries separated by
 * under five minutes for very long, while she may want to keep entries
 * separated by two weeks for much longer.
 */

#define K_CONST 15

void
config_log_garbage_collect (ConfigLog        *config_log,
			    gchar            *backend_id,
			    GarbageCollectCB  callback,
			    gpointer          data)
{
	GList *node, *list = NULL;
	ConfigLogEntry *e1, *e2;
	time_t t1, t2, now;
	struct tm now_b, *tmp_date;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, NULL);

	node = config_log->p->log_data;

	/* We build a list of config log nodes to facilitate removing the nodes
	 * from the main cache at a later point
	 */

	while (1) {
		node = find_config_log_entry_backend
			(config_log, node, backend_id);

		if (node == NULL)
			break;

		list = g_list_prepend (list, node);
		node = node->next;
	}

	if (list == NULL) return;

	now = time (NULL);
	gmtime_r (&now, &now_b);
	now = mktime (&now_b);

	for (node = list; node->next != NULL; node = node->next) {
		e1 = ((GList *) node->data)->data;
		e2 = ((GList *) node->next->data)->data;

		tmp_date = dup_date (e1->date);
		t1 = mktime (tmp_date);
		g_free (tmp_date);

		tmp_date = dup_date (e2->date);
		t2 = mktime (tmp_date);
		g_free (tmp_date);

		if (now < t2 || now < t1)
			g_warning ("Log entry is in the future!");
		if (t1 > t2)
			g_warning ("Log entries are out of order!");

		if (K_CONST * difftime (t2, t1) < difftime (now, t2)) {
			config_log->p->log_data =
				g_list_remove_link (config_log->p->log_data, node->data);
			callback (config_log, backend_id, e1->id, data);
		}
	}

	g_list_free (list);

	config_log->p->first_old = NULL;
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
		start = load_log_entry (config_log, last);
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
		start = load_log_entry (config_log, last);
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
find_config_log_entry_backend (ConfigLog *config_log,
			       GList *start, 
			       const gchar *backend_id) 
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
		start = load_log_entry (config_log, last);
		if (start == NULL) return NULL;
		entry = (ConfigLogEntry *) start->data;
		if (!strcmp (entry->backend_id, backend_id))
			return start;
	}

	return NULL;
}

/* Loads a log entry from the given file and attaches it to the end of the config log */

static GList *
load_log_entry (ConfigLog *config_log,
		GList     *last) 
{
	gchar *backend_id;
	gchar buffer[1024];
	ConfigLogEntry *entry;
	gboolean success;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);

	if (config_log->p->file_stream == NULL || feof (config_log->p->file_stream))
		return NULL;

	if (fgets (buffer, 1024, config_log->p->file_stream) == NULL)
		return NULL;

	entry = g_new0 (ConfigLogEntry, 1);
	entry->date = g_new0 (struct tm, 1);
	success = parse_line (buffer, &entry->id, 
			      entry->date, &backend_id);

	if (success) {
		entry->backend_id = g_strdup (backend_id);

		last = g_list_append (last, entry);

		if (config_log->p->log_data == NULL)
			config_log->p->log_data = last;

		if (config_log->p->first_old == NULL)
			config_log->p->first_old = last;

		return g_list_find (last, entry);
	} else {
		g_free (entry->date);
		g_free (entry);
		return NULL;
	}
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
	unsigned int len;

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

	date->tm_gmtoff = 0;
	date->tm_zone = "GMT";

	if (!isspace (*buffer) || *(buffer + 1) == '\0') return FALSE;
	buffer++;

	len = strlen (buffer);
	if (buffer[len - 1] == '\n')
		buffer[len - 1] = '\0';

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
 * file)
 */

static gboolean
do_load (ConfigLog *config_log) 
{
	g_return_val_if_fail (config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), FALSE);

	config_log->p->file_stream = fopen (config_log->p->filename, "r");
	config_log->p->first_old = NULL;

	return TRUE;
}

/* Closes the input file for a given log and dumps and clears the
 * cache 
 */

static void
do_unload (ConfigLog *config_log, gboolean write_log) 
{
	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	if (config_log->p->deleted) return;

	if (write_log) dump_log (config_log);

	if (config_log->p->file_stream) {
		fclose (config_log->p->file_stream);
		config_log->p->file_stream = NULL;
	}

	if (config_log->p->filename) {
		g_free (config_log->p->filename);
		config_log->p->filename = NULL;
	}

	g_list_foreach (config_log->p->log_data, (GFunc) config_log_entry_destroy, NULL);
	g_list_free (config_log->p->log_data);
	config_log->p->log_data = NULL;
}

/* Returns the next id number in the sequence */

static gint
get_next_id (ConfigLog *config_log) 
{
	if (config_log->p->log_data == NULL) {
		if (load_log_entry (config_log, NULL) == NULL)
			return 0;
	}

	return ((ConfigLogEntry *) config_log->p->log_data->data)->id + 1;
}

/* Return a newly allocated struct tm with all zeros */

static struct tm *
get_beginning_of_time (void)
{
	return g_new0 (struct tm, 1);
}

/* Return a newly allocated struct tm with the current time */

static struct tm *
get_current_date (void) 
{
	time_t current_time;
	struct tm time_2, *time_1 = &time_2, *ret;

	current_time = time (NULL);
	gmtime_r (&current_time, time_1);
	ret = g_new (struct tm, 1);
	memcpy (ret, time_1, sizeof (struct tm));
	return ret;
}

/* Write out a single log entry */

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

/* Writes out the entire current configuration log to the disk */

static void 
dump_log (ConfigLog *config_log) 
{
	char  *filename_out;
	GList *first;
	FILE  *input, *output;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));
	g_return_if_fail (config_log->p->location != NULL);
	g_return_if_fail (IS_LOCATION (config_log->p->location));
	g_return_if_fail (location_get_path (config_log->p->location) != NULL);

	if (config_log->p->deleted) return;

	filename_out = g_concat_dir_and_file (location_get_path
					      (config_log->p->location),
					      "config.log.out");

	output = fopen (filename_out, "w");

	if (output == NULL) {
		g_critical ("Could not open output file: %s",
			    g_strerror (errno));
		return;
	}

	for (first = config_log->p->log_data;
	     first != config_log->p->first_old; 
	     first = first->next)
		write_log (output, first->data);

	if (config_log->p->file_stream != NULL &&
	    ((config_log->p->first_old == NULL && config_log->p->log_data == NULL) ||
	     (config_log->p->first_old != NULL && config_log->p->log_data != NULL)))
	{
		input = fopen (config_log->p->filename, "r");
		dump_file (input, output);
		fclose (input);
	}

	config_log->p->first_old = config_log->p->log_data;

	fclose (output);

	if (config_log->p->filename)
		rename (filename_out, config_log->p->filename);
}

/* Return TRUE if the config log has entries made by actual configuration
 * changes, as opposed to default values placed there when the location was
 * initialized
 */

static gboolean
has_nondefaults (ConfigLog *config_log)
{
	ConfigLogEntry *first;

	if (config_log->p->log_data == NULL)
		load_log_entry (config_log, config_log->p->log_data);

	if (config_log->p->log_data == NULL)
		return FALSE;

	first = config_log->p->log_data->data;

	if (first->date->tm_year == 0)
		return FALSE;
	else
		return TRUE;
}

/* Deallocates the given config log entry structure */

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

/* Dumps the entire contents from one stream into another */

static void
dump_file (FILE *input, FILE *output) 
{
	char buffer[4096];
	size_t len;

	g_return_if_fail (input != NULL);
	g_return_if_fail (output != NULL);

	while (!feof (input)) {
		len = fread (buffer, sizeof (char), 4096, input);
		fwrite (buffer, sizeof (char), len, output);
	}
}
