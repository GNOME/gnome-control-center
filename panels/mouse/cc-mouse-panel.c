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

#include "cc-mouse-panel.h"
#include "cc-mouse-resources.h"

#include "gnome-mouse-properties.h"
#include "gnome-mouse-test.h"
#include <gtk/gtk.h>

#include <glib/gi18n.h>

struct _CcMousePanel
{
  CcPanel    parent_instance;

  GtkWidget *stack;
};

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

enum {
  CC_MOUSE_PAGE_PREFS,
  CC_MOUSE_PAGE_TEST
};

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
shell_test_button_toggled (GtkToggleButton *button, CcMousePanel *panel)
{
  gboolean active;

  active = gtk_toggle_button_get_active (button);
  gtk_stack_set_visible_child_name (GTK_STACK (panel->stack), active ? "test_widget" : "prefs_widget");
}

static void
cc_mouse_panel_constructed (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);
  GtkWidget *button;
  CcShell *shell;

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->constructed (object);

  /* Add test area button to shell header. */
  shell = cc_panel_get_shell (CC_PANEL (self));

  button = gtk_toggle_button_new_with_mnemonic (_("Test Your _Settings"));
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               "text-button");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_visible (button, TRUE);

  cc_shell_embed_widget_in_header (shell, button, GTK_POS_RIGHT);

  g_signal_connect (GTK_BUTTON (button), "toggled",
                    G_CALLBACK (shell_test_button_toggled),
                    self);
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  GtkWidget *prefs_widget, *test_widget;

  g_resources_register (cc_mouse_get_resource ());

  prefs_widget = cc_mouse_properties_new ();
  gtk_widget_show (prefs_widget);
  test_widget = cc_mouse_test_new ();
  gtk_widget_show (test_widget);

  self->stack = gtk_stack_new ();
  gtk_widget_show (self->stack);
  gtk_stack_add_named (GTK_STACK (self->stack), prefs_widget, "prefs_widget");
  gtk_stack_add_named (GTK_STACK (self->stack), test_widget, "test_widget");

  gtk_container_add (GTK_CONTAINER (self), self->stack);
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->dispose = cc_mouse_panel_dispose;
  object_class->constructed = cc_mouse_panel_constructed;
}
