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

typedef struct
{
  CcShell      *shell;
  GCancellable *cancellable;

  AdwNavigationView *navigation;
  gboolean           single_page_mode;

  GHashTable *subpages;
} CcPanelPrivate;

static GtkBuildableIface *parent_buildable_iface;
static void cc_panel_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcPanel, cc_panel, ADW_TYPE_NAVIGATION_PAGE,
                         G_ADD_PRIVATE (CcPanel) G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_panel_buildable_init))

enum
{
  PROP_0,
  PROP_SHELL,
  PROP_PARAMETERS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/* GtkBuildable interface */
static void
cc_panel_buildable_add_child (GtkBuildable *buildable,
                              GtkBuilder   *builder,
                              GObject      *child,
                              const gchar  *type)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (buildable));
  gboolean is_subpage_child = g_strcmp0 (type, "subpage") == 0;

  /* This is a hub panel (with subpages) such as System and Privacy. */
  if (ADW_IS_NAVIGATION_PAGE (child)) {
    if (!is_subpage_child) {
      g_warning ("<child type=\"subpage\" is expected for an AdwNavigationPage child widget");
      return;
    }
    adw_navigation_view_add (priv->navigation, ADW_NAVIGATION_PAGE (child));
  } else if (is_subpage_child) {
    g_warning ("<child type=\"subpage\" expects an AdwNavigationPage child widget");
    return;
  } else
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
set_subpage (CcPanel     *panel,
             const gchar *tag)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
  AdwNavigationPage *page;

  page = adw_navigation_view_find_page (priv->navigation, tag);
  if (!page)
    {
      if (g_hash_table_contains (priv->subpages, tag))
        {
          GType page_type = GPOINTER_TO_TYPE (g_hash_table_lookup (priv->subpages, tag));

          page = ADW_NAVIGATION_PAGE (g_object_new (page_type, NULL));
          adw_navigation_page_set_can_pop (page, !priv->single_page_mode);
          adw_navigation_view_add (priv->navigation, page);
        }
      else
        {
          g_warning ("Invalid subpage: '%s'", tag);
          return;
        }
     }

  adw_navigation_view_push_by_tag (priv->navigation, tag);
}

static void
navigation_push_cb (CcPanel     *panel,
                    const gchar *action_name,
                    GVariant    *params)
{
  set_subpage (panel, g_variant_get_string (params, NULL));
}

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

        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
          {
            set_subpage (CC_PANEL (object), g_variant_get_string (v, NULL));
          }
        else if (!g_variant_is_of_type (v, G_VARIANT_TYPE_DICTIONARY))
          g_warning ("Wrong type for the first argument GVariant, expected 'a{sv}' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        else if (g_variant_n_children (v) > 0)
          g_warning ("Ignoring additional flags");

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
  CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

  switch (prop_id)
    {
    case PROP_SHELL:
      g_value_set_object (value, priv->shell);
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
  g_clear_pointer (&priv->subpages, g_hash_table_unref);

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

  properties[PROP_SHELL] = g_param_spec_object ("shell",
                                                "Shell",
                                                "Shell the Panel resides in",
                                                CC_TYPE_SHELL,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PARAMETERS] = g_param_spec_variant ("parameters",
                                                      "Structured parameters",
                                                      "Additional parameters passed externally (ie. command line, D-Bus activation)",
                                                      G_VARIANT_TYPE ("av"),
                                                      NULL,
                                                      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class, "navigation.push", "s",
                                   (GtkWidgetActionActivateFunc) navigation_push_cb);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Settings/gtk/cc-panel.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcPanel, navigation);
}

static void
cc_panel_init (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  gtk_widget_init_template (GTK_WIDGET (panel));

  priv->subpages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
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
cc_panel_deactivate (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_cancellable_cancel (priv->cancellable);
}

void
cc_panel_add_subpage (CcPanel     *panel,
                      const gchar *page_tag,
                      AdwNavigationPage *subpage)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_if_fail (CC_IS_PANEL (panel));
  g_return_if_fail (ADW_IS_NAVIGATION_PAGE (subpage));

  adw_navigation_view_add (priv->navigation, subpage);
}

void
cc_panel_add_static_subpage (CcPanel     *panel,
                             const gchar *page_tag,
                             GType        page_type)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_if_fail (CC_IS_PANEL (panel));

  g_hash_table_insert (priv->subpages, g_strdup (page_tag), GTYPE_TO_POINTER (page_type));
}

void
cc_panel_push_subpage (CcPanel *panel,
                       AdwNavigationPage *subpage)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_if_fail (CC_IS_PANEL (panel));
  g_return_if_fail (ADW_IS_NAVIGATION_PAGE (subpage));

  adw_navigation_view_push (priv->navigation, subpage);
}

void
cc_panel_pop_visible_subpage (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_if_fail (CC_IS_PANEL (panel));

  adw_navigation_view_pop (priv->navigation);
}

AdwNavigationPage *
cc_panel_get_visible_subpage (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

  g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

  return adw_navigation_view_get_visible_page (priv->navigation);
}

void
cc_panel_enable_single_page_mode (CcPanel *panel)
{
  CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
  AdwNavigationPage *page;

  g_return_if_fail (CC_IS_PANEL (panel));

  priv->single_page_mode = TRUE;
  page = adw_navigation_view_get_visible_page (priv->navigation);
  if (page)
    adw_navigation_page_set_can_pop (page, FALSE);
}
