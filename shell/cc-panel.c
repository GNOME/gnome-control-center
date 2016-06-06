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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
    PROP_0,
    PROP_SHELL,
    PROP_PARAMETERS
};

G_DEFINE_ABSTRACT_TYPE (CcPanel, cc_panel, GTK_TYPE_BIN)

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
    case PROP_SHELL:
      /* construct only property */
      panel->priv->shell = g_value_get_object (value);
      break;

    case PROP_PARAMETERS:
      {
        GVariant *parameters = g_value_get_variant (value);
        GVariant *v;
        gsize n_parameters;

        if (parameters == NULL)
          return;

        n_parameters = g_variant_n_children (parameters);
        if (n_parameters == 0)
          return;

        g_variant_get_child (parameters, 0, "v", &v);

        if (!g_variant_is_of_type (v, G_VARIANT_TYPE_DICTIONARY))
          g_warning ("Wrong type for the first argument GVariant, expected 'a{sv}' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        else if (g_variant_n_children (v) > 0)
          g_warning ("Ignoring additional flags");

        g_variant_unref (v);

        if (n_parameters > 1)
          g_warning ("Ignoring additional parameters");

        break;
      }
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
    case PROP_SHELL:
      g_value_set_object (value, panel->priv->shell);
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
cc_panel_get_preferred_width (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum != NULL)
    *minimum = 0;

  if (natural != NULL)
    *natural = 0;

  if ((child = gtk_bin_get_child (bin)))
    gtk_widget_get_preferred_width (child, minimum, natural);
}

static void
cc_panel_get_preferred_height (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum != NULL)
    *minimum = 0;

  if (natural != NULL)
    *natural = 0;

  if ((child = gtk_bin_get_child (bin)))
    gtk_widget_get_preferred_height (child, minimum, natural);
}

static void
cc_panel_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  GtkAllocation child_allocation;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  child_allocation = *allocation;

  child = gtk_bin_get_child (GTK_BIN (widget));
  g_assert (child);
  gtk_widget_size_allocate (child, &child_allocation);
}

static void
cc_panel_class_init (CcPanelClass *klass)
{
  GParamSpec      *pspec;
  GObjectClass    *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass  *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_panel_get_property;
  object_class->set_property = cc_panel_set_property;
  object_class->finalize = cc_panel_finalize;

  widget_class->get_preferred_width = cc_panel_get_preferred_width;
  widget_class->get_preferred_height = cc_panel_get_preferred_height;
  widget_class->size_allocate = cc_panel_size_allocate;

  gtk_container_class_handle_border_width (GTK_CONTAINER_CLASS (klass));

  g_type_class_add_private (klass, sizeof (CcPanelPrivate));

  pspec = g_param_spec_object ("shell",
                               "Shell",
                               "Shell the Panel resides in",
                               CC_TYPE_SHELL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                               | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SHELL, pspec);

  pspec = g_param_spec_variant ("parameters",
                                "Structured parameters",
                                "Additional parameters passed externally (ie. command line, dbus activation)",
                                G_VARIANT_TYPE ("av"),
                                NULL,
                                G_PARAM_WRITABLE);
  g_object_class_install_property (object_class, PROP_PARAMETERS, pspec);
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

GPermission *
cc_panel_get_permission (CcPanel *panel)
{
  CcPanelClass *class = CC_PANEL_GET_CLASS (panel);

  if (class->get_permission)
    return class->get_permission (panel);

  return NULL;
}

const char *
cc_panel_get_help_uri (CcPanel *panel)
{
  CcPanelClass *class = CC_PANEL_GET_CLASS (panel);

  if (class->get_help_uri)
    return class->get_help_uri (panel);

  return NULL;
}

GtkWidget *
cc_panel_get_title_widget (CcPanel *panel)
{
  CcPanelClass *class = CC_PANEL_GET_CLASS (panel);

  if (class->get_title_widget)
    return class->get_title_widget (panel);

  return NULL;
}
