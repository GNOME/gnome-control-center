/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 */

#include "cc-location-panel.h"

G_DEFINE_DYNAMIC_TYPE (CcLocationPanel, cc_location_panel, CC_TYPE_PANEL)

#define LOCATION_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_LOCATION_PANEL, CcLocationPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct _CcLocationPanelPrivate
{
  GtkBuilder *builder;
  /* that's where private vars go I guess */
};

static void
cc_location_panel_get_property (GObject    *object,
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
cc_location_panel_set_property (GObject      *object,
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
cc_location_panel_dispose (GObject *object)
{
  CcLocationPanelPrivate *priv = CC_LOCATION_PANEL (object)->priv;
  G_OBJECT_CLASS (cc_location_panel_parent_class)->dispose (object);
}

static void
cc_location_panel_finalize (GObject *object)
{
  CcLocationPanelPrivate *priv = CC_LOCATION_PANEL (object)->priv;
  G_OBJECT_CLASS (cc_location_panel_parent_class)->finalize (object);
}

static void
cc_location_panel_class_init (CcLocationPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcLocationPanelPrivate));

  object_class->get_property = cc_location_panel_get_property;
  object_class->set_property = cc_location_panel_set_property;
  object_class->dispose = cc_location_panel_dispose;
  object_class->finalize = cc_location_panel_finalize;
}

static void
cc_location_panel_class_finalize (CcLocationPanelClass *klass)
{
}

static void
cc_location_panel_init (CcLocationPanel *self)
{
  GError     *error;
  GtkWidget  *widget;

  self->priv = LOCATION_PANEL_PRIVATE (self);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/location.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  widget = WID ("location-vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_location_panel_register (GIOModule *module)
{
  cc_location_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_LOCATION_PANEL,
                                  "location", 0);
}
