/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_DURATION_ROW (cc_duration_row_get_type())
G_DECLARE_FINAL_TYPE (CcDurationRow, cc_duration_row, CC, DURATION_ROW, AdwActionRow)

CcDurationRow *cc_duration_row_new (void);

guint cc_duration_row_get_duration (CcDurationRow *self);
void cc_duration_row_set_duration (CcDurationRow *self,
                                   guint          duration);

guint cc_duration_row_get_minimum (CcDurationRow *self);
void cc_duration_row_set_minimum (CcDurationRow *self,
                                  guint          minimum);

guint cc_duration_row_get_maximum (CcDurationRow *self);
void cc_duration_row_set_maximum (CcDurationRow *self,
                                  guint          maximum);

G_END_DECLS
