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
#include "cc-window.h"

#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

typedef struct {
    CcWindow *window;
    GCancellable *cancellable;

    gboolean single_page_mode;

    GHashTable *subpages;
    GHashTable *static_subpages;

    GPtrArray *navigation_stack;
} CcPanelPrivate;

static GtkBuildableIface *parent_buildable_iface;
static void cc_panel_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcPanel, cc_panel, ADW_TYPE_NAVIGATION_PAGE,
                         G_ADD_PRIVATE (CcPanel) G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_panel_buildable_init))

enum {
    PROP_0,
    PROP_WINDOW,
    PROP_PARAMETERS,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* GtkBuildable interface */
static void
cc_panel_buildable_add_child (GtkBuildable *buildable, GtkBuilder *builder, GObject *child, const gchar *type)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (buildable));
    gboolean is_subpage_child = g_strcmp0 (type, "subpage") == 0;

    /* This is a hub panel (with subpages) such as System and Privacy. */
    if (ADW_IS_NAVIGATION_PAGE (child)) {
        AdwNavigationPage *page;
        const gchar *page_tag;

        if (!is_subpage_child) {
            g_warning ("<child type=\"subpage\" is expected for an AdwNavigationPage child widget");
            return;
        }

        page = ADW_NAVIGATION_PAGE (child);
        page_tag = adw_navigation_page_get_tag (page);

        g_hash_table_insert (priv->subpages, g_strdup (page_tag), g_object_ref (page));
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
set_subpage (CcPanel *panel, const gchar *tag)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
    AdwNavigationView *navigation;
    AdwNavigationPage *page;

    navigation = cc_window_get_navigation_view (priv->window);
    page = adw_navigation_view_find_page (navigation, tag);
    if (page == NULL) {
        page = g_hash_table_lookup (priv->subpages, tag);

        if (page == NULL) {
            page = cc_panel_get_static_subpage (panel, tag);

            if (page == NULL) {
                g_warning ("Invalid subpage: '%s'", tag);
                return;
            }
        }
    }

    adw_navigation_page_set_can_pop (page, !priv->single_page_mode);
    g_ptr_array_add (priv->navigation_stack, page);
}

static void
cc_panel_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

    switch (prop_id) {
    case PROP_WINDOW:
        /* construct only property */
        priv->window = g_value_get_object (value);
        break;

    case PROP_PARAMETERS: {
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

        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING)) {
            set_subpage (CC_PANEL (object), g_variant_get_string (v, NULL));
        } else if (!g_variant_is_of_type (v, G_VARIANT_TYPE_DICTIONARY))
            g_warning ("Wrong type for the first argument GVariant, expected 'a{sv}' but got '%s'",
                       (gchar *) g_variant_get_type (v));
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
cc_panel_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (CC_PANEL (object));

    switch (prop_id) {
    case PROP_WINDOW:
        g_value_set_object (value, priv->window);
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
    g_clear_pointer (&priv->static_subpages, g_hash_table_unref);
    g_clear_pointer (&priv->navigation_stack, g_ptr_array_unref);

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

    properties[PROP_WINDOW] = g_param_spec_object ("window", NULL, NULL, CC_TYPE_WINDOW,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_PARAMETERS] = g_param_spec_variant ("parameters", NULL, NULL, G_VARIANT_TYPE ("av"), NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Settings/gtk/cc-panel.ui");
}

static void
cc_panel_init (CcPanel *panel)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

    gtk_widget_init_template (GTK_WIDGET (panel));

    priv->subpages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    priv->static_subpages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    priv->navigation_stack = g_ptr_array_new ();
    g_ptr_array_add (priv->navigation_stack, ADW_NAVIGATION_PAGE (panel));
}

/**
 * cc_panel_get_toplevel:
 * @panel: A #CcPanel
 *
 * Get the toplevel window that the panel resides in.
 *
 * Returns: a #CcWindow
 */
CcWindow *
cc_panel_get_toplevel (CcPanel *panel)
{
    CcPanelPrivate *priv;

    g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

    priv = cc_panel_get_instance_private (panel);

    return priv->window;
}

const gchar *
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
cc_panel_add_subpage (CcPanel *panel, const gchar *page_tag, AdwNavigationPage *subpage)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

    g_return_if_fail (CC_IS_PANEL (panel));
    g_return_if_fail (ADW_IS_NAVIGATION_PAGE (subpage));

    g_hash_table_insert (priv->subpages, g_strdup (page_tag), g_object_ref (subpage));
}

void
cc_panel_add_static_subpage (CcPanel *panel, const gchar *page_tag, GType page_type)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

    g_return_if_fail (CC_IS_PANEL (panel));

    g_hash_table_insert (priv->static_subpages, g_strdup (page_tag), GTYPE_TO_POINTER (page_type));
}

void
cc_panel_push_subpage (CcPanel *panel, AdwNavigationPage *subpage)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
    AdwNavigationView *navigation;

    g_return_if_fail (CC_IS_PANEL (panel));
    g_return_if_fail (ADW_IS_NAVIGATION_PAGE (subpage));

    navigation = cc_window_get_navigation_view (priv->window);
    adw_navigation_view_push (navigation, subpage);
}

AdwNavigationPage *
cc_panel_get_visible_subpage (CcPanel *panel)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
    AdwNavigationView *navigation;

    g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

    navigation = cc_window_get_navigation_view (priv->window);
    return adw_navigation_view_get_visible_page (navigation);
}

void
cc_panel_enable_single_page_mode (CcPanel *panel)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
    AdwNavigationView *navigation;
    AdwNavigationPage *page;

    g_return_if_fail (CC_IS_PANEL (panel));

    priv->single_page_mode = TRUE;
    navigation = cc_window_get_navigation_view (priv->window);
    page = adw_navigation_view_get_visible_page (navigation);
    if (page)
        adw_navigation_page_set_can_pop (page, FALSE);
}

GList *
cc_panel_get_subpages (CcPanel *panel)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

    g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

    return g_hash_table_get_values (priv->subpages);
}

AdwNavigationPage *
cc_panel_get_static_subpage (CcPanel *panel, const gchar *tag)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);
    gpointer page_type_ptr;
    GType page_type;

    g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

    page_type_ptr = g_hash_table_lookup (priv->static_subpages, tag);
    if (page_type_ptr == NULL)
        return NULL;

    page_type = GPOINTER_TO_TYPE (page_type_ptr);

    if (g_type_is_a (page_type, CC_TYPE_PANEL))
        return g_object_new (page_type, "window", priv->window, NULL);

    return g_object_new (page_type, NULL);
}

GPtrArray *
cc_panel_get_navigation_stack (CcPanel *panel)
{
    CcPanelPrivate *priv = cc_panel_get_instance_private (panel);

    g_return_val_if_fail (CC_IS_PANEL (panel), NULL);

    return priv->navigation_stack;
}
