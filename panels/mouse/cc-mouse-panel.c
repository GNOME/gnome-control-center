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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 *          Ondrej Holy <oholy@redhat.com>
 *
 */

#include "cc-mouse-panel.h"
#include "gnome-mouse-properties.h"
#include "gnome-mouse-test.h"
#include <gtk/gtk.h>

#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

#define MOUSE_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_MOUSE_PANEL, CcMousePanelPrivate))

#define WID(x) (GtkWidget*) gtk_builder_get_object (dialog, x)

struct _CcMousePanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *widget;
  GtkWidget  *prefs_widget;
  GtkWidget  *test_widget;
  GtkWidget  *shell_header;
};

enum {
  CC_MOUSE_PAGE_PREFS,
  CC_MOUSE_PAGE_TEST
};

static void
cc_mouse_panel_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_mouse_panel_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_mouse_panel_dispose (GObject *object)
{
  CcMousePanelPrivate *priv = CC_MOUSE_PANEL (object)->priv;

  g_clear_object (&priv->shell_header);

  if (priv->prefs_widget)
    {
      gnome_mouse_properties_dispose (priv->prefs_widget);
      priv->prefs_widget = NULL;
    }

  if (priv->test_widget)
    {
      gnome_mouse_test_dispose (priv->test_widget);
      priv->test_widget = NULL;
    }

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcMousePanelPrivate));

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->get_property = cc_mouse_panel_get_property;
  object_class->set_property = cc_mouse_panel_set_property;
  object_class->dispose = cc_mouse_panel_dispose;
}

/* Toggle between mouse panel properties and testing area. */
static void
shell_test_button_toggle_event (GtkToggleButton *button, CcMousePanel *panel)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (panel->priv->widget);
  gint page_num;

  if (gtk_toggle_button_get_active (button)) {
    GtkBuilder *dialog = panel->priv->builder;
    GtkAdjustment *adjustment;

    page_num = CC_MOUSE_PAGE_TEST;

    adjustment = GTK_ADJUSTMENT (WID ("scrolled_window_adjustment"));
    gtk_adjustment_set_value (adjustment,
                              gtk_adjustment_get_upper (adjustment));
  } else {
    page_num = CC_MOUSE_PAGE_PREFS;
  }

  gtk_notebook_set_current_page (notebook, page_num);
}

/* Add test area toggle to shell header. */
static gboolean
add_shell_test_button_cb (gpointer user_data)
{
  CcMousePanel *panel = CC_MOUSE_PANEL (user_data);
  GtkWidget *box, *button;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  button = gtk_toggle_button_new_with_mnemonic (_("_Test Your Settings"));
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_widget_set_visible (button, TRUE);

  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (panel)), box);
  gtk_widget_set_visible (box, TRUE);

  panel->priv->shell_header = g_object_ref (box);

  g_signal_connect (GTK_BUTTON (button), "toggled",
                    G_CALLBACK (shell_test_button_toggle_event),
                    panel);

  return FALSE;
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  CcMousePanelPrivate *priv;
  GtkBuilder *dialog;
  GError *error = NULL;

  priv = self->priv = MOUSE_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  gtk_builder_add_from_file (priv->builder,
                             GNOMECC_UI_DIR "/gnome-mouse-properties.ui",
                             &error);
  if (error != NULL)
    {
      g_warning ("Error loading UI file: %s", error->message);
      return;
    }

  gtk_builder_add_from_file (priv->builder,
                             GNOMECC_UI_DIR "/gnome-mouse-test.ui",
                             &error);
  if (error != NULL)
    {
      g_warning ("Error loading UI file: %s", error->message);
      return;
    }

  dialog = priv->builder;

  priv->prefs_widget = gnome_mouse_properties_init (priv->builder);
  priv->test_widget = gnome_mouse_test_init (priv->builder);

  priv->widget = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->widget), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->widget), FALSE);

  gtk_widget_reparent (WID ("prefs_widget"), priv->widget);
  gtk_widget_reparent (WID ("test_widget"), priv->widget);

  gtk_container_add (GTK_CONTAINER (self), priv->widget);
  gtk_widget_show (priv->widget);

  g_idle_add (add_shell_test_button_cb, self);
}

void
cc_mouse_panel_register (GIOModule *module)
{
  cc_mouse_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_MOUSE_PANEL,
                                  "mouse", 0);
}

