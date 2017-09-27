/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
 */

#ifndef __GVC_SPEAKER_TEST_H
#define __GVC_SPEAKER_TEST_H

#include <glib-object.h>
#include <gvc-mixer-card.h>
#include <gvc-mixer-control.h>

G_BEGIN_DECLS

#define GVC_TYPE_SPEAKER_TEST (gvc_speaker_test_get_type ())
G_DECLARE_FINAL_TYPE (GvcSpeakerTest, gvc_speaker_test, GVC, SPEAKER_TEST, GtkGrid)

GtkWidget *         gvc_speaker_test_new                 (GvcMixerControl *control,
                                                          GvcMixerStream  *stream);

G_END_DECLS

#endif /* __GVC_SPEAKER_TEST_H */
