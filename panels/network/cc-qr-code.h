/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-qr-code.h
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <cairo.h>

G_BEGIN_DECLS

#define CC_TYPE_QR_CODE (cc_qr_code_get_type ())

G_DECLARE_FINAL_TYPE (CcQrCode, cc_qr_code, CC, QR_CODE, GObject)

CcQrCode        *cc_qr_code_new         (void);
gboolean         cc_qr_code_set_text    (CcQrCode    *self,
                                         const gchar *text);
cairo_surface_t *cc_qr_code_get_surface (CcQrCode    *self,
                                         gint         size,
                                         gint         scale);

G_END_DECLS
