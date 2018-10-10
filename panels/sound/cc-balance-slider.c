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

#include "cc-sound-resources.h"
#include "cc-balance-slider.h"

struct _CcBalanceSlider
{
  GtkBox           parent_instance;

  GtkAdjustment   *adjustment;

  GvcChannelMap   *map;
  guint            volume_changed_handler;
};

G_DEFINE_TYPE (CcBalanceSlider, cc_balance_slider, GTK_TYPE_BOX)

static void
changed_cb (CcBalanceSlider *self)
{
}

static void
volume_changed_cb (CcBalanceSlider *self)
{
}

static void
cc_balance_slider_dispose (GObject *object)
{
  CcBalanceSlider *self = CC_BALANCE_SLIDER (object);

  g_clear_object (&self->map);

  G_OBJECT_CLASS (cc_balance_slider_parent_class)->dispose (object);
}

void
cc_balance_slider_class_init (CcBalanceSliderClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_balance_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-balance-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBalanceSlider, adjustment);

  gtk_widget_class_bind_template_callback (widget_class, changed_cb);
}

void
cc_balance_slider_init (CcBalanceSlider *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_balance_slider_set_channel_map (CcBalanceSlider *self,
                                   GvcChannelMap   *map)
{
  g_return_if_fail (CC_IS_BALANCE_SLIDER (self));

  if (self->map != NULL) {
    g_signal_handler_disconnect (self->map, self->volume_changed_handler);
    self->volume_changed_handler = 0;
  }
  g_clear_object (&self->map);

  if (map != NULL) {
    self->map = g_object_ref (map);

    self->volume_changed_handler = g_signal_connect_object (map,
                                                            "volume-changed",
                                                            G_CALLBACK (volume_changed_cb),
                                                            self, G_CONNECT_SWAPPED);
    volume_changed_cb (self);
  }
}
