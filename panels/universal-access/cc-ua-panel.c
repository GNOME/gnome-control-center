/*
 * Copyright (C) 2010 Intel, Inc
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-ua-panel.h"

G_DEFINE_DYNAMIC_TYPE (CcUaPanel, cc_ua_panel, CC_TYPE_PANEL)

#define UA_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_UA_PANEL, CcUaPanelPrivate))

struct _CcUaPanelPrivate
{
  GtkBuilder *builder;
};


static void
cc_ua_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_ua_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanelPrivate *priv = CC_UA_PANEL (object)->priv;
  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }
  G_OBJECT_CLASS (cc_ua_panel_parent_class)->dispose (object);
}

static void
cc_ua_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_ua_panel_parent_class)->finalize (object);
}

static void
cc_ua_panel_class_init (CcUaPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcUaPanelPrivate));

  object_class->get_property = cc_ua_panel_get_property;
  object_class->set_property = cc_ua_panel_set_property;
  object_class->dispose = cc_ua_panel_dispose;
  object_class->finalize = cc_ua_panel_finalize;
}

static void
cc_ua_panel_class_finalize (CcUaPanelClass *klass)
{
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  CcUaPanelPrivate *priv;
  GtkWidget *widget;
  GError *err = NULL;
  gchar *objects[] = { "universal_access_box", "contrast_model",
                       "text_size_model", "slowkeys_delay_adjustment",
                       "bouncekeys_delay_adjustment", "click_delay_adjustment",
                       "dwell_time_adjustment", "dwell_threshold_adjustment" };

  priv = self->priv = UA_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  gtk_builder_add_objects_from_file (priv->builder,
                                     GNOMECC_DATA_DIR "/ui/uap.ui",
                                     objects,
                                     &err);

  if (err)
    {
      g_warning ("Could not load interface file: %s", err->message);
      g_error_free (err);

      g_object_unref (priv->builder);
      priv->builder = NULL;

      return;
    }

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "universal_access_box");

  gtk_container_add (GTK_CONTAINER (self), widget);
}

void
cc_ua_panel_register (GIOModule *module)
{
  cc_ua_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_UA_PANEL,
                                  "gnome-universal-access.desktop", 0);
}

