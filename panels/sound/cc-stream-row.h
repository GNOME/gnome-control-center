/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <gvc-mixer-stream.h>

#include "cc-sound-enums.h"

G_BEGIN_DECLS

#define CC_TYPE_STREAM_ROW (cc_stream_row_get_type ())
G_DECLARE_FINAL_TYPE (CcStreamRow, cc_stream_row, CC, STREAM_ROW, GtkListBoxRow)

CcStreamRow     *cc_stream_row_new       (GtkSizeGroup    *size_group,
                                          GvcMixerStream  *stream,
                                          guint            id,
                                          CcStreamType     stream_type,
                                          GvcMixerControl *mixer_control);

GvcMixerStream *cc_stream_row_get_stream (CcStreamRow    *row);

guint           cc_stream_row_get_id     (CcStreamRow    *row);

G_END_DECLS
