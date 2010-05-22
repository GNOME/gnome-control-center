/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2002 Sun Microsystems Inc.
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
 *         Mark McLoughlin <mark@skynet.ie>
 *
 */

#include "cc-network-panel.h"

#include <gconf/gconf-client.h>

G_DEFINE_DYNAMIC_TYPE (CcNetworkPanel, cc_network_panel, CC_TYPE_PANEL)

#define NETWORK_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PANEL, CcNetworkPanelPrivate))

struct _CcNetworkPanelPrivate
{
  GtkBuilder *builder;
  GConfClient *client;
};


static void
cc_network_panel_get_property (GObject    *object,
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
cc_network_panel_set_property (GObject      *object,
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
cc_network_panel_dispose (GObject *object)
{
  CcNetworkPanelPrivate *priv = CC_NETWORK_PANEL (object)->priv;

  if (!priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (!priv->client)
    {
      g_object_unref (priv->client);
      priv->client = NULL;
    }

  G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcNetworkPanelPrivate));

  object_class->get_property = cc_network_panel_get_property;
  object_class->set_property = cc_network_panel_set_property;
  object_class->dispose = cc_network_panel_dispose;
  object_class->finalize = cc_network_panel_finalize;
}

static void
cc_network_panel_class_finalize (CcNetworkPanelClass *klass)
{
}

static void
cc_network_panel_init (CcNetworkPanel *self)
{
  CcNetworkPanelPrivate *priv;
  GtkWidget *widget;

  priv = self->priv = NETWORK_PANEL_PRIVATE (self);


  priv->builder = gtk_builder_new ();
  priv->client = gconf_client_get_default ();

  gnome_network_properties_init (priv->builder, priv->client);

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
                                                 "network-panel");
  gtk_widget_reparent (widget, GTK_WIDGET (self));
}

void
cc_network_panel_register (GIOModule *module)
{
  cc_network_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_NETWORK_PANEL,
                                  "gnome-network-properties.desktop", 0);
}

