/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "cc-privacy-panel.h"

#include <egg-list-box/egg-list-box.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct _CcPrivacyPanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *list_box;

  GSettings  *lock_settings;
};

static void
cc_privacy_panel_finalize (GObject *object)
{
  CcPrivacyPanelPrivate *priv = CC_PRIVACY_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_object (&priv->lock_settings);

  G_OBJECT_CLASS (cc_privacy_panel_parent_class)->finalize (object);
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (*separator);
    }
}

static void
activate_child (CcPrivacyPanel *self,
                GtkWidget      *child)
{
  GObject *w;
  const gchar *dialog_id;
  GtkWidget *toplevel;

  dialog_id = g_object_get_data (G_OBJECT (child), "dialog-id");
  w = gtk_builder_get_object (self->priv->builder, dialog_id);
  if (w == NULL)
    {
      g_warning ("No such dialog: %s", dialog_id);
      return;
    }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (w), TRUE);
  gtk_window_present (GTK_WINDOW (w));
}

static void
cc_privacy_panel_init (CcPrivacyPanel *self)
{
  GError    *error;
  GtkWidget *widget;
  GtkWidget *scrolled_window;
  guint res;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CC_TYPE_PRIVACY_PANEL, CcPrivacyPanelPrivate);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  res = gtk_builder_add_from_file (self->priv->builder,
                                   GNOMECC_UI_DIR "/privacy.ui",
                                   &error);

  if (res == 0)
    {
      g_warning ("Could not load interface file: %s",
                 (error != NULL) ? error->message : "unknown error");
      g_clear_error (&error);
      return;
    }

  scrolled_window = WID ("scrolled_window");
  widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_add_to_scrolled (EGG_LIST_BOX (widget), GTK_SCROLLED_WINDOW (scrolled_window));
  self->priv->list_box = widget;
  gtk_widget_show (widget);

  g_signal_connect_swapped (widget, "child-activated",
                            G_CALLBACK (activate_child), self);

  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);

  widget = WID ("privacy_vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = cc_privacy_panel_finalize;

  g_type_class_add_private (klass, sizeof (CcPrivacyPanelPrivate));
}

void
cc_privacy_panel_register (GIOModule *module)
{
  cc_privacy_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_PRIVACY_PANEL,
                                  "privacy", 0);
}
