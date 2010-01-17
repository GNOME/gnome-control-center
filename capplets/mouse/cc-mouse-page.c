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
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>

#include "gconf-property-editor.h"
#include "capplet-stock-icons.h"

#include "cc-mouse-page.h"

#define CC_MOUSE_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_MOUSE_PAGE, CcMousePagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcMousePagePrivate
{
        GConfChangeSet *changeset;
        GtkWidget      *double_click_image;

        int             double_click_state;
        guint32         double_click_timestamp;
        guint           test_maybe_timeout_id;
        guint           test_on_timeout_id;
};

enum {
        PROP_0,
};

static void     cc_mouse_page_class_init     (CcMousePageClass *klass);
static void     cc_mouse_page_init           (CcMousePage      *mouse_page);
static void     cc_mouse_page_finalize       (GObject          *object);

G_DEFINE_TYPE (CcMousePage, cc_mouse_page, CC_TYPE_PAGE)

enum {
        DOUBLE_CLICK_TEST_OFF = 0,
        DOUBLE_CLICK_TEST_MAYBE,
        DOUBLE_CLICK_TEST_ON
};

#define MOUSE_KEY_DIR "/desktop/gnome/peripherals/mouse"
#define MOUSE_DOUBLE_CLICK_KEY MOUSE_KEY_DIR "/double_click"
#define MOUSE_LEFT_HANDED_KEY MOUSE_KEY_DIR "/left_handed"
#define MOUSE_LOCATE_POINTER_KEY MOUSE_KEY_DIR "/locate_pointer"
#define MOUSE_MOTION_ACCELERATION_KEY MOUSE_KEY_DIR "/motion_acceleration"
#define MOUSE_MOTION_THRESHOLD_KEY MOUSE_KEY_DIR "/motion_threshold"
#define MOUSE_DRAG_THRESHOLD_KEY MOUSE_KEY_DIR "/drag_threshold"

static GConfValue *
double_click_from_gconf (GConfPropertyEditor *peditor,
                         const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_INT);
        gconf_value_set_int (new_value,
                             CLAMP ((int) floor ((gconf_value_get_int (value) + 50) / 100.0) * 100, 100, 1000));
        return new_value;
}

static void
get_default_mouse_info (int *default_numerator,
                        int *default_denominator,
                        int *default_threshold)
{
        int numerator, denominator;
        int threshold;
        int tmp_num, tmp_den, tmp_threshold;

        /* Query X for the default value */
        XGetPointerControl (GDK_DISPLAY (),
                            &numerator,
                            &denominator,
                            &threshold);
        XChangePointerControl (GDK_DISPLAY (),
                               True,
                               True,
                               -1,
                               -1,
                               -1);
        XGetPointerControl (GDK_DISPLAY (),
                            &tmp_num,
                            &tmp_den,
                            &tmp_threshold);
        XChangePointerControl (GDK_DISPLAY (),
                               True,
                               True,
                               numerator,
                               denominator,
                               threshold);

        if (default_numerator)
                *default_numerator = tmp_num;

        if (default_denominator)
                *default_denominator = tmp_den;

        if (default_threshold)
                *default_threshold = tmp_threshold;

}

static GConfValue *
motion_acceleration_from_gconf (GConfPropertyEditor *peditor,
                                const GConfValue    *value)
{
        GConfValue *new_value;
        gfloat      motion_acceleration;

        new_value = gconf_value_new (GCONF_VALUE_FLOAT);

        if (gconf_value_get_float (value) == -1.0) {
                int numerator, denominator;

                get_default_mouse_info (&numerator,
                                        &denominator,
                                        NULL);

                motion_acceleration = CLAMP ((gfloat)(numerator / denominator), 0.2, 6.0);
        }
        else {
                motion_acceleration = CLAMP (gconf_value_get_float (value), 0.2, 6.0);
        }

        if (motion_acceleration >= 1)
                gconf_value_set_float (new_value, motion_acceleration + 4);
        else
                gconf_value_set_float (new_value, motion_acceleration * 5);

        return new_value;
}

static GConfValue *
motion_acceleration_to_gconf (GConfPropertyEditor *peditor,
                              const GConfValue    *value)
{
        GConfValue *new_value;
        gfloat      motion_acceleration;

        new_value = gconf_value_new (GCONF_VALUE_FLOAT);
        motion_acceleration = CLAMP (gconf_value_get_float (value), 1.0, 10.0);

        if (motion_acceleration < 5)
                gconf_value_set_float (new_value, motion_acceleration / 5.0);
        else
                gconf_value_set_float (new_value, motion_acceleration - 4);

        return new_value;
}

static GConfValue *
threshold_from_gconf (GConfPropertyEditor *peditor,
                      const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_FLOAT);

        if (gconf_value_get_int (value) == -1) {
                int threshold;

                get_default_mouse_info (NULL, NULL, &threshold);
                gconf_value_set_float (new_value, CLAMP (threshold, 1, 10));
        }
        else {
                gconf_value_set_float (new_value, CLAMP (gconf_value_get_int (value), 1, 10));
        }

        return new_value;
}

static GConfValue *
drag_threshold_from_gconf (GConfPropertyEditor *peditor,
                           const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_FLOAT);

        gconf_value_set_float (new_value, CLAMP (gconf_value_get_int (value), 1, 10));

        return new_value;
}

/* Timeout for the double click test */
static gboolean
test_maybe_timeout (CcMousePage *page)
{
        page->priv->double_click_state = DOUBLE_CLICK_TEST_OFF;

        gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                  MOUSE_DBLCLCK_OFF,
                                  mouse_capplet_dblclck_icon_get_size ());

        page->priv->test_maybe_timeout_id = 0;

        return FALSE;
}

static gboolean
test_on_timeout (CcMousePage *page)
{
        page->priv->double_click_state = DOUBLE_CLICK_TEST_OFF;

        gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                  MOUSE_DBLCLCK_OFF,
                                  mouse_capplet_dblclck_icon_get_size ());

        page->priv->test_on_timeout_id = 0;

        return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gboolean
on_event_box_button_press_event (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 CcMousePage    *page)
{
        gint          double_click_time;
        GConfValue   *value;
        GConfClient  *client;

        if (event->type != GDK_BUTTON_PRESS)
                return FALSE;

        if (!(page->priv->changeset
              && gconf_change_set_check_value (page->priv->changeset,
                                               MOUSE_DOUBLE_CLICK_KEY,
                                               &value))) {
                client = gconf_client_get_default ();
                double_click_time = gconf_client_get_int (client,
                                                          MOUSE_DOUBLE_CLICK_KEY,
                                                          NULL);
                g_object_unref (client);

        } else {
                double_click_time = gconf_value_get_int (value);
        }

        if (page->priv->test_maybe_timeout_id != 0) {
                g_source_remove (page->priv->test_maybe_timeout_id);
                page->priv->test_maybe_timeout_id = 0;
        }
        if (page->priv->test_on_timeout_id != 0) {
                g_source_remove (page->priv->test_on_timeout_id);
                page->priv->test_on_timeout_id = 0;
        }

        switch (page->priv->double_click_state) {
        case DOUBLE_CLICK_TEST_OFF:
                page->priv->double_click_state = DOUBLE_CLICK_TEST_MAYBE;
                page->priv->test_maybe_timeout_id = g_timeout_add (double_click_time,
                                                                   (GtkFunction) test_maybe_timeout,
                                                                   page);
                break;
        case DOUBLE_CLICK_TEST_MAYBE:
                if (event->time - page->priv->double_click_timestamp < double_click_time) {
                        page->priv->double_click_state = DOUBLE_CLICK_TEST_ON;
                        page->priv->test_on_timeout_id = g_timeout_add (2500,
                                                                        (GtkFunction) test_on_timeout,
                                                                        page);
                }
                break;
        case DOUBLE_CLICK_TEST_ON:
                page->priv->double_click_state = DOUBLE_CLICK_TEST_OFF;
                break;
        }

        page->priv->double_click_timestamp = event->time;

        switch (page->priv->double_click_state) {
        case DOUBLE_CLICK_TEST_ON:
                gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                          MOUSE_DBLCLCK_ON,
                                          mouse_capplet_dblclck_icon_get_size ());
                break;
        case DOUBLE_CLICK_TEST_MAYBE:
                gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                          MOUSE_DBLCLCK_MAYBE,
                                          mouse_capplet_dblclck_icon_get_size ());
                break;
        case DOUBLE_CLICK_TEST_OFF:
                gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                          MOUSE_DBLCLCK_OFF,
                                          mouse_capplet_dblclck_icon_get_size ());
                break;
        }

        return TRUE;
}

static void
orientation_radio_button_release_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static GConfValue *
left_handed_from_gconf (GConfPropertyEditor *peditor,
                        const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_INT);

        gconf_value_set_int (new_value, gconf_value_get_bool (value));

        return new_value;
}

static GConfValue *
left_handed_to_gconf (GConfPropertyEditor *peditor,
                      const GConfValue    *value)
{
        GConfValue *new_value;

        new_value = gconf_value_new (GCONF_VALUE_BOOL);

        gconf_value_set_bool (new_value, gconf_value_get_int (value) == 1);

        return new_value;
}

static void
cc_mouse_page_set_property (GObject      *object,
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
cc_mouse_page_get_property (GObject    *object,
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
setup_page (CcMousePage *page)
{
        GtkBuilder      *builder;
        GtkWidget       *widget;
        GError          *error;
        GtkSizeGroup    *size_group;
        GtkRadioButton  *radio;
        GObject         *peditor;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/gnome-mouse-properties.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        page->priv->double_click_image = WID ("double_click_image");

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("acceleration_label"));
        gtk_size_group_add_widget (size_group, WID ("sensitivity_label"));
        gtk_size_group_add_widget (size_group, WID ("threshold_label"));
        gtk_size_group_add_widget (size_group, WID ("timeout_label"));

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("acceleration_fast_label"));
        gtk_size_group_add_widget (size_group, WID ("sensitivity_high_label"));
        gtk_size_group_add_widget (size_group, WID ("threshold_large_label"));
        gtk_size_group_add_widget (size_group, WID ("timeout_long_label"));

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, WID ("acceleration_slow_label"));
        gtk_size_group_add_widget (size_group, WID ("sensitivity_low_label"));
        gtk_size_group_add_widget (size_group, WID ("threshold_small_label"));
        gtk_size_group_add_widget (size_group, WID ("timeout_short_label"));

#if 1
        /* Orientation radio buttons */
        radio = GTK_RADIO_BUTTON (WID ("left_handed_radio"));
        peditor = gconf_peditor_new_select_radio (page->priv->changeset,
                                                  MOUSE_LEFT_HANDED_KEY,
                                                  gtk_radio_button_get_group (radio),
                                                  "conv-to-widget-cb",
                                                  left_handed_from_gconf,
                                                  "conv-from-widget-cb",
                                                  left_handed_to_gconf,
                                                  NULL);

        /* explicitly connect to button-release so that you can change orientation with either button */
        g_signal_connect (WID ("right_handed_radio"),
                          "button_release_event",
                          G_CALLBACK (orientation_radio_button_release_event),
                          NULL);
        g_signal_connect (WID ("left_handed_radio"),
                          "button_release_event",
                          G_CALLBACK (orientation_radio_button_release_event),
                          NULL);

        /* Locate pointer toggle */
        peditor = gconf_peditor_new_boolean (page->priv->changeset,
                                             MOUSE_LOCATE_POINTER_KEY,
                                             WID ("locate_pointer_toggle"),
                                             NULL);

        /* Double-click time */
        peditor = gconf_peditor_new_numeric_range (page->priv->changeset,
                                                   MOUSE_DOUBLE_CLICK_KEY,
                                                   WID ("delay_scale"),
                                                   "conv-to-widget-cb",
                                                   double_click_from_gconf,
                                                   NULL);

#endif
        gtk_image_set_from_stock (GTK_IMAGE (page->priv->double_click_image),
                                  MOUSE_DBLCLCK_OFF,
                                  mouse_capplet_dblclck_icon_get_size ());

        g_signal_connect (WID ("double_click_eventbox"),
                          "button_press_event",
                          G_CALLBACK (on_event_box_button_press_event),
                          page);

        /* speed */
        gconf_peditor_new_numeric_range (page->priv->changeset,
                                         MOUSE_MOTION_ACCELERATION_KEY,
                                         WID ("accel_scale"),
                                         "conv-to-widget-cb",
                                         motion_acceleration_from_gconf,
                                         "conv-from-widget-cb",
                                         motion_acceleration_to_gconf,
                                         NULL);

        gconf_peditor_new_numeric_range (page->priv->changeset,
                                         MOUSE_MOTION_THRESHOLD_KEY,
                                         WID ("sensitivity_scale"),
                                         "conv-to-widget-cb",
                                         threshold_from_gconf,
                                         NULL);

        /* DnD threshold */
        gconf_peditor_new_numeric_range (page->priv->changeset,
                                         MOUSE_DRAG_THRESHOLD_KEY,
                                         WID ("drag_threshold_scale"),
                                         "conv-to-widget-cb",
                                         drag_threshold_from_gconf,
                                         NULL);


        widget = WID ("general_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);
}

static GObject *
cc_mouse_page_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
        CcMousePage      *mouse_page;

        mouse_page = CC_MOUSE_PAGE (G_OBJECT_CLASS (cc_mouse_page_parent_class)->constructor (type,
                                                                                              n_construct_properties,
                                                                                              construct_properties));

        g_object_set (mouse_page,
                      "display-name", _("Mouse"),
                      "id", "general",
                      NULL);

        setup_page (mouse_page);

        return G_OBJECT (mouse_page);
}

static void
cc_mouse_page_class_init (CcMousePageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_mouse_page_get_property;
        object_class->set_property = cc_mouse_page_set_property;
        object_class->constructor = cc_mouse_page_constructor;
        object_class->finalize = cc_mouse_page_finalize;

        g_type_class_add_private (klass, sizeof (CcMousePagePrivate));
}

static void
cc_mouse_page_init (CcMousePage *page)
{
        GConfClient *client;

        page->priv = CC_MOUSE_PAGE_GET_PRIVATE (page);

        client = gconf_client_get_default ();
        gconf_client_add_dir (client,
                              MOUSE_KEY_DIR,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
        g_object_unref (client);

        capplet_init_stock_icons ();
}

static void
cc_mouse_page_finalize (GObject *object)
{
        CcMousePage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_MOUSE_PAGE (object));

        page = CC_MOUSE_PAGE (object);

        g_return_if_fail (page->priv != NULL);


        G_OBJECT_CLASS (cc_mouse_page_parent_class)->finalize (object);
}

CcPage *
cc_mouse_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_MOUSE_PAGE, NULL);

        return CC_PAGE (object);
}
