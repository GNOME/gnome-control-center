/* -*- mode: c; c-file-style: "gnu"; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-speaker-test-row.h
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

#include <gsound.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define CC_TYPE_SPEAKER_TEST_ROW (cc_speaker_test_row_get_type ())
G_DECLARE_FINAL_TYPE (CcSpeakerTestRow, cc_speaker_test_row, CC, SPEAKER_TEST_ROW, GtkListBoxRow)

void cc_speaker_test_row_set_channel_position (CcSpeakerTestRow     *self,
                                               GSoundContext        *context,
                                               pa_channel_position_t position);

G_END_DECLS
