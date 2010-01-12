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

#include "cc-keyboard-panel.h"

#include "gconf-property-editor.h"
#include "capplet-stock-icons.h"

#include "gnome-keyboard-properties-a11y.h"
#include "gnome-keyboard-properties-xkb.h"

#define CC_KEYBOARD_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_KEYBOARD_PANEL, CcKeyboardPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcKeyboardPanelPrivate
{
        gpointer dummy;
};

enum {
        PROP_0,
};

static void     cc_keyboard_panel_class_init     (CcKeyboardPanelClass *klass);
static void     cc_keyboard_panel_init           (CcKeyboardPanel      *keyboard_panel);
static void     cc_keyboard_panel_finalize       (GObject             *object);

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


static GConfValue *
blink_from_widget (GConfPropertyEditor *peditor,
                   const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_INT);
        gconf_value_set_int (new_value,
                             2600 - gconf_value_get_int (value));

        return new_value;
}

static GConfValue *
blink_to_widget (GConfPropertyEditor * peditor, const GConfValue * value)
{
        GConfValue *new_value;
        gint current_rate;

        current_rate = gconf_value_get_int (value);
        new_value = gconf_value_new (GCONF_VALUE_INT);
        gconf_value_set_int (new_value,
                             CLAMP (2600 - current_rate, 100, 2500));

        return new_value;
}

static void
setup_panel (CcKeyboardPanel *panel)
{
        GtkBuilder     *builder;
        GtkSizeGroup   *size_group;
        GtkWidget      *image;
        GtkWidget      *widget;
        GObject        *peditor;
        char           *monitor;
        GConfChangeSet *changeset;

        changeset = NULL;

        builder = gtk_builder_new ();
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/gnome-keyboard-properties-dialog.ui",
                                   NULL);

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("repeat_slow_label"));
        gtk_size_group_add_widget (size_group, WID ("delay_short_label"));
        gtk_size_group_add_widget (size_group, WID ("blink_slow_label"));
        g_object_unref (G_OBJECT (size_group));

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("repeat_fast_label"));
        gtk_size_group_add_widget (size_group, WID ("delay_long_label"));
        gtk_size_group_add_widget (size_group, WID ("blink_fast_label"));
        g_object_unref (G_OBJECT (size_group));

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("repeat_delay_scale"));
        gtk_size_group_add_widget (size_group, WID ("repeat_speed_scale"));
        gtk_size_group_add_widget (size_group, WID ("cursor_blink_time_scale"));
        g_object_unref (G_OBJECT (size_group));

        image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (WID ("xkb_layouts_add")), image);

        image = gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (WID ("xkb_reset_to_defaults")), image);

        peditor = gconf_peditor_new_boolean (changeset, "/desktop/gnome/peripherals/keyboard/repeat",
                                             WID ("repeat_toggle"), NULL);
        gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
                                        WID ("repeat_table"));

        gconf_peditor_new_numeric_range (changeset, "/desktop/gnome/peripherals/keyboard/delay",
                                         WID ("repeat_delay_scale"), NULL);

        gconf_peditor_new_numeric_range (changeset, "/desktop/gnome/peripherals/keyboard/rate",
                                         WID ("repeat_speed_scale"), NULL);

        peditor = gconf_peditor_new_boolean (changeset, "/desktop/gnome/interface/cursor_blink",
                                             WID ("cursor_toggle"), NULL);
        gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
                                        WID ("cursor_hbox"));
        gconf_peditor_new_numeric_range (changeset,
                                         "/desktop/gnome/interface/cursor_blink_time",
                                         WID ("cursor_blink_time_scale"),
                                         "conv-to-widget-cb",
                                         blink_to_widget,
                                         "conv-from-widget-cb",
                                         blink_from_widget, NULL);

        /* Ergonomics */
        monitor = g_find_program_in_path ("gnome-typing-monitor");
        if (monitor != NULL) {
                g_free (monitor);

                peditor = gconf_peditor_new_boolean (changeset, "/desktop/gnome/typing_break/enabled",
                                                     WID ("break_enabled_toggle"), NULL);
                gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
                                                WID ("break_details_table"));
                gconf_peditor_new_numeric_range (changeset,
                                                 "/desktop/gnome/typing_break/type_time",
                                                 WID ("break_enabled_spin"), NULL);
                gconf_peditor_new_numeric_range (changeset,
                                                 "/desktop/gnome/typing_break/break_time",
                                                 WID ("break_interval_spin"),
                                                 NULL);
                gconf_peditor_new_boolean (changeset,
                                           "/desktop/gnome/typing_break/allow_postpone",
                                           WID ("break_postponement_toggle"),
                                           NULL);

        } else {
                /* don't show the typing break tab if the daemon is not available */
                GtkNotebook *nb;
                gint         tb_page;

                nb = GTK_NOTEBOOK (WID ("keyboard_notebook"));
                tb_page = gtk_notebook_page_num (nb, WID ("break_enabled_toggle"));
                gtk_notebook_remove_page (nb, tb_page);
        }

        setup_xkb_tabs (builder, changeset);
        setup_a11y_tabs (builder, changeset);

        widget = WID ("main-vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (panel));
        gtk_widget_show (widget);
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
