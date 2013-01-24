/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Debarshi Ray <debarshir@gnome.org>
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include <egg-list-box/egg-list-box.h>
#include "cc-online-accounts-add-account-dialog.h"

struct _GoaPanelAddAccountDialogPrivate
{
  EggListBox *list_box;
  GError *error;
  GoaClient *client;
  GoaObject *object;
  GtkListStore *list_store;
};

#define GOA_ADD_ACCOUNT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, GoaPanelAddAccountDialogPrivate))

enum
{
  PROP_0,
  PROP_CLIENT,
};

enum
{
  COLUMN_PROVIDER,
  COLUMN_ICON,
  COLUMN_MARKUP,
  N_COLUMNS
};

G_DEFINE_TYPE (GoaPanelAddAccountDialog, goa_panel_add_account_dialog, GTK_TYPE_DIALOG)

static void
add_account_dialog_add_account (GoaPanelAddAccountDialog *add_account, GoaProvider *provider)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;
  GList *children;
  GList *l;
  GtkWidget *action_area;
  GtkWidget *vbox;

  action_area = gtk_dialog_get_action_area (GTK_DIALOG (add_account));
  vbox = gtk_dialog_get_content_area (GTK_DIALOG (add_account));
  children = gtk_container_get_children (GTK_CONTAINER (vbox));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *child = l->data;
      if (child != action_area)
        gtk_container_remove (GTK_CONTAINER (vbox), child);
    }
  g_list_free (children);

  priv->object = goa_provider_add_account (provider,
                                           priv->client,
                                           GTK_DIALOG (add_account),
                                           GTK_BOX (vbox),
                                           &priv->error);
}

static void
list_box_child_activated_cb (GoaPanelAddAccountDialog *add_account, GtkWidget *child)
{
  GoaProvider *provider;

  provider = g_object_get_data (G_OBJECT (child), "provider");
  add_account_dialog_add_account (add_account, provider);
  gtk_dialog_response (GTK_DIALOG (add_account), GTK_RESPONSE_OK);
}

static void
list_box_separator_cb (GtkWidget **separator, GtkWidget *child, GtkWidget *before, gpointer user_data)
{
  if (*separator == NULL && before != NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

      /* https://bugzilla.gnome.org/show_bug.cgi?id=690545 */
      g_object_ref_sink (*separator);
      gtk_widget_show (*separator);
    }
}

static void
goa_panel_add_account_dialog_realize (GtkWidget *widget)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (widget);
  GtkWidget *button;
  GtkWindow *parent;

  parent = gtk_window_get_transient_for (GTK_WINDOW (add_account));
  if (parent != NULL)
    {
      gint width;
      gint height;

      gtk_window_get_size (parent, &width, &height);
      gtk_widget_set_size_request (GTK_WIDGET (add_account), (gint) (0.5 * width), (gint) (0.9 * height));
    }

  GTK_WIDGET_CLASS (goa_panel_add_account_dialog_parent_class)->realize (widget);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (add_account), GTK_RESPONSE_CANCEL);
  gtk_widget_grab_focus (button);
}

static void
goa_panel_add_account_dialog_dispose (GObject *object)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (object);
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  g_clear_object (&priv->object);
  g_clear_object (&priv->client);

  G_OBJECT_CLASS (goa_panel_add_account_dialog_parent_class)->dispose (object);
}

static void
goa_panel_add_account_dialog_finalize (GObject *object)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (object);
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  if (priv->error != NULL)
    g_error_free (priv->error);

  G_OBJECT_CLASS (goa_panel_add_account_dialog_parent_class)->finalize (object);
}

static void
goa_panel_add_account_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (object);
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  switch (prop_id)
    {
    case PROP_CLIENT:
      priv->client = GOA_CLIENT (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
goa_panel_add_account_dialog_init (GoaPanelAddAccountDialog *add_account)
{
  GoaPanelAddAccountDialogPrivate *priv;
  GtkWidget *sw;
  GtkWidget *vbox;
  GtkWidget *grid;

  add_account->priv = GOA_ADD_ACCOUNT_DIALOG_GET_PRIVATE (add_account);
  priv = add_account->priv;

  gtk_container_set_border_width (GTK_CONTAINER (add_account), 6);
  gtk_window_set_modal (GTK_WINDOW (add_account), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (add_account), FALSE);
  /* translators: This is the title of the "Add Account" dialog. */
  gtk_window_set_title (GTK_WINDOW (add_account), _("Add Account"));

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (add_account));
  grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
  gtk_widget_set_margin_bottom (grid, 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (vbox), grid);

  priv->list_store = gtk_list_store_new (N_COLUMNS, GOA_TYPE_PROVIDER, G_TYPE_ICON, G_TYPE_STRING);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), sw);

  priv->list_box = egg_list_box_new ();
  egg_list_box_add_to_scrolled (priv->list_box, GTK_SCROLLED_WINDOW (sw));
  egg_list_box_set_selection_mode (priv->list_box, GTK_SELECTION_NONE);
  egg_list_box_set_separator_funcs (priv->list_box, list_box_separator_cb, NULL, NULL);
  g_signal_connect_swapped (priv->list_box, "child-activated",
                            G_CALLBACK (list_box_child_activated_cb), add_account);

  gtk_dialog_add_button (GTK_DIALOG (add_account), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_set_default_response (GTK_DIALOG (add_account), GTK_RESPONSE_CANCEL);
}

static void
goa_panel_add_account_dialog_class_init (GoaPanelAddAccountDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = goa_panel_add_account_dialog_dispose;
  object_class->finalize = goa_panel_add_account_dialog_finalize;
  object_class->set_property = goa_panel_add_account_dialog_set_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = goa_panel_add_account_dialog_realize;

  g_type_class_add_private (object_class, sizeof (GoaPanelAddAccountDialogPrivate));

  g_object_class_install_property (object_class,
                                   PROP_CLIENT,
                                   g_param_spec_object ("client",
							"Goa Client",
							"A Goa client for talking to the Goa daemon.",
							GOA_TYPE_CLIENT,
							G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
goa_panel_add_account_dialog_new (GoaClient *client)
{
  return g_object_new (GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, "client", client, NULL);
}

void
goa_panel_add_account_dialog_add_provider (GoaPanelAddAccountDialog *add_account, GoaProvider *provider)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;
  GIcon *icon;
  GtkWidget *grid;
  GtkWidget *image;
  GtkWidget *label;
  gchar *markup;
  gchar *name;

  grid = gtk_grid_new ();
  g_object_set_data_full (G_OBJECT (grid), "provider", g_object_ref (provider), g_object_unref);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_container_add (GTK_CONTAINER (priv->list_box), grid);

  icon = goa_provider_get_provider_icon (provider, NULL);
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_container_add (GTK_CONTAINER (grid), image);
  g_object_unref (icon);

  name = goa_provider_get_provider_name (provider, NULL);
  markup = g_strdup_printf ("<b>%s</b>", name);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (grid), label);
  g_free (markup);
  g_free (name);
}

GoaObject *
goa_panel_add_account_dialog_get_account (GoaPanelAddAccountDialog *add_account, GError **error)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  if (error != NULL && priv->error != NULL)
    *error = g_error_copy (priv->error);

  if (priv->object != NULL)
    g_object_ref (priv->object);

  return priv->object;
}
