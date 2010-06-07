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

#include "cc-display-panel.h"

#include "xrandr-capplet.h"

G_DEFINE_DYNAMIC_TYPE (CcDisplayPanel, cc_display_panel, CC_TYPE_PANEL)

#define DISPLAY_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PANEL, CcDisplayPanelPrivate))

struct _CcDisplayPanelPrivate
{
  gpointer dummy;
};


static void
cc_display_panel_get_property (GObject    *object,
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
cc_display_panel_set_property (GObject      *object,
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
cc_display_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
cc_display_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_display_panel_parent_class)->finalize (object);
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDisplayPanelPrivate));

  object_class->get_property = cc_display_panel_get_property;
  object_class->set_property = cc_display_panel_set_property;
  object_class->dispose = cc_display_panel_dispose;
  object_class->finalize = cc_display_panel_finalize;
}

static void
cc_display_panel_class_finalize (CcDisplayPanelClass *klass)
{
}

static void
cc_display_panel_init (CcDisplayPanel *self)
{
  GtkWidget *widget;

  self->priv = DISPLAY_PANEL_PRIVATE (self);

  widget = run_application ();

  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (self), widget);
}

void
cc_display_panel_register (GIOModule *module)
{
  cc_display_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_DISPLAY_PANEL,
                                  "display", 0);
}

