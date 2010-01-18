/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <gconf/gconf-client.h>

#include "cc-display-panel.h"
#include "cc-display-page.h"

#define CC_DISPLAY_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PANEL, CcDisplayPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcDisplayPanelPrivate
{
        GtkWidget *notebook;
        CcPage    *display_page;
};

enum {
        PROP_0,
};

static void     cc_display_panel_class_init     (CcDisplayPanelClass *klass);
static void     cc_display_panel_init           (CcDisplayPanel      *display_panel);
static void     cc_display_panel_finalize       (GObject             *object);

G_DEFINE_DYNAMIC_TYPE (CcDisplayPanel, cc_display_panel, CC_TYPE_PANEL)

static void
cc_display_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_display_panel_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
setup_panel (CcDisplayPanel *panel)
{
        GtkWidget *label;
        char      *display_name;

        panel->priv->notebook = gtk_notebook_new ();
        gtk_container_add (GTK_CONTAINER (panel), panel->priv->notebook);
        gtk_widget_show (panel->priv->notebook);

        panel->priv->display_page = cc_display_page_new ();
        g_object_get (panel->priv->display_page,
                      "display-name", &display_name,
                      NULL);
        label = gtk_label_new (display_name);
        g_free (display_name);
        gtk_notebook_append_page (GTK_NOTEBOOK (panel->priv->notebook),
                                  GTK_WIDGET (panel->priv->display_page),
                                  label);
        gtk_widget_show (GTK_WIDGET (panel->priv->display_page));

        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (panel->priv->notebook),
                                    FALSE);

        g_object_set (panel,
                      "current-page", panel->priv->display_page,
                      NULL);
}

static GObject *
cc_display_panel_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
        CcDisplayPanel      *display_panel;

        display_panel = CC_DISPLAY_PANEL (G_OBJECT_CLASS (cc_display_panel_parent_class)->constructor (type,
                                                                                                          n_construct_properties,
                                                                                                          construct_properties));
        g_object_set (display_panel,
                      "display-name", _("Display"),
                      "id", "display-properties.desktop",
                      NULL);

        setup_panel (display_panel);

        return G_OBJECT (display_panel);
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_display_panel_get_property;
        object_class->set_property = cc_display_panel_set_property;
        object_class->constructor = cc_display_panel_constructor;
        object_class->finalize = cc_display_panel_finalize;

        g_type_class_add_private (klass, sizeof (CcDisplayPanelPrivate));
}

static void
cc_display_panel_class_finalize (CcDisplayPanelClass *klass)
{
}

static void
cc_display_panel_init (CcDisplayPanel *panel)
{
        panel->priv = CC_DISPLAY_PANEL_GET_PRIVATE (panel);
}

static void
cc_display_panel_finalize (GObject *object)
{
        CcDisplayPanel *display_panel;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_DISPLAY_PANEL (object));

        display_panel = CC_DISPLAY_PANEL (object);

        g_return_if_fail (display_panel->priv != NULL);

        G_OBJECT_CLASS (cc_display_panel_parent_class)->finalize (object);
}

void
cc_display_panel_register (GIOModule *module)
{
        cc_display_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_PANEL_EXTENSION_POINT_NAME,
                                        CC_TYPE_DISPLAY_PANEL,
                                        "display",
                                        10);
}
