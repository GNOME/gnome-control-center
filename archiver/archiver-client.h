/* -*- mode: c; style: linux -*- */

/* archiver-client.h
 * Copyright (C) 2001 Ximian, Inc.
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

#ifndef __ARCHIVER_CLIENT_H
#define __ARCHIVER_CLIENT_H

#include <gnome.h>
#include <gnome-xml/tree.h>
#include <time.h>

#include "ConfigArchiver.h"

xmlDocPtr location_client_load_rollback_data (ConfigArchiver_Location   location,
					      const struct tm          *date,
					      guint                     steps,
					      const gchar              *backend_id,
					      gboolean                  parent_chain,
					      CORBA_Environment        *opt_ev);

void      location_client_store_xml          (ConfigArchiver_Location   location, 
					      const gchar              *backend_id, 
					      xmlDocPtr                 xml_doc,
					      ConfigArchiver_StoreType  store_type,
					      CORBA_Environment        *opt_ev);

#endif /* __ARCHIVER_CLIENT_H */
