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

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

#define MOUSE_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_MOUSE_PANEL, CcMousePanelPrivate))

struct _CcMousePanelPrivate
{
  GtkWidget  *test_dialog;
  GtkWidget  *prefs_widget;
  GtkWidget  *test_widget;
};

enum {
  CC_MOUSE_PAGE_PREFS,
  CC_MOUSE_PAGE_TEST
};

static void
cc_mouse_panel_dispose (GObject *object)
{
  CcMousePanelPrivate *priv = CC_MOUSE_PANEL (object)->priv;

  if (priv->test_dialog)
    {
      gtk_widget_destroy (priv->test_dialog);
      priv->test_dialog = NULL;
    }

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
shell_test_button_clicked (GtkButton *button, CcMousePanel *panel)
{
  CcMousePanelPrivate *priv = panel->priv;

  /* GTK_RESPONSE_NONE is returned if the dialog is being destroyed, so only
   * hide the dialog if it is not being destroyed */
  if (gtk_dialog_run (GTK_DIALOG (priv->test_dialog)) != GTK_RESPONSE_NONE)
    gtk_widget_hide (priv->test_dialog);
}

static void
cc_mouse_panel_constructed (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);
  CcMousePanelPrivate *priv = self->priv;
  GtkWidget *button, *container, *toplevel;
  CcShell *shell;

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->constructed (object);

  /* Add test area button to shell header. */
  shell = cc_panel_get_shell (CC_PANEL (self));

  button = gtk_button_new_with_mnemonic (_("Test Your _Settings"));
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               "text-button");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_visible (button, TRUE);

  cc_shell_embed_widget_in_header (shell, button);

  g_signal_connect (GTK_BUTTON (button), "clicked",
                    G_CALLBACK (shell_test_button_clicked),
                    self);

  toplevel = cc_shell_get_toplevel (shell);
  priv->test_dialog = g_object_new (GTK_TYPE_DIALOG, "title", _("Test Your Settings"),
                                                     "transient-for", GTK_WINDOW (toplevel),
                                                     "modal", TRUE,
                                                     "use_header-bar", TRUE,
                                                     "resizable", FALSE,
                                                     NULL);

  container = gtk_dialog_get_content_area (GTK_DIALOG (priv->test_dialog));
  gtk_container_add (GTK_CONTAINER (container), priv->test_widget);
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  CcMousePanelPrivate *priv;

  priv = self->priv = MOUSE_PANEL_PRIVATE (self);
  g_resources_register (cc_mouse_get_resource ());

  priv->prefs_widget = cc_mouse_properties_new ();
  priv->test_widget = cc_mouse_test_new ();

  gtk_widget_set_margin_start (priv->prefs_widget, 6);
  gtk_widget_set_margin_end (priv->prefs_widget, 6);
  gtk_widget_set_margin_top (priv->prefs_widget, 6);
  gtk_widget_set_margin_bottom (priv->prefs_widget, 6);

  gtk_container_add (GTK_CONTAINER (self), priv->prefs_widget);
  gtk_widget_show (priv->prefs_widget);
  gtk_widget_show (priv->test_widget);
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcMousePanelPrivate));

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->dispose = cc_mouse_panel_dispose;
  object_class->constructed = cc_mouse_panel_constructed;
}
