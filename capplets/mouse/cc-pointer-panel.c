/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2010 Intel, Inc.
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

#include "cc-pointer-panel.h"
#include <glib/gi18n.h>

G_DEFINE_DYNAMIC_TYPE (CcPointerPanel, cc_pointer_panel, CC_TYPE_PANEL)

#define POINTER_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POINTER_PANEL, CcPointerPanelPrivate))

struct _CcPointerPanelPrivate
{
  gpointer dummy;
};


static void
cc_pointer_panel_get_property (GObject    *object,
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
cc_pointer_panel_set_property (GObject      *object,
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
cc_pointer_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_pointer_panel_parent_class)->dispose (object);
}

static void
cc_pointer_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_pointer_panel_parent_class)->finalize (object);
}

/* this is defined in gnome-mouse-properties.c */
GtkBuilder * create_dialog (void);
/* TODO: split out the pages into seperate objects */

static GObject*
cc_pointer_panel_constructor (GType                  type,
                              guint                  n_properties,
                              GObjectConstructParam *properties)
{
  CcPointerPanel *panel;
  GObjectClass *parent_class;

  parent_class = G_OBJECT_CLASS (cc_pointer_panel_parent_class);

  panel = (CcPointerPanel*) parent_class->constructor (type, n_properties,
                                                       properties);

  g_object_set (panel,
                "id", "gnome-settings-mouse.desktop",
                "display-name", _("Pointing Devices"),
                NULL);

  return (GObject *) panel;
}

static void
cc_pointer_panel_class_init (CcPointerPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcPointerPanelPrivate));

  object_class->get_property = cc_pointer_panel_get_property;
  object_class->set_property = cc_pointer_panel_set_property;
  object_class->dispose = cc_pointer_panel_dispose;
  object_class->finalize = cc_pointer_panel_finalize;
  object_class->constructor = cc_pointer_panel_constructor;
}

static void
cc_pointer_panel_class_finalize (CcPointerPanelClass *klass)
{
}

static void
cc_pointer_panel_init (CcPointerPanel *self)
{
  GtkBuilder *builder;
  GtkWidget *prefs_widget;

  self->priv = POINTER_PANEL_PRIVATE (self);


  builder = create_dialog ();

  prefs_widget = (GtkWidget*) gtk_builder_get_object (builder, "prefs_widget");

  gtk_widget_reparent (prefs_widget, GTK_WIDGET (self));
}

CcPointerPanel *
cc_pointer_panel_new (void)
{
  return g_object_new (CC_TYPE_POINTER_PANEL, NULL);
}


void
cc_pointer_panel_register (GIOModule *module)
{
  cc_pointer_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_PANEL_EXTENSION_POINT_NAME,
                                  CC_TYPE_POINTER_PANEL,
                                  "pointer",
                                  10);
}
