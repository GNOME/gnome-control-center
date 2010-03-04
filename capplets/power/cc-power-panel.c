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

#include "cc-power-panel.h"

#define CC_POWER_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POWER_PANEL, CcPowerPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcPowerPanelPrivate
{
        GtkWidget *idle_slider;
};

static void     cc_power_panel_class_init     (CcPowerPanelClass *klass);
static void     cc_power_panel_init           (CcPowerPanel      *power_panel);
static void     cc_power_panel_finalize       (GObject             *object);

G_DEFINE_DYNAMIC_TYPE (CcPowerPanel, cc_power_panel, CC_TYPE_PANEL)

static void
setup_panel (CcPowerPanel *panel)
{
        GError *error = NULL;
        GtkBuilder *builder;
        GtkWidget *widget;

        builder = gtk_builder_new ();

        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR "/power.ui",
                                   &error);

        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"), error->message);
                g_error_free (error);
                return;
        }

        widget = WID ("main_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (panel));
        gtk_widget_show (widget);
}

static GObject *
cc_power_panel_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
        CcPowerPanel      *power_panel;

        power_panel = CC_POWER_PANEL (G_OBJECT_CLASS (cc_power_panel_parent_class)->constructor
                                      (type, n_construct_properties, construct_properties));

        g_object_set (power_panel,
                      "display-name", _("Power and brightness"),
                      "id", "power-properties.desktop",
                      NULL);

        setup_panel (power_panel);

        return G_OBJECT (power_panel);
}

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = cc_power_panel_constructor;
        object_class->finalize = cc_power_panel_finalize;

        g_type_class_add_private (klass, sizeof (CcPowerPanelPrivate));
}

static void
cc_power_panel_class_finalize (CcPowerPanelClass *klass)
{
}

static void
cc_power_panel_init (CcPowerPanel *panel)
{
        panel->priv = CC_POWER_PANEL_GET_PRIVATE (panel);
}

static void
cc_power_panel_finalize (GObject *object)
{
        CcPowerPanel *power_panel;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_POWER_PANEL (object));

        power_panel = CC_POWER_PANEL (object);

        g_return_if_fail (power_panel->priv != NULL);

        G_OBJECT_CLASS (cc_power_panel_parent_class)->finalize (object);
}

void
cc_power_panel_register (GIOModule *module)
{
        cc_power_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_PANEL_EXTENSION_POINT_NAME,
                                        CC_TYPE_POWER_PANEL,
                                        "power",
                                        10);
}
