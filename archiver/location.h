/* -*- mode: c; style: linux -*- */

/* location.h
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

#ifndef __LOCATION_H
#define __LOCATION_H

#include <gnome.h>
#include <tree.h>
#include <bonobo.h>

#include "ConfigArchiver.h"

#include "config-log.h"

#define LOCATION(obj)          GTK_CHECK_CAST (obj, location_get_type (), Location)
#define LOCATION_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, location_get_type (), LocationClass)
#define IS_LOCATION(obj)       GTK_CHECK_TYPE (obj, location_get_type ())

typedef struct _LocationClass LocationClass;
typedef struct _LocationPrivate LocationPrivate;
typedef struct _Archive Archive;

typedef ConfigArchiver_ContainmentType ContainmentType;
typedef ConfigArchiver_StoreType StoreType;

typedef int (*LocationBackendCB) (Location *, gchar *, gpointer);

struct _Location 
{
	BonoboXObject object;

	LocationPrivate *p;
};

struct _LocationClass 
{
	BonoboXObjectClass parent;

	POA_ConfigArchiver_Location__epv epv;

	gboolean (*do_rollback) (Location *location,
				 gchar *backend_id,
				 xmlDocPtr xml_doc);
};

GType           location_get_type                     (void);

BonoboObject   *location_new                          (Archive         *archive, 
						       const gchar     *locid, 
						       const gchar     *label,
						       Location        *parent);
BonoboObject   *location_open                         (Archive         *archive, 
						       const gchar     *locid);

void            location_delete                       (Location        *location);

gchar          *location_get_storage_filename         (Location        *location,
						       const gchar     *backend_id,
						       gboolean         is_default);
gchar          *location_get_rollback_filename        (Location        *location,
						       struct tm       *date,
						       gint             steps,
						       const gchar     *backend_id,
						       gboolean         parent_chain);

void            location_storage_complete             (Location        *location,
						       const gchar     *filename);

gint            location_store                        (Location        *location, 
						       gchar           *backend_id, 
						       FILE            *input,
						       ConfigArchiver_StoreType store_type);
void            location_store_xml                    (Location        *location, 
						       gchar           *backend_id, 
						       xmlDocPtr        xml_doc,
						       ConfigArchiver_StoreType store_type);

void            location_rollback_backends_to         (Location        *location,
						       struct tm       *date,
						       gint             steps,
						       GList           *backends,
						       gboolean         parent_chain);

const struct tm *location_get_modification_time        (Location        *location,
							const gchar     *backend_id);

ContainmentType location_contains                     (Location        *location,
						       const gchar     *backend_id);
gint            location_add_backend                  (Location        *location,
						       const gchar     *backend_id,
						       ContainmentType  type);
void            location_remove_backend               (Location        *location,
						       const gchar     *backend_id);

void            location_foreach_backend              (Location          *location,
						       LocationBackendCB  callback,
						       gpointer           data);

GList          *location_find_path_from_common_parent (Location        *location, 
						       Location        *location2);

Location       *location_get_parent                   (Location        *location);
const gchar    *location_get_path                     (Location        *location);
const gchar    *location_get_label                    (Location        *location);
const gchar    *location_get_id                       (Location        *location);

void            location_set_id                       (Location        *location,
						       const gchar     *locid);

gint            location_store_full_snapshot          (Location        *location);

GList          *location_get_changed_backends         (Location        *location,
						       Location        *location1);
gboolean        location_does_backend_change          (Location        *location,
						       Location        *location1,
						       const gchar     *backend_id);

ConfigLog      *location_get_config_log               (Location        *location);

void            location_garbage_collect              (Location        *location);

gboolean        location_is_deleted                   (const Location  *location);

#endif /* __LOCATION */
