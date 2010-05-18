/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Authors: William Jon McCann <jmccann@redhat.com>
 *          Thomas Wood <thomas.wood@intel.com>
 *
 */

/**
 * SECTION:cc-panel
 * @short_description: An abstract class for Control Center panels
 *
 * CcPanel is an abstract class used to implement panels for the shell. A
 * panel contains a collection of related settings that are displayed within
 * the shell window.
 */

#include "config.h"

#include "cc-panel.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

#define CC_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PANEL, CcPanelPrivate))

struct CcPanelPrivate
{
  gchar    *id;
  gchar    *display_name;
  gchar    *category;
  gchar    *current_location;

  gboolean  is_active;
  CcShell  *shell;
};

enum
{
    PROP_ID = 1,
    PROP_DISPLAY_NAME,
    PROP_SHELL,
    PROP_ACTIVE
};

G_DEFINE_ABSTRACT_TYPE (CcPanel, cc_panel, GTK_TYPE_ALIGNMENT)

static void
cc_panel_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  CcPanel *panel;

  panel = CC_PANEL (object);

  switch (prop_id)
    {
    case PROP_ID:
      /* construct only property */
      g_free (panel->priv->id);
      panel->priv->id = g_value_dup_string (value);
      break;

    case PROP_DISPLAY_NAME:
      cc_panel_set_display_name (panel, g_value_get_string (value));
      break;

    case PROP_SHELL:
      /* construct only property */
      panel->priv->shell = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_panel_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  CcPanel *panel;

  panel = CC_PANEL (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, panel->priv->id);
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, panel->priv->display_name);
      break;

    case PROP_SHELL:
      g_value_set_object (value, panel->priv->shell);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, panel->priv->is_active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_panel_finalize (GObject *object)
{
  CcPanel *panel;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_PANEL (object));

  panel = CC_PANEL (object);

  g_free (panel->priv->id);
  g_free (panel->priv->display_name);

  G_OBJECT_CLASS (cc_panel_parent_class)->finalize (object);
}


static void
cc_panel_class_init (CcPanelClass *klass)
{
  GParamSpec      *pspec;
  GObjectClass    *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_panel_get_property;
  object_class->set_property = cc_panel_set_property;
  object_class->finalize = cc_panel_finalize;

  g_type_class_add_private (klass, sizeof (CcPanelPrivate));

  pspec = g_param_spec_string ("id",
                               "id",
                               "Unique id of the Panel",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_ID, pspec );

  pspec = g_param_spec_string ("display-name",
                               "display name",
                               "Display name of the Panel",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

  pspec = g_param_spec_object ("shell",
                               "Shell",
                               "Shell the Panel resides in",
                               CC_TYPE_SHELL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                               | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SHELL, pspec);

  pspec = g_param_spec_boolean ("active",
                                "Active",
                                "Whether the panel is currently the active"
                                " panel of the shell",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACTIVE, pspec);
}

static void
cc_panel_init (CcPanel *panel)
{
  panel->priv = CC_PANEL_GET_PRIVATE (panel);
}

/**
 * cc_panel_get_shell:
 * @panel: A #CcPanel
 *
 * Get the shell that the panel resides in
 *
 * Returns: a #CcShell
 */
CcShell *
cc_panel_get_shell (CcPanel *panel)
{
  return panel->priv->shell;
}

/**
 * cc_panel_get_id:
 * @panel: A #CcPanel
 *
 * Get the value of the #CcPanel:id property
 *
 * Returns: value of the id property, owned by the panel
 */
const gchar*
cc_panel_get_id (CcPanel *panel)
{
  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  return panel->priv->id;
}


/**
 * cc_panel_get_active:
 * @panel: A #CcPanel
 *
 * Get the current value of the #CcPanel:active property
 *
 * Returns: #TRUE if the panel is marked as active
 */
gboolean
cc_panel_get_active (CcPanel *panel)
{
  g_return_val_if_fail (CC_IS_PANEL (panel), FALSE);

  return panel->priv->is_active;
}

/**
 * cc_panel_set_active:
 * @panel: A #CcPanel
 * @is_active: #TRUE if the panel is now active
 *
 * Mark the panel as active. This should only be called by CcShell
 * implementations.
 *
 */
void
cc_panel_set_active (CcPanel  *panel,
                     gboolean  is_active)
{
  g_return_if_fail (CC_IS_PANEL (panel));

  if (panel->priv->is_active != is_active)
    {
      gtk_widget_queue_resize (GTK_WIDGET (panel));

      g_object_notify (G_OBJECT (panel), "active");
    }
}

/**
 * cc_panel_set_display_name:
 * @panel: A #CcPanel
 * @display_name: Display name of the panel
 *
 * Set the value of the #CcPanel:display-name property.
 *
 */
void
cc_panel_set_display_name (CcPanel     *panel,
                           const gchar *display_name)
{
  g_return_if_fail (CC_IS_PANEL (panel));
  g_return_if_fail (display_name != NULL);

  g_free (panel->priv->display_name);
  panel->priv->display_name = g_strdup (display_name);
}

/**
 * cc_panel_get_display_name:
 * @panel: A #CcPanel
 *
 * Get the value of the #CcPanel:display-name property.
 *
 * Returns: the display name, owned by the panel
 */
const gchar*
cc_panel_get_display_name (CcPanel *panel)
{
  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  return panel->priv->display_name;
}
