/* -*- mode: c; style: linux -*- */

/* config-log.h
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

#ifndef __CONFIG_LOG_H
#define __CONFIG_LOG_H

#include <gnome.h>
#include <stdio.h>
#include <time.h>

#define CONFIG_LOG(obj)          GTK_CHECK_CAST (obj, config_log_get_type (), ConfigLog)
#define CONFIG_LOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, config_log_get_type (), ConfigLogClass)
#define IS_CONFIG_LOG(obj)       GTK_CHECK_TYPE (obj, config_log_get_type ())

typedef struct _ConfigLog ConfigLog;
typedef struct _ConfigLogClass ConfigLogClass;
typedef struct _ConfigLogPrivate ConfigLogPrivate;

typedef struct _Location Location;

typedef gint (*ConfigLogIteratorCB) (ConfigLog *, gint, gchar *, 
				     struct tm *, gpointer);
typedef void (*GarbageCollectCB) (ConfigLog *, gchar *, gint, gpointer);

struct _ConfigLog 
{
	GtkObject object;

	ConfigLogPrivate *p;
};

struct _ConfigLogClass 
{
	GtkObjectClass parent;
};

guint      config_log_get_type                 (void);

GtkObject *config_log_open                     (Location            *location);
void       config_log_delete                   (ConfigLog           *config_log);

gint       config_log_get_rollback_id_for_date (ConfigLog           *config_log,
						struct tm           *date,
						gchar               *backend_id);
gint       config_log_get_rollback_id_by_steps (ConfigLog           *config_log,
						guint                steps,
						gchar               *backend_id);

gchar     *config_log_get_backend_id_for_id    (ConfigLog           *config_log,
						gint                 id);
struct tm *config_log_get_date_for_id          (ConfigLog           *config_log,
						gint                 id);

gint       config_log_write_entry              (ConfigLog           *config_log,
						gchar               *backend_id,
						gboolean             is_default_data);

void       config_log_iterate                  (ConfigLog           *config_log,
						ConfigLogIteratorCB  callback,
						gpointer             data);

void       config_log_reset_filenames          (ConfigLog           *config_log);
void       config_log_reload                   (ConfigLog           *config_log);

void       config_log_garbage_collect          (ConfigLog           *config_log,
						gchar               *backend_id,
						GarbageCollectCB     callback,
						gpointer             data);

#endif /* __CONFIG_LOG */
