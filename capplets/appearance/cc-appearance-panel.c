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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-desktop-thumbnail.h>

#include "cc-background-page.h"
#include "cc-appearance-panel.h"

#include "gconf-property-editor.h"
#if 0
#include "appearance-desktop.h"
#include "appearance-font.h"
#include "appearance-themes.h"
#include "appearance-style.h"
#endif
#include "theme-installer.h"
#include "theme-thumbnail.h"

#define CC_APPEARANCE_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_APPEARANCE_PANEL, CcAppearancePanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcAppearancePanelPrivate
{
        GtkWidget *notebook;
        CcPage    *background_page;
};

enum {
        PROP_0,
};

static void     cc_appearance_panel_class_init     (CcAppearancePanelClass *klass);
static void     cc_appearance_panel_init           (CcAppearancePanel      *appearance_panel);
static void     cc_appearance_panel_finalize       (GObject             *object);

G_DEFINE_DYNAMIC_TYPE (CcAppearancePanel, cc_appearance_panel, CC_TYPE_PANEL)

static void
cc_appearance_panel_set_property (GObject      *object,
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
cc_appearance_panel_get_property (GObject    *object,
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

#if 0
/* FIXME */
static void
install_theme (CcAppearancePanel *panel,
               const char        *filename)
{
        GFile     *inst;
        GtkWidget *toplevel;

        g_assert (filename != NULL);

        inst = g_file_new_for_commandline_arg (filename);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        if (GTK_WIDGET_TOPLEVEL (toplevel)) {
                gnome_theme_install (inst, GTK_WINDOW (toplevel));
        } else {
                gnome_theme_install (inst, NULL);
        }
        g_object_unref (inst);
}
#endif

static void
setup_panel (CcAppearancePanel *panel)
{
        GtkWidget *label;
        char      *display_name;

        panel->priv->notebook = gtk_notebook_new ();
        gtk_container_add (GTK_CONTAINER (panel), panel->priv->notebook);
        gtk_widget_show (panel->priv->notebook);

        /* FIXME: load pages */
        panel->priv->background_page = cc_background_page_new ();
        g_object_get (panel->priv->background_page,
                      "display-name", &display_name,
                      NULL);
        label = gtk_label_new (display_name);
        g_free (display_name);
        gtk_notebook_append_page (GTK_NOTEBOOK (panel->priv->notebook), GTK_WIDGET (panel->priv->background_page), label);
        gtk_widget_show (GTK_WIDGET (panel->priv->background_page));

        g_object_set (panel,
                      "current-page", panel->priv->background_page,
                      NULL);
}

static GObject *
cc_appearance_panel_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
        CcAppearancePanel      *appearance_panel;

        appearance_panel = CC_APPEARANCE_PANEL (G_OBJECT_CLASS (cc_appearance_panel_parent_class)->constructor (type,
                                                                                                                n_construct_properties,
                                                                                                                construct_properties));

        g_object_set (appearance_panel,
                      "display-name", _("Appearance"),
                      "id", "gnome-appearance-properties.desktop",
                      NULL);

        //theme_thumbnail_factory_init (0, NULL);

        setup_panel (appearance_panel);

        return G_OBJECT (appearance_panel);
}

static void
cc_appearance_panel_class_init (CcAppearancePanelClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_appearance_panel_get_property;
        object_class->set_property = cc_appearance_panel_set_property;
        object_class->constructor = cc_appearance_panel_constructor;
        object_class->finalize = cc_appearance_panel_finalize;

        g_type_class_add_private (klass, sizeof (CcAppearancePanelPrivate));
}

static void
cc_appearance_panel_class_finalize (CcAppearancePanelClass *klass)
{
}

static void
cc_appearance_panel_init (CcAppearancePanel *panel)
{
        GConfClient *client;

        panel->priv = CC_APPEARANCE_PANEL_GET_PRIVATE (panel);

        client = gconf_client_get_default ();
        gconf_client_add_dir (client,
                              "/desktop/gnome/peripherals/appearance",
                              GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
        gconf_client_add_dir (client, "/desktop/gnome/interface",
                              GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
        g_object_unref (client);
}

static void
cc_appearance_panel_finalize (GObject *object)
{
        CcAppearancePanel *appearance_panel;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_APPEARANCE_PANEL (object));

        appearance_panel = CC_APPEARANCE_PANEL (object);

        g_return_if_fail (appearance_panel->priv != NULL);

        G_OBJECT_CLASS (cc_appearance_panel_parent_class)->finalize (object);
}

void
cc_appearance_panel_register (GIOModule *module)
{
        cc_appearance_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_PANEL_EXTENSION_POINT_NAME,
                                        CC_TYPE_APPEARANCE_PANEL,
                                        "appearance",
                                        10);
}
