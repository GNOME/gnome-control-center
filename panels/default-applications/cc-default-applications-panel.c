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

#include "cc-default-applications-panel.h"

#include "gnome-da-capplet.h"
#include "gnome-da-xml.h"

G_DEFINE_DYNAMIC_TYPE (CcDefaultApplicationsPanel, cc_default_applications_panel, CC_TYPE_PANEL)

#define DEFAULT_APPLICATIONS_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DEFAULT_APPLICATIONS_PANEL, CcDefaultApplicationsPanelPrivate))

struct _CcDefaultApplicationsPanelPrivate
{
  GnomeDACapplet *capplet;
};


static void
cc_default_applications_panel_get_property (GObject    *object,
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
cc_default_applications_panel_set_property (GObject      *object,
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
cc_default_applications_panel_dispose (GObject *object)
{
  CcDefaultApplicationsPanelPrivate *priv;

  priv = CC_DEFAULT_APPLICATIONS_PANEL (object)->priv;

  if (priv->capplet)
    {
      g_object_unref (priv->capplet->terminal_settings);
      g_object_unref (priv->capplet->at_mobility_settings);
      g_object_unref (priv->capplet->at_visual_settings);

      if (priv->capplet->theme_changed_id > 0)
        {
          g_signal_handler_disconnect (priv->capplet->icon_theme,
                                       priv->capplet->theme_changed_id);
          priv->capplet->theme_changed_id = 0;
        }

      gnome_da_xml_free (priv->capplet);

      priv->capplet = NULL;
    }

  G_OBJECT_CLASS (cc_default_applications_panel_parent_class)->dispose (object);
}

static void
cc_default_applications_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_default_applications_panel_parent_class)->finalize (object);
}

static void
cc_default_applications_panel_class_init (CcDefaultApplicationsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDefaultApplicationsPanelPrivate));

  object_class->get_property = cc_default_applications_panel_get_property;
  object_class->set_property = cc_default_applications_panel_set_property;
  object_class->dispose = cc_default_applications_panel_dispose;
  object_class->finalize = cc_default_applications_panel_finalize;
}

static void
cc_default_applications_panel_class_finalize (CcDefaultApplicationsPanelClass *klass)
{
}

static void
cc_default_applications_panel_init (CcDefaultApplicationsPanel *self)
{
  CcDefaultApplicationsPanelPrivate *priv;
  GtkWidget *widget;

  priv = self->priv = DEFAULT_APPLICATIONS_PANEL_PRIVATE (self);

  priv->capplet = g_new0 (GnomeDACapplet, 1);
  priv->capplet->terminal_settings = g_settings_new ("org.gnome.desktop.default-applications.terminal");
  priv->capplet->at_mobility_settings = g_settings_new ("org.gnome.desktop.default-applications.at.mobility");
  priv->capplet->at_visual_settings = g_settings_new ("org.gnome.desktop.default-applications.at.visual");

  gnome_default_applications_panel_init (priv->capplet);

  widget = (GtkWidget *) gtk_builder_get_object (priv->capplet->builder,
                                                 "preferred_apps_notebook");

  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_default_applications_panel_register (GIOModule *module)
{
  cc_default_applications_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_DEFAULT_APPLICATIONS_PANEL,
                                  "default-applications", 0);
}

