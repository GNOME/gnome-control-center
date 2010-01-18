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

#include "cc-keyboard-panel.h"
#include "cc-keyboard-page.h"
#include "cc-shortcuts-page.h"

#define CC_KEYBOARD_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_KEYBOARD_PANEL, CcKeyboardPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcKeyboardPanelPrivate
{
        GtkWidget *notebook;
        CcPage    *keyboard_page;
        CcPage    *shortcuts_page;
};

enum {
        PROP_0,
};

static void     cc_keyboard_panel_class_init     (CcKeyboardPanelClass *klass);
static void     cc_keyboard_panel_init           (CcKeyboardPanel      *keyboard_panel);
static void     cc_keyboard_panel_finalize       (GObject              *object);

G_DEFINE_DYNAMIC_TYPE (CcKeyboardPanel, cc_keyboard_panel, CC_TYPE_PANEL)

static void
cc_keyboard_panel_set_property (GObject      *object,
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
cc_keyboard_panel_get_property (GObject    *object,
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
on_notebook_switch_page (GtkNotebook     *notebook,
                         GtkNotebookPage *page,
                         guint            page_num,
                         CcKeyboardPanel *panel)
{
        if (page_num == 0) {
                g_object_set (panel,
                              "current-page",
                              panel->priv->keyboard_page,
                              NULL);
        } else {
                g_object_set (panel,
                              "current-page",
                              panel->priv->shortcuts_page,
                              NULL);
        }
}

static void
setup_panel (CcKeyboardPanel *panel)
{
        GtkWidget *label;
        char      *display_name;

        panel->priv->notebook = gtk_notebook_new ();
        g_signal_connect (panel->priv->notebook,
                          "switch-page",
                          G_CALLBACK (on_notebook_switch_page),
                          panel);

        gtk_container_add (GTK_CONTAINER (panel), panel->priv->notebook);
        gtk_widget_show (panel->priv->notebook);

        panel->priv->keyboard_page = cc_keyboard_page_new ();
        g_object_get (panel->priv->keyboard_page,
                      "display-name", &display_name,
                      NULL);
        label = gtk_label_new (display_name);
        g_free (display_name);
        gtk_notebook_append_page (GTK_NOTEBOOK (panel->priv->notebook),
                                  GTK_WIDGET (panel->priv->keyboard_page),
                                  label);
        gtk_widget_show (GTK_WIDGET (panel->priv->keyboard_page));


        panel->priv->shortcuts_page = cc_shortcuts_page_new ();
        g_object_get (panel->priv->shortcuts_page,
                      "display-name", &display_name,
                      NULL);
        label = gtk_label_new (display_name);
        g_free (display_name);
        gtk_notebook_append_page (GTK_NOTEBOOK (panel->priv->notebook),
                                  GTK_WIDGET (panel->priv->shortcuts_page),
                                  label);
        gtk_widget_show (GTK_WIDGET (panel->priv->shortcuts_page));

        g_object_set (panel,
                      "current-page", panel->priv->keyboard_page,
                      NULL);
}

static GObject *
cc_keyboard_panel_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
        CcKeyboardPanel      *keyboard_panel;

        keyboard_panel = CC_KEYBOARD_PANEL (G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructor (type,
                                                                                                          n_construct_properties,
                                                                                                          construct_properties));
        g_object_set (keyboard_panel,
                      "display-name", _("Keyboard"),
                      "id", "keyboard.desktop",
                      NULL);

        setup_panel (keyboard_panel);

        return G_OBJECT (keyboard_panel);
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_keyboard_panel_get_property;
        object_class->set_property = cc_keyboard_panel_set_property;
        object_class->constructor = cc_keyboard_panel_constructor;
        object_class->finalize = cc_keyboard_panel_finalize;

        g_type_class_add_private (klass, sizeof (CcKeyboardPanelPrivate));
}

static void
cc_keyboard_panel_class_finalize (CcKeyboardPanelClass *klass)
{
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *panel)
{
        GConfClient *client;

        panel->priv = CC_KEYBOARD_PANEL_GET_PRIVATE (panel);

        client = gconf_client_get_default ();
        gconf_client_add_dir (client,
                              "/desktop/gnome/peripherals/keyboard",
                              GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
        gconf_client_add_dir (client, "/desktop/gnome/interface",
                              GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
        g_object_unref (client);
}

static void
cc_keyboard_panel_finalize (GObject *object)
{
        CcKeyboardPanel *keyboard_panel;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_KEYBOARD_PANEL (object));

        keyboard_panel = CC_KEYBOARD_PANEL (object);

        g_return_if_fail (keyboard_panel->priv != NULL);

        G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

void
cc_keyboard_panel_register (GIOModule *module)
{
        cc_keyboard_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_PANEL_EXTENSION_POINT_NAME,
                                        CC_TYPE_KEYBOARD_PANEL,
                                        "keyboard",
                                        10);
}
