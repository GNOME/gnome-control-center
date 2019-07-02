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

#include "cc-sound-enums.h"

G_BEGIN_DECLS

#define CC_TYPE_STREAM_LIST_BOX (cc_stream_list_box_get_type ())
G_DECLARE_FINAL_TYPE (CcStreamListBox, cc_stream_list_box, CC, STREAM_LIST_BOX, GtkListBox)

void cc_stream_list_box_set_mixer_control (CcStreamListBox *combo_box,
                                           GvcMixerControl *mixer_control);

void cc_stream_list_box_set_stream_type   (CcStreamListBox *combo_box,
                                           CcStreamType     type);

G_END_DECLS
