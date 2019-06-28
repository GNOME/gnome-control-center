/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2019  Red Hat, Inc,
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
 * Written by: Kalev Lember <klember@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-subscription-details-dialog.h"

#define DBUS_TIMEOUT 300000 /* 5 minutes */

typedef enum {
  DIALOG_STATE_SHOW_DETAILS,
  DIALOG_STATE_UNREGISTER,
  DIALOG_STATE_UNREGISTERING
} DialogState;

struct _CcSubscriptionDetailsDialog
{
  GtkDialog     parent_instance;

  DialogState   state;
  GCancellable *cancellable;
  GDBusProxy   *subscription_proxy;
  GPtrArray    *products;

  /* template widgets */
  GtkButton    *back_button;
  GtkSpinner   *spinner;
  GtkButton    *header_unregister_button;
  GtkRevealer  *notification_revealer;
  GtkLabel     *error_label;
  GtkStack     *stack;
  GtkBox       *products_box1;
  GtkBox       *products_box2;
  GtkButton    *unregister_button;
};

G_DEFINE_TYPE (CcSubscriptionDetailsDialog, cc_subscription_details_dialog, GTK_TYPE_DIALOG);

typedef struct
{
  gchar *product_name;
  gchar *product_id;
  gchar *version;
  gchar *arch;
  gchar *status;
  gchar *starts;
  gchar *ends;
} ProductData;

static void
product_data_free (ProductData *product)
{
  g_free (product->product_name);
  g_free (product->product_id);
  g_free (product->version);
  g_free (product->arch);
  g_free (product->status);
  g_free (product->starts);
  g_free (product->ends);
  g_free (product);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ProductData, product_data_free);

static void
add_product_row (GtkGrid *product_grid, const gchar *name, const gchar *value, gint top_attach)
{
  GtkWidget *w;

  w = gtk_label_new (name);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "dim-label");
  gtk_grid_attach (product_grid, w, 0, top_attach, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_END);
  gtk_widget_show (w);

  if (value == NULL)
    value = _("Unknown");

  w = gtk_label_new (value);
  gtk_grid_attach (product_grid, w, 1, top_attach, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_show (w);
}

static GtkWidget *
add_product (CcSubscriptionDetailsDialog *self, ProductData *product)
{
  GtkGrid *product_grid;
  const gchar *status_text;

  if (g_strcmp0 (product->status, "subscribed") == 0)
    status_text = _("Subscribed");
  else
    status_text = _("Not Subscribed (Not supported by a valid subscription.)");

  product_grid = GTK_GRID (gtk_grid_new ());
  gtk_grid_set_column_spacing (product_grid, 12);
  gtk_grid_set_row_spacing (product_grid, 6);
  gtk_widget_set_margin_top (GTK_WIDGET (product_grid), 18);
  gtk_widget_set_margin_bottom (GTK_WIDGET (product_grid), 12);
  gtk_widget_show (GTK_WIDGET (product_grid));

  add_product_row (product_grid, _("Product Name"), product->product_name, 0);
  add_product_row (product_grid, _("Product ID"), product->product_id, 1);
  add_product_row (product_grid, _("Version"), product->version, 2);
  add_product_row (product_grid, _("Arch"), product->arch, 3);
  add_product_row (product_grid, _("Status"), status_text, 4);
  add_product_row (product_grid, _("Starts"), product->starts, 5);
  add_product_row (product_grid, _("Ends"), product->ends, 6);

  return GTK_WIDGET (product_grid);
}

static void
remove_all_children (GtkContainer *container)
{
  g_autoptr(GList) list = gtk_container_get_children (container);

  for (GList *l = list; l != NULL; l = l->next)
    gtk_container_remove (container, (GtkWidget *) l->data);
}

static void
dialog_reload (CcSubscriptionDetailsDialog *self)
{
  GtkHeaderBar *header = GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (self)));

  switch (self->state)
    {
    case DIALOG_STATE_SHOW_DETAILS:
      gtk_header_bar_set_show_close_button (header, TRUE);

      gtk_window_set_title (GTK_WINDOW (self), _("Registration Details"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->header_unregister_button), TRUE);

      gtk_widget_hide (GTK_WIDGET (self->back_button));
      gtk_widget_hide (GTK_WIDGET (self->header_unregister_button));

      gtk_stack_set_visible_child_name (self->stack, "show-details");
      break;

    case DIALOG_STATE_UNREGISTER:
      gtk_header_bar_set_show_close_button (header, FALSE);

      gtk_window_set_title (GTK_WINDOW (self), _("Unregister System"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->header_unregister_button), TRUE);

      gtk_widget_show (GTK_WIDGET (self->back_button));
      gtk_widget_show (GTK_WIDGET (self->header_unregister_button));

      gtk_stack_set_visible_child_name (self->stack, "unregister");
      break;

    case DIALOG_STATE_UNREGISTERING:
      gtk_header_bar_set_show_close_button (header, FALSE);

      gtk_window_set_title (GTK_WINDOW (self), _("Unregistering System…"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->header_unregister_button), FALSE);

      gtk_widget_show (GTK_WIDGET (self->back_button));
      gtk_widget_show (GTK_WIDGET (self->header_unregister_button));

      gtk_stack_set_visible_child_name (self->stack, "unregister");
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  remove_all_children (GTK_CONTAINER (self->products_box1));
  remove_all_children (GTK_CONTAINER (self->products_box2));

  if (self->products == NULL || self->products->len == 0)
    {
      /* the widgets are duplicate to allow sliding between two stack pages */
      GtkWidget *w1 = gtk_label_new (_("No installed products detected."));
      GtkWidget *w2 = gtk_label_new (_("No installed products detected."));
      gtk_widget_show (w1);
      gtk_widget_show (w2);
      gtk_container_add (GTK_CONTAINER (self->products_box1), w1);
      gtk_container_add (GTK_CONTAINER (self->products_box2), w2);
      return;
    }

  for (guint i = 0; i < self->products->len; i++)
    {
      ProductData *product = g_ptr_array_index (self->products, i);
      /* the widgets are duplicate to allow sliding between two stack pages */
      GtkWidget *w1 = add_product (self, product);
      GtkWidget *w2 = add_product (self, product);
      gtk_container_add (GTK_CONTAINER (self->products_box1), w1);
      gtk_container_add (GTK_CONTAINER (self->products_box2), w2);
    }
}

static ProductData *
parse_product_variant (GVariant *product_variant)
{
  g_autoptr(ProductData) product = g_new0 (ProductData, 1);
  g_auto(GVariantDict) dict;

  g_variant_dict_init (&dict, product_variant);

  g_variant_dict_lookup (&dict, "product-name", "s", &product->product_name);
  g_variant_dict_lookup (&dict, "product-id", "s", &product->product_id);
  g_variant_dict_lookup (&dict, "version", "s", &product->version);
  g_variant_dict_lookup (&dict, "arch", "s", &product->arch);
  g_variant_dict_lookup (&dict, "status", "s", &product->status);
  g_variant_dict_lookup (&dict, "starts", "s", &product->starts);
  g_variant_dict_lookup (&dict, "ends", "s", &product->ends);

  return g_steal_pointer (&product);
}

static void
load_installed_products (CcSubscriptionDetailsDialog *self)
{
  GVariantIter iter_array;
  GVariant *child;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) installed_products_variant = NULL;

  installed_products_variant = g_dbus_proxy_get_cached_property (self->subscription_proxy, "InstalledProducts");
  if (installed_products_variant == NULL)
    {
      g_debug ("Unable to get InstalledProducts dbus property");
      return;
    }

  g_ptr_array_set_size (self->products, 0);

  g_variant_iter_init (&iter_array, installed_products_variant);
  while ((child = g_variant_iter_next_value (&iter_array)) != NULL)
    {
      g_autoptr(GVariant) product_variant = g_steal_pointer (&child);
      g_ptr_array_add (self->products, parse_product_variant (product_variant));
    }
}

static void
unregistration_done_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  CcSubscriptionDetailsDialog *self = (CcSubscriptionDetailsDialog *) user_data;
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_dbus_error_strip_remote_error (error);
      gtk_label_set_text (self->error_label, error->message);
      gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

      gtk_spinner_stop (self->spinner);

      self->state = DIALOG_STATE_UNREGISTER;
      dialog_reload (self);
      return;
    }

  gtk_spinner_stop (self->spinner);

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
}

static void
header_unregister_button_clicked_cb (CcSubscriptionDetailsDialog *self)
{
  gtk_spinner_start (self->spinner);

  self->state = DIALOG_STATE_UNREGISTERING;
  dialog_reload (self);

  g_dbus_proxy_call (self->subscription_proxy,
                     "Unregister",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     DBUS_TIMEOUT,
                     self->cancellable,
                     unregistration_done_cb,
                     self);
}

static void
back_button_clicked_cb (CcSubscriptionDetailsDialog *self)
{
  gtk_spinner_stop (self->spinner);

  self->state = DIALOG_STATE_SHOW_DETAILS;
  dialog_reload (self);
}

static void
unregister_button_clicked_cb (CcSubscriptionDetailsDialog *self)
{
  self->state = DIALOG_STATE_UNREGISTER;
  dialog_reload (self);
}

static void
dismiss_notification (CcSubscriptionDetailsDialog *self)
{
  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
}

static void
cc_subscription_details_dialog_init (CcSubscriptionDetailsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  self->products = g_ptr_array_new_with_free_func ((GDestroyNotify) product_data_free);
  self->state = DIALOG_STATE_SHOW_DETAILS;
}

static void
cc_subscription_details_dialog_dispose (GObject *obj)
{
  CcSubscriptionDetailsDialog *self = (CcSubscriptionDetailsDialog *) obj;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->subscription_proxy);

  G_OBJECT_CLASS (cc_subscription_details_dialog_parent_class)->dispose (obj);
}

static void
cc_subscription_details_dialog_finalize (GObject *obj)
{
  CcSubscriptionDetailsDialog *self = (CcSubscriptionDetailsDialog *) obj;

  g_clear_pointer (&self->products, g_ptr_array_unref);

  G_OBJECT_CLASS (cc_subscription_details_dialog_parent_class)->finalize (obj);
}

static void
cc_subscription_details_dialog_class_init (CcSubscriptionDetailsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_subscription_details_dialog_dispose;
  object_class->finalize = cc_subscription_details_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info/cc-subscription-details-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, header_unregister_button);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, error_label);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, products_box1);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, products_box2);
  gtk_widget_class_bind_template_child (widget_class, CcSubscriptionDetailsDialog, unregister_button);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, header_unregister_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, unregister_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dismiss_notification);
}

CcSubscriptionDetailsDialog *
cc_subscription_details_dialog_new (GDBusProxy *subscription_proxy)
{
  CcSubscriptionDetailsDialog *self;

  self = g_object_new (CC_TYPE_SUBSCRIPTION_DETAILS_DIALOG, "use-header-bar", TRUE, NULL);
  self->subscription_proxy = g_object_ref (subscription_proxy);

  load_installed_products (self);
  dialog_reload (self);

  return self;
}
