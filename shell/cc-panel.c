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
 *
 * # Buildable
 *
 * CcPanel implements the GtkBuildable interface, and allows having different
 * types of children for convenience.
 *
 * It is possible to add widgets to the start and end of the panel titlebar
 * using, respectively, the `titlebar-start` and `titlebar-end` child types.
 * It is also possible to override the titlebar entirely with a custom titlebar
 * using the `titlebar` child type.
 *
 * Most panels will use the `content` child type, which sets the panel content
 * beneath the titlebar.
 *
 * At last, it is possible to override all custom CcPanel widgets by not setting
 * any child type.
 */

#include "config.h"

#include "cc-panel-private.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct
{
  AdwBin       *content_bin;
  GtkBox       *main_box;
  AdwBin       *titlebar_bin;
  AdwHeaderBar *titlebar;

  CcShell      *shell;
  GCancellable *cancellable;
  gboolean      folded;
  gchar        *title;
} CcPanelPrivate;

static void cc_panel_buildable_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (CcPanel, cc_panel, ADW_TYPE_BIN,
                                  G_ADD_PRIVATE (CcPanel)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_panel_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

enum
{
  SIDEBAR_ACTIVATED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SHELL,
  PROP_PARAMETERS,
  PROP_FOLDED,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static guint signals [LAST_SIGNAL] = { 0 };

/* GtkBuildable interface */

static void
cc_panel_buildable_add_child (GtkBuildable *buildable,
                              GtkBuilder   *builder,
                              GObject      *child,
                              const char   *type)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (buildable));

  if (GTK_IS_WIDGET (child) && !priv->main_box)
    {
      adw_bin_set_child (ADW_BIN (buildable), GTK_WIDGET (child));
      return;
    }

  if (g_strcmp0 (type, "content") == 0)
    adw_bin_set_child (priv->content_bin, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "titlebar-start") == 0)
    adw_header_bar_pack_start (priv->titlebar, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "titlebar-end") == 0)
    adw_header_bar_pack_end (priv->titlebar, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "titlebar") == 0)
    adw_bin_set_child (priv->titlebar_bin, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
cc_panel_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = cc_panel_buildable_add_child;
}

/* GObject overrides */

static void
cc_panel_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

  switch (prop_id)
    {
    case PROP_SHELL:
      /* construct only property */
      priv->shell = g_value_get_object (value);
      break;

    case PROP_PARAMETERS:
      {
        g_autoptr(GVariant) v = NULL;
        GVariant *parameters;
        gsize n_parameters;

        parameters = g_value_get_variant (value);

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

        if (n_parameters > 1)
          g_warning ("Ignoring additional parameters");

        break;
      }

    case PROP_TITLE:
      priv->title = g_value_dup_string (value);
      break;

    case PROP_FOLDED:
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
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

  switch (prop_id)
    {
    case PROP_SHELL:
      g_value_set_object (value, priv->shell);
      break;

    case PROP_FOLDED:
      g_value_set_boolean (value, priv->folded);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_panel_finalize (GObject *object)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (cc_panel_parent_class)->finalize (object);
}

static void
cc_panel_class_init (CcPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_panel_get_property;
  object_class->set_property = cc_panel_set_property;
  object_class->finalize = cc_panel_finalize;

  signals[SIDEBAR_ACTIVATED] = g_signal_new ("sidebar-activated",
                                             G_TYPE_FROM_CLASS (object_class),
                                             G_SIGNAL_RUN_LAST,
                                             0, NULL, NULL,
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE, 0);

  properties[PROP_SHELL] = g_param_spec_object ("shell",
                                                "Shell",
                                                "Shell the Panel resides in",
                                                CC_TYPE_SHELL,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FOLDED] = g_param_spec_boolean ("folded", NULL, NULL,
                                                  FALSE,
                                                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PARAMETERS] = g_param_spec_variant ("parameters",
                                                      "Structured parameters",
                                                      "Additional parameters passed externally (ie. command line, D-Bus activation)",
                                                      G_VARIANT_TYPE ("av"),
                                                      NULL,
                                                      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_TITLE] = g_param_spec_string ("title", NULL, NULL, NULL,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Settings/gtk/cc-panel.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcPanel, content_bin);
  gtk_widget_class_bind_template_child_private (widget_class, CcPanel, main_box);
  gtk_widget_class_bind_template_child_private (widget_class, CcPanel, titlebar_bin);
  gtk_widget_class_bind_template_child_private (widget_class, CcPanel, titlebar);
}

static void
cc_panel_init (CcPanel *panel)
{
  gtk_widget_init_template (GTK_WIDGET (panel));
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
  CcPanelPrivate *priv;

  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  priv = cc_panel_get_instance_private (panel);

  return priv->shell;
}

const gchar*
cc_panel_get_help_uri (CcPanel *panel)
{
  CcPanelClass *class = CC_PANEL_GET_CLASS (panel);

  if (class->get_help_uri)
    return class->get_help_uri (panel);

  return NULL;
}

GtkWidget*
cc_panel_get_sidebar_widget (CcPanel *panel)
{
  CcPanelClass *class = CC_PANEL_GET_CLASS (panel);

  if (class->get_sidebar_widget)
    {
      GtkWidget *sidebar_widget;

      sidebar_widget = class->get_sidebar_widget (panel);
      g_assert (sidebar_widget != NULL);

      return sidebar_widget;
    }

  return NULL;
}

GCancellable *
cc_panel_get_cancellable (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();

  return priv->cancellable;
}

void
cc_panel_set_folded (CcPanel  *panel,
                     gboolean  folded)
{
  CcPanelPrivate *priv;

  g_return_if_fail (CC_IS_PANEL (panel));

  priv = cc_panel_get_instance_private (panel);

  if (priv->folded != folded)
    {
      g_debug ("Panel %s folded: %s",
               G_OBJECT_TYPE_NAME (panel),
               folded ? "yes" : "no");

      priv->folded = folded;
      g_object_notify_by_pspec (G_OBJECT (panel), properties[PROP_FOLDED]);
    }
}

gboolean
cc_panel_get_folded (CcPanel *panel)
{
  CcPanelPrivate *priv;

  g_return_val_if_fail (CC_IS_PANEL (panel), FALSE);

  priv = cc_panel_get_instance_private (panel);
  return priv->folded;
}

GtkWidget*
cc_panel_get_content (CcPanel *panel)
{
  CcPanelPrivate *priv;

  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  priv = cc_panel_get_instance_private (panel);
  return adw_bin_get_child (priv->content_bin);
}

void
cc_panel_set_content (CcPanel   *panel,
                      GtkWidget *content)
{
  CcPanelPrivate *priv;

  g_return_if_fail (CC_IS_PANEL (panel));

  priv = cc_panel_get_instance_private (panel);
  adw_bin_set_child (priv->content_bin, content);
}

GtkWidget*
cc_panel_get_titlebar (CcPanel *panel)
{
  CcPanelPrivate *priv;

  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  priv = cc_panel_get_instance_private (panel);
  return adw_bin_get_child (priv->titlebar_bin);
}

void
cc_panel_set_titlebar (CcPanel   *panel,
                       GtkWidget *titlebar)
{
  CcPanelPrivate *priv;

  g_return_if_fail (CC_IS_PANEL (panel));

  priv = cc_panel_get_instance_private (panel);
  adw_bin_set_child (priv->titlebar_bin, titlebar);
}

void
cc_panel_deactivate (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_cancellable_cancel (priv->cancellable);
}
