/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Ondrej Holy <oholy@redhat.com>,
 *             Felipe Borges <felipeborges@gnome.org>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gi18n.h>

#include "cc-mouse-test.h"

#define RESET_TEST_BUTTON_STATE_INTERVAL 1

struct _CcMouseTest
{
    AdwWindow  parent_instance;

    GtkWidget *arrow_down;
    GtkWidget *arrow_up;
    GtkWidget *double_click_button;
    GtkWidget *image;
    GtkAdjustment *scrolled_window_adjustment;
    GtkWidget *single_click_button;
    GtkWidget *viewport;

    gint test_buttons_timeout_id;
    gint double_click_button_timeout_id;
};

G_DEFINE_TYPE (CcMouseTest, cc_mouse_test, ADW_TYPE_WINDOW);

static void
on_scroll_adjustment_changed_cb (GtkAdjustment *adjustment,
                                 gpointer       user_data)
{
    CcMouseTest *self = CC_MOUSE_TEST (user_data);
    gboolean is_bottom, is_top;
    gdouble value;

    value = gtk_adjustment_get_value (adjustment);
    is_top = value == gtk_adjustment_get_lower (adjustment);
    is_bottom = value == (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));

    gtk_widget_set_visible (self->arrow_up, !is_top);
    gtk_widget_set_visible (self->arrow_down, !is_bottom);
}

static gboolean
test_buttons_timeout (CcMouseTest *self)
{
    gtk_widget_remove_css_class (self->single_click_button, "success");
    gtk_button_set_label (GTK_BUTTON (self->single_click_button), _("Single Click"));
    gtk_widget_remove_css_class (self->double_click_button, "success");
    gtk_button_set_label (GTK_BUTTON (self->double_click_button), _("Double Click"));

    self->test_buttons_timeout_id = 0;

    return FALSE;
}

static void
on_test_button_clicked_cb (GtkGestureClick *gesture,
                           gint             n_press,
                           gdouble          x,
                           gdouble          y,
                           gpointer         user_data)
{
    CcMouseTest *self = CC_MOUSE_TEST (user_data);
    GtkWidget *button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    guint n_success_press = (button == self->single_click_button) ? 1 : 2;

    if (n_press == n_success_press) {
        gtk_widget_add_css_class (button, "success");
        gtk_button_set_icon_name (GTK_BUTTON (button), "object-select-symbolic");
    } else {
        gtk_widget_remove_css_class (button, "success");
        gtk_button_set_icon_name (GTK_BUTTON (button), "process-stop-symbolic");
    }

    if (self->test_buttons_timeout_id != 0) {
        g_source_remove (self->test_buttons_timeout_id);
        self->test_buttons_timeout_id = 0;
    }

    self->test_buttons_timeout_id =
        g_timeout_add_seconds (RESET_TEST_BUTTON_STATE_INTERVAL, (GSourceFunc) test_buttons_timeout, self);
}

static void
on_mouse_test_show_cb (CcMouseTest *self)
{
    /* Always scroll back to the top */
    gtk_adjustment_set_value (self->scrolled_window_adjustment, 0);
}

static void
setup_dialog (CcMouseTest *self)
{
    g_autoptr(GtkCssProvider) provider = NULL;

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/mouse/mouse-test.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
cc_mouse_test_finalize (GObject *object)
{
    CcMouseTest *self = CC_MOUSE_TEST (object);

    if (self->test_buttons_timeout_id != 0) {
        g_source_remove (self->test_buttons_timeout_id);
        self->test_buttons_timeout_id = 0;
    }

    G_OBJECT_CLASS (cc_mouse_test_parent_class)->finalize (object);
}

static void
cc_mouse_test_class_init (CcMouseTestClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cc_mouse_test_finalize;

    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-test.ui");

    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, arrow_down);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, arrow_up);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, double_click_button);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, image);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, scrolled_window_adjustment);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, single_click_button);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, viewport);

    gtk_widget_class_bind_template_callback (widget_class, on_mouse_test_show_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_scroll_adjustment_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_test_button_clicked_cb);
}

static void
cc_mouse_test_init (CcMouseTest *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->test_buttons_timeout_id = 0;

    setup_dialog (self);
}

GtkWidget *
cc_mouse_test_new (void)
{
    return (GtkWidget *) g_object_new (CC_TYPE_MOUSE_TEST, NULL);
}
