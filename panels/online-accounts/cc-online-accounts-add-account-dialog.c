/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Debarshi Ray <debarshir@gnome.org>
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include "shell/list-box-helper.h"
#include "cc-online-accounts-add-account-dialog.h"

#define BRANDED_PAGE "_branded"
#define OTHER_PAGE "_other"

struct _GoaPanelAddAccountDialogPrivate
{
  GtkListBox *branded_list_box;
  GtkListBox *contacts_list_box;
  GtkListBox *mail_list_box;
  GtkListBox *chat_list_box;
  GtkListBox *ticketing_list_box;
  GError *error;
  GoaClient *client;
  GoaObject *object;
  GoaProvider *provider;
  GtkWidget *contacts_grid;
  GtkWidget *mail_grid;
  GtkWidget *chat_grid;
  GtkWidget *ticketing_grid;
  GtkWidget *stack;
  gboolean add_other;
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
add_account_dialog_add_account (GoaPanelAddAccountDialog *add_account,
                                GoaProvider *provider)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;
  GtkWidget *vbox;

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (add_account));
  gtk_container_foreach (GTK_CONTAINER (vbox), (GtkCallback) gtk_widget_destroy, NULL);

  /* This spins gtk_dialog_run() */
  priv->object = goa_provider_add_account (provider,
                                           priv->client,
                                           GTK_DIALOG (add_account),
                                           GTK_BOX (vbox),
                                           &priv->error);
}

static void
list_box_row_activated_cb (GoaPanelAddAccountDialog *add_account, GtkListBoxRow *row)
{
  GoaProvider *provider;

  provider = g_object_get_data (G_OBJECT (row), "provider");
  if (provider == NULL)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (add_account->priv->stack), OTHER_PAGE);
      return;
    }

  add_account_dialog_add_account (add_account, provider);
}

static void
add_account_dialog_create_group_ui (GoaPanelAddAccountDialog *add_account,
                                    GtkListBox **list_box,
                                    GtkWidget **group_grid,
                                    GtkWidget *page_grid,
                                    const gchar *name)
{
  GtkWidget *label;
  GtkWidget *sw;
  gchar *markup;

  *group_grid = gtk_grid_new ();
  gtk_widget_set_no_show_all (*group_grid, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (*group_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (*group_grid), 6);
  gtk_container_add (GTK_CONTAINER (page_grid), *group_grid);

  markup = g_strdup_printf ("<b>%s</b>", name);
  label = gtk_label_new (NULL);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (*group_grid), label);
  g_free (markup);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (*group_grid), sw);

  *list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (*list_box));
  gtk_list_box_set_selection_mode (*list_box, GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (*list_box, cc_list_box_update_header_func, NULL, NULL);
  g_signal_connect_swapped (*list_box, "row-activated",
                            G_CALLBACK (list_box_row_activated_cb), add_account);
}

static void
add_account_dialog_create_provider_ui (GoaPanelAddAccountDialog *add_account,
                                       GoaProvider *provider,
                                       GtkListBox *list_box)
{
  GIcon *icon;
  GList *children;
  GtkWidget *row;
  GtkWidget *row_grid;
  GtkWidget *image;
  GtkWidget *label;
  gchar *markup;
  gchar *name;

  row = gtk_list_box_row_new ();
  row_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (row_grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (row_grid), 6);
  gtk_container_add (GTK_CONTAINER (row), row_grid);

  if (provider == NULL)
    {
      g_object_set_data (G_OBJECT (row), "provider", NULL);
      icon = g_themed_icon_new_with_default_fallbacks ("goa-account");
      name = g_strdup (C_("Online Account", "Other"));
    }
  else
    {
      g_object_set_data_full (G_OBJECT (row), "provider", g_object_ref (provider), g_object_unref);
      icon = goa_provider_get_provider_icon (provider, NULL);
      name = goa_provider_get_provider_name (provider, NULL);
    }

  children = gtk_container_get_children (GTK_CONTAINER (list_box));
  if (children != NULL)
    {
      /* FIXME: Ideally we want the list boxes to use as much space as
       * it's available to try to show all the content, but GtkScrolledView
       * ignores its child's natural size,
       * see https://bugzilla.gnome.org/show_bug.cgi?id=660654
       * For now we just make list boxes with multiple children expand as
       * the result is quite similar. For those with only one child,
       * we turn off the scrolling. */

      GtkWidget *sw;

      sw = gtk_widget_get_ancestor (GTK_WIDGET (list_box), GTK_TYPE_SCROLLED_WINDOW);
      g_assert_nonnull (sw);

      gtk_widget_set_vexpand (sw, TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

      g_list_free (children);
    }

  gtk_container_add (GTK_CONTAINER (list_box), row);

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_container_add (GTK_CONTAINER (row_grid), image);

  markup = g_strdup_printf ("<b>%s</b>", name);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (row_grid), label);
  g_free (markup);

  g_free (name);
  g_object_unref (icon);
}

static void
goa_panel_add_account_dialog_realize (GtkWidget *widget)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (widget);
  GtkWindow *parent;

  parent = gtk_window_get_transient_for (GTK_WINDOW (add_account));
  if (parent != NULL)
    {
      gint width;
      gint height;

      gtk_window_get_size (parent, &width, &height);
      gtk_widget_set_size_request (GTK_WIDGET (add_account), (gint) (0.5 * width), (gint) (1.25 * height));
    }

  GTK_WIDGET_CLASS (goa_panel_add_account_dialog_parent_class)->realize (widget);
}

static void
goa_panel_add_account_dialog_dispose (GObject *object)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (object);
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  g_clear_object (&priv->object);
  g_clear_object (&priv->client);
  g_clear_object (&priv->provider);

  G_OBJECT_CLASS (goa_panel_add_account_dialog_parent_class)->dispose (object);
}

static void
goa_panel_add_account_dialog_finalize (GObject *object)
{
  GoaPanelAddAccountDialog *add_account = GOA_PANEL_ADD_ACCOUNT_DIALOG (object);
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  g_clear_error (&priv->error);

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

  priv->stack = gtk_stack_new ();
  gtk_stack_set_transition_type (GTK_STACK (priv->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_container_add (GTK_CONTAINER (grid), priv->stack);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_stack_add_named (GTK_STACK (priv->stack), sw, BRANDED_PAGE);

  priv->branded_list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->branded_list_box));
  gtk_list_box_set_selection_mode (priv->branded_list_box, GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (priv->branded_list_box, cc_list_box_update_header_func, NULL, NULL);
  g_signal_connect_swapped (priv->branded_list_box, "row-activated",
                            G_CALLBACK (list_box_row_activated_cb), add_account);

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_stack_add_named (GTK_STACK (priv->stack), grid, OTHER_PAGE);

  add_account_dialog_create_group_ui (add_account,
                                      &priv->mail_list_box,
                                      &priv->mail_grid,
                                      grid,
                                      _("Mail"));

  add_account_dialog_create_group_ui (add_account,
                                      &priv->contacts_list_box,
                                      &priv->contacts_grid,
                                      grid,
                                      _("Contacts"));

  add_account_dialog_create_group_ui (add_account,
                                      &priv->chat_list_box,
                                      &priv->chat_grid,
                                      grid,
                                      _("Chat"));

  add_account_dialog_create_group_ui (add_account,
                                      &priv->ticketing_list_box,
                                      &priv->ticketing_grid,
                                      grid,
                                      _("Resources"));

  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), BRANDED_PAGE);
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
  return g_object_new (GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, "client", client, "use-header-bar", TRUE, NULL);
}

void
goa_panel_add_account_dialog_set_preseed_data (GoaPanelAddAccountDialog *add_account,
                                               GoaProvider *provider,
                                               GVariant *preseed)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;

  g_clear_object (&priv->provider);

  if (provider != NULL)
    {
      priv->provider = g_object_ref (provider);
      goa_provider_set_preseed_data (provider, preseed);
    }
}

void
goa_panel_add_account_dialog_add_provider (GoaPanelAddAccountDialog *add_account, GoaProvider *provider)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;
  GtkListBox *list_box;
  GoaProviderGroup group;
  GtkWidget *group_grid = NULL;

  g_return_if_fail (provider != NULL);

  group = goa_provider_get_provider_group (provider);

  /* The list of providers returned by GOA are sorted such that all
   * the branded providers are at the beginning of the list, followed
   * by the others. Since this is the order in which they are added,
   * we can rely on this fact, which helps to simplify the code.
   */
  if (group != GOA_PROVIDER_GROUP_BRANDED && !priv->add_other)
    {
      add_account_dialog_create_provider_ui (add_account, NULL, priv->branded_list_box);
      priv->add_other = TRUE;
    }

  switch (group)
    {
    case GOA_PROVIDER_GROUP_BRANDED:
      list_box = priv->branded_list_box;
      break;
    case GOA_PROVIDER_GROUP_CONTACTS:
      group_grid = priv->contacts_grid;
      list_box = priv->contacts_list_box;
      break;
    case GOA_PROVIDER_GROUP_MAIL:
      group_grid = priv->mail_grid;
      list_box = priv->mail_list_box;
      break;
    case GOA_PROVIDER_GROUP_CHAT:
      group_grid = priv->chat_grid;
      list_box = priv->chat_list_box;
      break;
    case GOA_PROVIDER_GROUP_TICKETING:
      group_grid = priv->ticketing_grid;
      list_box = priv->ticketing_list_box;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  add_account_dialog_create_provider_ui (add_account, provider, list_box);

  if (group_grid != NULL)
    {
      gtk_widget_set_no_show_all (group_grid, FALSE);
      gtk_widget_show_all (group_grid);
    }
}

void
goa_panel_add_account_dialog_run (GoaPanelAddAccountDialog *add_account)
{
  GoaPanelAddAccountDialogPrivate *priv = add_account->priv;
  if (priv->provider != NULL)
    add_account_dialog_add_account (add_account, priv->provider);
  else
    gtk_dialog_run (GTK_DIALOG (add_account));
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
