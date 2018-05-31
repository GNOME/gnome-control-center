/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2006 Jonh Wendell <wendell@bani.com.br> 
 * Copyright © 2010 Codethink Limited
 * Copyright (C) 2013 Intel, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      Jonh Wendell <wendell@bani.com.br>
 *      Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

GVariant * vino_set_authtype (const GValue       *value,
                              const GVariantType *type,
                              gpointer            user_data);
gboolean vino_get_authtype (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data);

GVariant *
vino_set_password (const GValue       *value,
                   const GVariantType *type,
                   gpointer            user_data);
gboolean
vino_get_password (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data);
