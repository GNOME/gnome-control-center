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

/* Maximum amount to read from an I/O channel at one time */

#define READ_BUF_LEN 4096

#ifndef SUN_LEN
#define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)	      \
		      + strlen ((ptr)->sun_path))
#endif

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

struct _IOBuffer 
{
	GIOChannel  *channel;
	char         buffer[READ_BUF_LEN + 1];
	gchar       *read_ptr;
	gchar       *write_ptr;
	gboolean     from_socket;
	gboolean     read_cycle_done;
	gboolean     closed;
};

struct _Slave
{
	ConfigLog   *config_log;
	IOBuffer    *buffer;
	guint        source_id;
};

struct _ConfigLogPrivate 
{
	Location    *location;

	IOBuffer    *file_buffer;
	char        *filename;
	gboolean     deleted;

	GList       *log_data;
	GList       *first_old;

	char        *socket_filename;
	IOBuffer    *socket_buffer;
	gboolean     socket_owner;
	guint        input_id;

	GList       *slaves;
};

static void       config_log_init               (ConfigLog *config_log);
static void       config_log_class_init         (ConfigLogClass *klass);

static void       config_log_set_arg            (GtkObject *object,
						 GtkArg *arg,
						 guint arg_id);

static void       config_log_get_arg            (GtkObject *object,
						 GtkArg *arg,
						 guint arg_id);

static void       config_log_destroy            (GtkObject *object);
static void       config_log_finalize           (GtkObject *object);

static GList     *find_config_log_entry_id      (ConfigLog *config_log,
						 GList *start, 
						 gint id);
static GList     *find_config_log_entry_date    (ConfigLog *config_log,
						 GList *start, 
						 struct tm *date);
static GList     *find_config_log_entry_backend (ConfigLog *config_log,
						 GList *start, 
						 gchar *backend_id);

static GList     *load_log_entry                (ConfigLog *config_log,
						 gboolean from_socket,
						 IOBuffer *input,
						 GList *last);

static gboolean   parse_line                    (char *buffer, 
						 int *id, 
						 struct tm *time, 
						 char **backend_id);
static gboolean   time_geq                      (struct tm *time1, 
						 struct tm *time2);

static gboolean   do_load                       (ConfigLog *config_log);
static void       do_unload                     (ConfigLog *config_log,
						 gboolean write_log);

static gint       get_next_id                   (ConfigLog *config_log);
static struct tm *get_beginning_of_time         (void);
static struct tm *get_current_date              (void);
static void       write_log                     (IOBuffer *output, 
						 ConfigLogEntry *entry);
static void       dump_log                      (ConfigLog *config_log);
static gboolean   has_nondefaults               (ConfigLog *config_log);

static gboolean   connect_socket                (ConfigLog *config_log);
static gboolean   check_socket_filename         (ConfigLog *config_log);
static gboolean   bind_socket                   (ConfigLog *config_log,
						 int fd,
						 gboolean do_connect);
static void       disconnect_socket             (ConfigLog *config_log);
static gboolean   socket_connect_cb             (GIOChannel *channel,
						 GIOCondition condition,
						 ConfigLog *config_log);
static gboolean   socket_data_cb                (GIOChannel *channel,
						 GIOCondition condition,
						 ConfigLog *config_log);

static void       config_log_entry_destroy      (ConfigLogEntry *entry);

static Slave     *slave_new                     (ConfigLog *config_log,
						 int fd);
static void       slave_destroy                 (Slave *slave);

static gboolean   slave_data_cb                 (GIOChannel *channel,
						 GIOCondition condition,
						 Slave *slave);
static void       slave_broadcast_data          (Slave *slave,
						 ConfigLog *config_log);

static IOBuffer  *io_buffer_new                 (GIOChannel *channel,
						 gboolean from_socket);
static void       io_buffer_destroy             (IOBuffer *buffer);
static int        io_buffer_cycle               (IOBuffer *buffer);
static gchar     *io_buffer_read_line           (IOBuffer *buffer);
static void       io_buffer_write               (IOBuffer *buffer,
						 gchar *str);
static void       io_buffer_rewind              (IOBuffer *buffer);
static void       io_buffer_dump                (IOBuffer *source,
						 IOBuffer *dest);

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
	disconnect_socket (config_log);

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
	connect_socket (CONFIG_LOG (object));

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

	if (config_log->p->file_buffer != NULL) {
		io_buffer_destroy (config_log->p->file_buffer);
		config_log->p->file_buffer = NULL;
	}

	if (config_log->p->filename != NULL)
		unlink (config_log->p->filename);

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
				     gchar *backend_id) 
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, NULL);

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
				     guint steps, gchar *backend_id)
{
	GList *node;

	g_return_val_if_fail (config_log != NULL, -1);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), -1);
	g_return_val_if_fail (backend_id != NULL, -1);

	node = config_log->p->log_data;

	if (node == NULL)
		node = load_log_entry (config_log, FALSE,
				       config_log->p->file_buffer, node);

	while (node != NULL && steps-- > 0) {
		node = find_config_log_entry_backend
			(config_log, node, backend_id);

		if (((ConfigLogEntry *) node->data)->date->tm_year == 0)
			return ((ConfigLogEntry *) node->data)->id;

		if (steps > 0) {
			if (node->next == NULL)
				node = load_log_entry
					(config_log, FALSE,
					 config_log->p->file_buffer, node);
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

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, NULL);

	node = find_config_log_entry_id (config_log,
					 config_log->p->log_data, id);

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

	if (config_log->p->log_data == NULL)
		config_log->p->log_data = 
			load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, NULL);

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
config_log_write_entry (ConfigLog *config_log, gchar *backend_id,
			gboolean is_default_data) 
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

	if (config_log->p->socket_owner) {
		slave_broadcast_data (NULL, config_log);
		dump_log (config_log);
		if (config_log->p->file_buffer)
			io_buffer_destroy (config_log->p->file_buffer);
		do_load (config_log);
	} else {
		write_log (config_log->p->socket_buffer, entry);
	}

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
			node = load_log_entry (config_log, FALSE,
					       config_log->p->file_buffer,
					       node);
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

	if (config_log->p->socket_filename != NULL) {
		if (config_log->p->socket_owner)
			unlink (config_log->p->socket_filename);

		g_free (config_log->p->socket_filename);
	}

	config_log->p->socket_filename = 
		g_concat_dir_and_file (location_get_path 
				       (config_log->p->location),
				       "config.log.socket");

	if (config_log->p->socket_owner) {
		if (check_socket_filename (config_log))
			bind_socket (config_log,
				     g_io_channel_unix_get_fd
				     (config_log->p->socket_buffer->channel),
				     FALSE);
		else
			g_warning ("Could not rebind socket after "
				   "reseting filenames");
	}
}

/**
 * config_log_reload
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
		start = load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, last);
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
		start = load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, last);
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
		start = load_log_entry (config_log, FALSE,
					config_log->p->file_buffer, last);
		if (start == NULL) return NULL;
		entry = (ConfigLogEntry *) start->data;
		if (!strcmp (entry->backend_id, backend_id))
			return start;
	}

	return NULL;
}

/* Loads a log entry from the given file and attaches it to the beginning or
 * the end of the config log */

static GList *
load_log_entry (ConfigLog *config_log, gboolean from_socket,
		IOBuffer *input, GList *last) 
{
	gchar *buffer, *backend_id;
	ConfigLogEntry *entry;
	gboolean success;

	g_return_val_if_fail (config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), NULL);

	if (input == NULL || input->closed) return NULL;

	buffer = io_buffer_read_line (input);

	if (buffer == NULL) return NULL;

	entry = g_new0 (ConfigLogEntry, 1);
	entry->date = g_new0 (struct tm, 1);
	success = parse_line (buffer, &entry->id, 
			      entry->date, &backend_id);

	if (success) {
		entry->backend_id = g_strdup (backend_id);

		if (from_socket) {
			config_log->p->log_data = 
				g_list_prepend (config_log->p->log_data, entry);

			return config_log->p->log_data;
		} else {
			last = g_list_append (last, entry);

			if (config_log->p->log_data == NULL)
				config_log->p->log_data = last;

			if (config_log->p->first_old == NULL)
				config_log->p->first_old = last;

			return g_list_find (last, entry);
		}
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
 * file)
 */

static gboolean
do_load (ConfigLog *config_log) 
{
	int fd;

	g_return_val_if_fail (config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), FALSE);
	g_return_val_if_fail (config_log->p->location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (config_log->p->location), FALSE);

	fd = open (config_log->p->filename, O_RDONLY);

	if (fd != -1)
		config_log->p->file_buffer = 
			io_buffer_new (g_io_channel_unix_new (fd), FALSE);
	else
		config_log->p->file_buffer = NULL;

	return TRUE;
}

/* Closes the input file for a given log and dumps and clears the
 * cache 
 */

static void
do_unload (ConfigLog *config_log, gboolean write_log) 
{
	GList *tmp;

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));

	if (write_log && config_log->p->socket_owner) dump_log (config_log);

	if (config_log->p->file_buffer) {
		io_buffer_destroy (config_log->p->file_buffer);
		config_log->p->file_buffer = NULL;
	}

	if (config_log->p->filename) {
		g_free (config_log->p->filename);
		config_log->p->filename = NULL;
	}

	if (config_log->p->socket_filename) {
		if (config_log->p->socket_owner)
			unlink (config_log->p->socket_filename);

		g_free (config_log->p->socket_filename);
		config_log->p->socket_filename = NULL;
	}

	while (config_log->p->log_data != NULL) {
		tmp = config_log->p->log_data->next;
		config_log_entry_destroy
			((ConfigLogEntry *) config_log->p->log_data->data);
		g_list_free_1 (config_log->p->log_data);
		config_log->p->log_data = tmp;
	}
}

/* Returns the next id number in the sequence */

static gint
get_next_id (ConfigLog *config_log) 
{
	if (config_log->p->log_data == NULL) {
		if (load_log_entry (config_log, FALSE,
				    config_log->p->file_buffer, NULL) == NULL)
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
	struct tm *time_1, *ret;

	current_time = time (NULL);
	time_1 = localtime (&current_time);
	ret = g_new (struct tm, 1);
	memcpy (ret, time_1, sizeof (struct tm));
	return ret;
}

/* Write out a log entry */

static void
write_log (IOBuffer *output, ConfigLogEntry *entry) 
{
	gchar *str;

	g_return_if_fail (output != NULL);
	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->id >= 0);
	g_return_if_fail (entry->date != NULL);
	g_return_if_fail (entry->backend_id != NULL);

	str = g_strdup_printf ("%08x %04d%02d%02d %02d:%02d:%02d %s\n",
			       entry->id, entry->date->tm_year + 1900, 
			       entry->date->tm_mon + 1, entry->date->tm_mday, 
			       entry->date->tm_hour, entry->date->tm_min, 
			       entry->date->tm_sec, entry->backend_id);
	DEBUG_MSG ("Writing %s, from_socket = %d", str, output->from_socket);
	io_buffer_write (output, str);
	g_free (str);
}

static void 
dump_log (ConfigLog *config_log) 
{
	char *filename_out;
	GList *first;
	int out_fd;
	IOBuffer *output;

	DEBUG_MSG ("Enter");

	g_return_if_fail (config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (config_log));
	g_return_if_fail (config_log->p->location != NULL);
	g_return_if_fail (IS_LOCATION (config_log->p->location));
	g_return_if_fail (location_get_path (config_log->p->location) != NULL);

	filename_out = g_concat_dir_and_file (location_get_path
					      (config_log->p->location),
					      "config.log.out");

	out_fd = open (filename_out, O_CREAT | O_WRONLY | O_TRUNC, 0600);

	if (out_fd == -1) {
		g_critical ("Could not open output file: %s",
			    g_strerror (errno));
		return;
	}

	output = io_buffer_new (g_io_channel_unix_new (out_fd), FALSE);

	for (first = config_log->p->log_data;
	     first != config_log->p->first_old; 
	     first = first->next)
		write_log (output, first->data);

	config_log->p->first_old = config_log->p->log_data;

	if (config_log->p->file_buffer) {
		io_buffer_rewind (config_log->p->file_buffer);
		io_buffer_dump (config_log->p->file_buffer, output);
	}

	io_buffer_destroy (output);

	if (config_log->p->filename)
		rename (filename_out, config_log->p->filename);

	DEBUG_MSG ("Exit");
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
		load_log_entry (config_log, FALSE, config_log->p->file_buffer,
				config_log->p->log_data);

	if (config_log->p->log_data == NULL)
		return FALSE;

	first = config_log->p->log_data->data;

	if (first->date->tm_year == 0)
		return FALSE;
	else
		return TRUE;
}

/* Try to connect to the synchronization socket for this configuration log. If
 * no socket exists, take ownership of the config log and listen for slave
 * connections.
 */

static gboolean
connect_socket (ConfigLog *config_log)
{
	int fd, flags;

	config_log->p->socket_buffer = NULL;
	config_log->p->socket_owner = check_socket_filename (config_log);

	fd = socket (PF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) {
		g_warning ("Could not create socket: %s", g_strerror (errno));
		return FALSE;
	}

	if (!bind_socket (config_log, fd, TRUE)) {
		close (fd);
		return FALSE;
	}

	config_log->p->socket_buffer =
		io_buffer_new (g_io_channel_unix_new (fd), TRUE);

	if (config_log->p->socket_owner) {
		config_log->p->input_id = 
			g_io_add_watch (config_log->p->socket_buffer->channel,
					G_IO_IN | G_IO_ERR,
					(GIOFunc) socket_connect_cb,
					config_log);
	} else {
		DEBUG_MSG ("Adding watch to listen for data"); 

		config_log->p->input_id = 
			g_io_add_watch (config_log->p->socket_buffer->channel,
					G_IO_IN | G_IO_ERR | G_IO_HUP,
					(GIOFunc) socket_data_cb,
					config_log);

		/* Read any data that might have come through before we added
		 * the watch */
		flags = fcntl (fd, F_GETFL);

		if (flags != -1) {
			flags |= O_NONBLOCK;
			fcntl (fd, F_SETFL, flags);
		}

		socket_data_cb (config_log->p->socket_buffer->channel,
				G_IO_IN, config_log);
	}

	return config_log->p->socket_owner;
}

/* Checks to see if the filename associated with the socket is free. Returns
 * TRUE if it is and FALSE otherwise */

static gboolean
check_socket_filename (ConfigLog *config_log) 
{
	struct stat buf;
	gboolean is_free;

	if (config_log->p->socket_filename == NULL) return FALSE;

	if (stat (config_log->p->socket_filename, &buf) == -1) {
		if (errno == ENOENT) {
			is_free = TRUE;
		} else {
			g_warning ("Could not stat file: %s",
				   g_strerror (errno));
			return FALSE;
		}
	} else {
		is_free = FALSE;
	}

	if (!is_free && !S_ISSOCK (buf.st_mode)) {
		g_warning ("There is another file in the way of the socket");
		return FALSE;
	}

	return is_free;
}

/* Binds (or rebinds) the given socket to the correct filename; optionally
 * connects to the correct socket if the current object is not the config log
 * owner
 *
 * If it cannot connect to the given socket, this means the process owner has
 * died unexpectedly and left it lying around. It then tries to take ownership
 * of it.
 */

static gboolean
bind_socket (ConfigLog *config_log, int fd, gboolean do_connect) 
{
	struct sockaddr_un name;

	/* FIXME: What if the socket filename is too long here? */
	if (config_log->p->socket_owner || do_connect) {
		name.sun_family = AF_UNIX;
		strncpy (name.sun_path, config_log->p->socket_filename,
			 sizeof (name.sun_path));
	}

	if (do_connect && !config_log->p->socket_owner) {
		DEBUG_MSG ("Trying to connect to socket");

		if (!connect (fd, (struct sockaddr *) &name, SUN_LEN (&name)))
			return TRUE;

		if (errno != ECONNREFUSED) {
			g_warning ("Could not connect to socket: %s",
				   g_strerror (errno));
			return FALSE;
		} else {
			unlink (config_log->p->socket_filename);
			config_log->p->socket_owner = TRUE;
		}
	}

	if (bind (fd, (struct sockaddr *) &name, SUN_LEN (&name)) < 0) {
		g_warning ("Could not bind to socket filename: %s",
			   g_strerror (errno));
		return FALSE;
	}

	if (listen (fd, 5) < 0) {
		g_warning ("Could not set up socket for listening: %s",
			   g_strerror (errno));
		return FALSE;
	}

	return TRUE;
}

static void
disconnect_socket (ConfigLog *config_log) 
{
	if (config_log->p->socket_owner)
		g_list_foreach (config_log->p->slaves,
				(GFunc) slave_destroy, NULL);

	g_source_remove (config_log->p->input_id);

	if (config_log->p->socket_buffer != NULL)
		io_buffer_destroy (config_log->p->socket_buffer);
}

static gboolean
socket_connect_cb (GIOChannel *channel, GIOCondition condition,
		   ConfigLog *config_log) 
{
	int fd;
	struct sockaddr_un addr;
	socklen_t len;

	DEBUG_MSG ("Enter");

	g_return_val_if_fail (config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), FALSE);

	if (condition == G_IO_IN) {
		fd = accept (g_io_channel_unix_get_fd
			     (config_log->p->socket_buffer->channel),
			     &addr, &len);

		if (fd < 0) {
			g_warning ("Could not accept connection: %s",
				   g_strerror (fd));
			return TRUE;
		}

		config_log->p->slaves =
			g_list_prepend (config_log->p->slaves,
					slave_new (config_log, fd));
	}

	DEBUG_MSG ("Exit");

	return TRUE;
}

/* Callback issued when data comes in over the socket; only used when this is
 * *not* the master socket. */

static gboolean
socket_data_cb (GIOChannel *channel, GIOCondition condition,
		ConfigLog *config_log)
{
	g_return_val_if_fail (config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (config_log), FALSE);

	DEBUG_MSG ("Enter");

	if (condition & G_IO_HUP) {
		DEBUG_MSG ("Connection closing");
		disconnect_socket (config_log);
		connect_socket (config_log);
		return FALSE;
	}
	else if (condition & G_IO_IN) {
		load_log_entry (config_log, TRUE,
				config_log->p->socket_buffer, NULL);
	}

	DEBUG_MSG ("Exit");

	return TRUE;
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

static Slave *
slave_new (ConfigLog *config_log, int fd) 
{
	Slave *slave;

	slave = g_new0 (Slave, 1);
	slave->config_log = config_log;
	slave->buffer = io_buffer_new (g_io_channel_unix_new (fd), TRUE);
	slave->source_id = g_io_add_watch (slave->buffer->channel,
					   G_IO_IN | G_IO_ERR | G_IO_HUP,
					   (GIOFunc) slave_data_cb,
					   slave);

	return slave;
}

static void
slave_destroy (Slave *slave) 
{
	g_return_if_fail (slave != NULL);
	g_return_if_fail (slave->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (slave->config_log));

	slave->config_log->p->slaves =
		g_list_remove (slave->config_log->p->slaves, slave);
	io_buffer_destroy (slave->buffer);
	g_free (slave);
}

static gboolean
slave_data_cb (GIOChannel *channel, GIOCondition condition,
	       Slave *slave)
{
	DEBUG_MSG ("Enter");

	g_return_val_if_fail (slave != NULL, FALSE);
	g_return_val_if_fail (slave->config_log != NULL, FALSE);
	g_return_val_if_fail (IS_CONFIG_LOG (slave->config_log), FALSE);

	DEBUG_MSG ("Condition is %d", condition);

	if (condition & G_IO_HUP || slave->buffer->closed) {
		DEBUG_MSG ("Removing slave");
		slave_destroy (slave);
		return FALSE;
	}
	else if (condition & G_IO_IN) {
		if (load_log_entry (slave->config_log, TRUE, slave->buffer,
				    NULL) != NULL) 
		{
			slave_broadcast_data (slave, slave->config_log);
			dump_log (slave->config_log);
			if (slave->config_log->p->file_buffer)
				io_buffer_destroy
					(slave->config_log->p->file_buffer);
			do_load (slave->config_log);
		}
	}

	DEBUG_MSG ("Exit");

	return TRUE;
}

/* Broadcasts first log entry to all the slaves of the config log besides the
 * one given */

static void
slave_broadcast_data (Slave *slave, ConfigLog *config_log) 
{
	GList *node;
	Slave *current;
	ConfigLogEntry *first_entry;

	DEBUG_MSG ("Enter");

	first_entry = config_log->p->log_data->data;

	for (node = config_log->p->slaves; node != NULL; node = node->next) {
		current = node->data;

		if (current == slave) continue;
		write_log (current->buffer, first_entry);
	}

	DEBUG_MSG ("Exit");
}

static IOBuffer *
io_buffer_new (GIOChannel *channel, gboolean from_socket)
{
	IOBuffer *buffer;

	buffer = g_new0 (IOBuffer, 1);
	buffer->channel = channel;
	buffer->read_ptr = buffer->write_ptr = buffer->buffer;
	buffer->from_socket = from_socket;

	return buffer;
}

static void
io_buffer_destroy (IOBuffer *buffer)
{
	g_io_channel_close (buffer->channel);
	g_io_channel_unref (buffer->channel);
	g_free (buffer);
}

/* Note: The two functions below are borrowed from GDict */

/* io_buffer_cycle ()
 *
 * Reads additional data from the socket if it can
 *
 * Returns 0 on success, 1 if there is no more data to read, -1 on socket error
 */

static int 
io_buffer_cycle (IOBuffer *buffer) 
{
	int amt_read, res;
    
	if (!buffer->read_cycle_done && !buffer->closed) {
		if (buffer->write_ptr > buffer->read_ptr)
			memmove (buffer->buffer, buffer->read_ptr,
				 buffer->write_ptr - buffer->read_ptr);
		buffer->write_ptr -= buffer->read_ptr - buffer->buffer;
		buffer->read_ptr = buffer->buffer;

		res = g_io_channel_read (buffer->channel, buffer->write_ptr,
					 buffer->buffer + READ_BUF_LEN 
					 - buffer->write_ptr, 
					 &amt_read);

		if (res == G_IO_ERROR_AGAIN)
			buffer->read_cycle_done = TRUE;
		else if (amt_read == 0)
			buffer->closed = TRUE;
		else if (res != G_IO_ERROR_NONE)
			return -1;

		buffer->write_ptr += amt_read;
		return 0;
	}
	else {
		return 1;
	}
}

/* io_buffer_read_line ()
 *
 * Reads a line of text from the socket and performs a CR-LF translation
 *
 * Returns a pointer to the string on success
 */

static gchar *
io_buffer_read_line (IOBuffer *buffer) {
	gchar *start_ptr, *end_ptr;

	if (!buffer->channel) return NULL;

	while (1) {
		end_ptr = strchr (buffer->read_ptr, '\n');

		if (end_ptr == NULL || end_ptr > buffer->write_ptr) {
			if (io_buffer_cycle (buffer)) return NULL;
		}
		else {
			break;
		}
	}

	end_ptr[0] = '\0';

	start_ptr = buffer->read_ptr;
	buffer->read_ptr = end_ptr + 1;

	DEBUG_MSG ("Line read was %s; from_socket = %d",
		   start_ptr, buffer->from_socket);

	return start_ptr;
}

static void
io_buffer_write (IOBuffer *buffer, gchar *str) 
{
	gint bytes_written;

	g_return_if_fail (buffer != NULL);

	if (str == NULL) return;

	g_io_channel_write (buffer->channel, str, strlen (str),
			    &bytes_written);
}

static void
io_buffer_rewind (IOBuffer *buffer)
{
	g_return_if_fail (buffer != NULL);

	g_io_channel_seek (buffer->channel, 0, G_SEEK_SET);
}

/* Dumps the contents of the source into the destination.
 *
 * Note: The source will be completely screwed up and unusable after this
 * point!!!!
 */

static void
io_buffer_dump (IOBuffer *source, IOBuffer *dest)
{
	gchar buffer[4096];
	gint written, read;

	g_return_if_fail (buffer != NULL);

	do {
		g_io_channel_read (source->channel, buffer, 4096, &read);
		g_io_channel_write (dest->channel, buffer, read, &written);
	} while (read == 4096);
}
