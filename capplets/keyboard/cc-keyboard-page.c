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

#include "gconf-property-editor.h"

#include "cc-keyboard-page.h"

#define CC_KEYBOARD_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_KEYBOARD_PAGE, CcKeyboardPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcKeyboardPagePrivate
{
        gpointer dummy;
};

enum {
        PROP_0,
};

static void     cc_keyboard_page_class_init     (CcKeyboardPageClass *klass);
static void     cc_keyboard_page_init           (CcKeyboardPage      *keyboard_page);
static void     cc_keyboard_page_finalize       (GObject             *object);

G_DEFINE_TYPE (CcKeyboardPage, cc_keyboard_page, CC_TYPE_PAGE)

static void
cc_keyboard_page_set_property (GObject      *object,
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
cc_keyboard_page_get_property (GObject    *object,
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
blink_to_widget (GConfPropertyEditor *peditor,
                 const GConfValue    *value)
{
        GConfValue *new_value;
        int         current_rate;

        current_rate = gconf_value_get_int (value);
        new_value = gconf_value_new (GCONF_VALUE_INT);
        gconf_value_set_int (new_value,
                             CLAMP (2600 - current_rate, 100, 2500));

        return new_value;
}

static void
setup_page (CcKeyboardPage *page)
{
        GtkBuilder      *builder;
        GtkWidget       *widget;
        GError          *error;
        GtkSizeGroup    *size_group;
        GObject         *peditor;
        GConfChangeSet  *changeset;

        changeset = NULL;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/gnome-keyboard-properties-dialog.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }


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


        peditor = gconf_peditor_new_boolean (changeset,
                                             "/desktop/gnome/peripherals/keyboard/repeat",
                                             WID ("repeat_toggle"),
                                             NULL);
        gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
                                        WID ("repeat_table"));

        gconf_peditor_new_numeric_range (changeset,
                                         "/desktop/gnome/peripherals/keyboard/delay",
                                         WID ("repeat_delay_scale"),
                                         NULL);

        gconf_peditor_new_numeric_range (changeset,
                                         "/desktop/gnome/peripherals/keyboard/rate",
                                         WID ("repeat_speed_scale"),
                                         NULL);

        peditor = gconf_peditor_new_boolean (changeset,
                                             "/desktop/gnome/interface/cursor_blink",
                                             WID ("cursor_toggle"),
                                             NULL);
        gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor),
                                        WID ("cursor_hbox"));
        gconf_peditor_new_numeric_range (changeset,
                                         "/desktop/gnome/interface/cursor_blink_time",
                                         WID ("cursor_blink_time_scale"),
                                         "conv-to-widget-cb",
                                         blink_to_widget,
                                         "conv-from-widget-cb",
                                         blink_from_widget, NULL);

        widget = WID ("general_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);
}

static GObject *
cc_keyboard_page_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        CcKeyboardPage      *keyboard_page;

        keyboard_page = CC_KEYBOARD_PAGE (G_OBJECT_CLASS (cc_keyboard_page_parent_class)->constructor (type,
                                                                                                                n_construct_properties,
                                                                                                                construct_properties));

        g_object_set (keyboard_page,
                      "display-name", _("Keyboard"),
                      "id", "general",
                      NULL);

        setup_page (keyboard_page);

        return G_OBJECT (keyboard_page);
}

static void
cc_keyboard_page_class_init (CcKeyboardPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_keyboard_page_get_property;
        object_class->set_property = cc_keyboard_page_set_property;
        object_class->constructor = cc_keyboard_page_constructor;
        object_class->finalize = cc_keyboard_page_finalize;

        g_type_class_add_private (klass, sizeof (CcKeyboardPagePrivate));
}

static void
cc_keyboard_page_init (CcKeyboardPage *page)
{
        page->priv = CC_KEYBOARD_PAGE_GET_PRIVATE (page);
}

static void
cc_keyboard_page_finalize (GObject *object)
{
        CcKeyboardPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_KEYBOARD_PAGE (object));

        page = CC_KEYBOARD_PAGE (object);

        g_return_if_fail (page->priv != NULL);


        G_OBJECT_CLASS (cc_keyboard_page_parent_class)->finalize (object);
}

CcPage *
cc_keyboard_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_KEYBOARD_PAGE, NULL);

        return CC_PAGE (object);
}
