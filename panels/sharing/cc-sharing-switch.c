/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "cc-sharing-switch.h"

struct _CcSharingSwitch {
  GtkSwitch  parent_instance;

  GtkWidget *widget;
};

G_DEFINE_TYPE (CcSharingSwitch, cc_sharing_switch, GTK_TYPE_SWITCH)

enum {
  PROP_0,
  PROP_WIDGET
};

static void     cc_sharing_switch_class_init     (CcSharingSwitchClass *klass);
static void     cc_sharing_switch_init           (CcSharingSwitch      *self);
static void     cc_sharing_switch_finalize       (GObject                *object);

static void
cc_sharing_switch_constructed (GObject *object)
{
  CcSharingSwitch *self;
  GtkWidget *other_sw;

  G_OBJECT_CLASS (cc_sharing_switch_parent_class)->constructed (object);

  self = CC_SHARING_SWITCH (object);

  other_sw = g_object_get_data (G_OBJECT (self->widget), "switch");

  g_object_bind_property (other_sw, "visible", self, "visible", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (other_sw, "state", self, "state", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (other_sw, "active", self, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (other_sw, "sensitive", self, "sensitive", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
}

static void
cc_sharing_switch_init (CcSharingSwitch *self)
{
}

GtkWidget *
cc_sharing_switch_new (GtkWidget *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);

  return GTK_WIDGET (g_object_new (CC_TYPE_SHARING_SWITCH,
				   "widget", widget,
				   NULL));
}

static void
cc_sharing_switch_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  CcSharingSwitch *self;

  self = CC_SHARING_SWITCH (object);

  switch (prop_id) {
  case PROP_WIDGET:
    self->widget = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_sharing_switch_finalize (GObject *object)
{
  CcSharingSwitch *self;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_SHARING_SWITCH (object));

  self = CC_SHARING_SWITCH (object);

  g_return_if_fail (self != NULL);

  g_clear_object (&self->widget);

  G_OBJECT_CLASS (cc_sharing_switch_parent_class)->finalize (object);
}

static void
cc_sharing_switch_class_init (CcSharingSwitchClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_sharing_switch_set_property;
  object_class->finalize = cc_sharing_switch_finalize;
  object_class->constructed = cc_sharing_switch_constructed;

  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        "widget",
                                                        "widget",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
