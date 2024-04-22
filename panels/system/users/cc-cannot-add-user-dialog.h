/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 * Copyright 2024 GNOME Foundation Inc.
 *   Written by: Adrian Vovk
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_CANNOT_ADD_USER_DIALOG (cc_cannot_add_user_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcCannotAddUserDialog, cc_cannot_add_user_dialog, CC, CANNOT_ADD_USER_DIALOG, AdwDialog)

CcCannotAddUserDialog *cc_cannot_add_user_dialog_new (void);

G_END_DECLS
