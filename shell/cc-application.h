/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#pragma once

#include "cc-shell-model.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_APPLICATION (cc_application_get_type())

G_DECLARE_FINAL_TYPE (CcApplication, cc_application, CC, APPLICATION, GtkApplication)

GtkApplication        *cc_application_new                    (void);

CcShellModel          *cc_application_get_model              (CcApplication *self);

G_END_DECLS
