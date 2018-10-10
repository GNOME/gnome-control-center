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

#include "cc-level-bar.h"
#include "gvc-level-bar.h"

struct _CcLevelBar
{
  GtkBox          parent_instance;

  GtkWidget      *legacy_level_bar;

  GvcMixerStream *stream;
};

G_DEFINE_TYPE (CcLevelBar, cc_level_bar, GTK_TYPE_BOX)

static void
cc_level_bar_dispose (GObject *object)
{
  CcLevelBar *self = CC_LEVEL_BAR (object);

  g_clear_object (&self->stream);

  G_OBJECT_CLASS (cc_level_bar_parent_class)->dispose (object);
}

void
cc_level_bar_class_init (CcLevelBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_level_bar_dispose;
}

void
cc_level_bar_init (CcLevelBar *self)
{
  self->legacy_level_bar = gvc_level_bar_new ();
  gtk_widget_show (self->legacy_level_bar);
  gtk_widget_set_hexpand (self->legacy_level_bar, TRUE);
  gtk_container_add (GTK_CONTAINER (self), self->legacy_level_bar);
}

void
cc_level_bar_set_stream (CcLevelBar *self,
                         GvcMixerStream *stream)
{
  g_return_if_fail (CC_IS_LEVEL_BAR (self));

  // FIXME: Disconnect signals
  g_clear_object (&self->stream);

  self->stream = g_object_ref (stream);
}
