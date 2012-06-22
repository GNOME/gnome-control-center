/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors: Ray Strode
 */

#ifndef __GSD_IDENTITY_MANAGER_PRIVATE_H__
#define __GSD_IDENTITY_MANAGER_PRIVATE_H__

#include <glib.h>
#include <glib-object.h>

#include "gsd-identity-manager.h"

G_BEGIN_DECLS

void      _gsd_identity_manager_emit_identity_added (GsdIdentityManager *identity_manager,
                                                     GsdIdentity        *identity);
void      _gsd_identity_manager_emit_identity_removed (GsdIdentityManager *identity_manager,
                                                       GsdIdentity        *identity);
void      _gsd_identity_manager_emit_identity_refreshed (GsdIdentityManager *identity_manager,
                                                         GsdIdentity        *identity);
void      _gsd_identity_manager_emit_identity_renamed (GsdIdentityManager *identity_manager,
                                                       GsdIdentity        *identity);

void      _gsd_identity_manager_emit_identity_expiring (GsdIdentityManager *self,
                                                        GsdIdentity        *identity);

void      _gsd_identity_manager_emit_identity_needs_renewal (GsdIdentityManager *identity_manager,
                                                             GsdIdentity        *identity);
void      _gsd_identity_manager_emit_identity_expired (GsdIdentityManager *identity_manager,
                                                       GsdIdentity        *identity);
G_END_DECLS

#endif /* __GSD_IDENTITY_MANAGER_PRIVATE_H__ */
