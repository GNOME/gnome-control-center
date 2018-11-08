/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 *          Ondrej Holy <oholy@redhat.com>
 *
 */

#include <gtk/gtk.h>

#include "cc-mouse-panel.h"
#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"
#include "gnome-mouse-properties.h"

struct _CcMousePanel
{
  CcPanel            parent_instance;

  CcMouseProperties *mouse_properties;
  CcMouseTest       *mouse_test;
  GtkStack          *stack;
  GtkButton         *test_button;
};

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

static void
cc_mouse_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
test_button_toggled_cb (CcMousePanel *self)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->test_button)))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->mouse_test));
  else
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->mouse_properties));
}

static void
cc_mouse_panel_constructed (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);
  CcShell *shell;

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->constructed (object);

  /* Add test area button to shell header. */
  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, GTK_WIDGET (self->test_button));
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  g_resources_register (cc_mouse_get_resource ());

  cc_mouse_properties_get_type ();
  cc_mouse_test_get_type ();
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->dispose = cc_mouse_panel_dispose;
  object_class->constructed = cc_mouse_panel_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_properties);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_test);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, test_button);

  gtk_widget_class_bind_template_callback (widget_class, test_button_toggled_cb);
}
