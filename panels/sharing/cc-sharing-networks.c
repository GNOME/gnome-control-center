/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "cc-sharing-networks.h"
#include "org.gnome.SettingsDaemon.Sharing.h"
#include "gsd-sharing-enums.h"
#include "list-box-helper.h"

struct _CcSharingNetworksPrivate {
  GtkWidget *listbox;

  GtkWidget *current_row;
  GtkWidget *current_label;
  GtkWidget *current_icon;
  GtkWidget *current_switch;

  GtkWidget *no_network_row;

  char *service_name;
  GsdSharing *proxy;
  CcSharingStatus status;

  GList *networks; /* list of CcSharingNetwork */
};


G_DEFINE_TYPE_WITH_CODE (CcSharingNetworks, cc_sharing_networks, GTK_TYPE_GRID,
			 G_ADD_PRIVATE (CcSharingNetworks))

enum {
  PROP_0,
  PROP_PROXY,
  PROP_SERVICE_NAME,
  PROP_STATUS
};

static void     cc_sharing_networks_class_init     (CcSharingNetworksClass *klass);
static void     cc_sharing_networks_init           (CcSharingNetworks      *self);
static void     cc_sharing_networks_finalize       (GObject                *object);

static void     cc_sharing_update_networks_box     (CcSharingNetworks *self);

typedef struct {
  char *uuid;
  char *network_name;
  char *carrier_type;
} CcSharingNetwork;

static void
cc_sharing_network_free (gpointer data)
{
  CcSharingNetwork *net = data;

  g_free (net->uuid);
  g_free (net->network_name);
  g_free (net->carrier_type);
  g_free (net);
}

static void
cc_sharing_networks_update_status (CcSharingNetworks *self)
{
  CcSharingStatus status;

  if (self->priv->networks == NULL)
    status = CC_SHARING_STATUS_OFF;
  else if (gtk_widget_is_visible (self->priv->current_switch) &&
	   gtk_switch_get_active (GTK_SWITCH (self->priv->current_switch)))
    status = CC_SHARING_STATUS_ACTIVE;
  else
    status = CC_SHARING_STATUS_ENABLED;

  if (status != self->priv->status) {
    self->priv->status = status;
    g_object_notify (G_OBJECT (self), "status");
  }
}

static void
cc_sharing_update_networks (CcSharingNetworks *self)
{
  GVariant *networks;
  char *uuid, *network_name, *carrier_type;
  GVariantIter iter;
  GError *error = NULL;

  g_list_free_full (self->priv->networks, cc_sharing_network_free);
  self->priv->networks = NULL;

  if (!gsd_sharing_call_list_networks_sync (self->priv->proxy, self->priv->service_name, &networks, NULL, &error)) {
    g_warning ("couldn't list networks: %s", error->message);
    g_dbus_proxy_set_cached_property (G_DBUS_PROXY (self->priv->proxy),
				      "SharingStatus",
				      g_variant_new_uint32 (GSD_SHARING_STATUS_OFFLINE));
    g_error_free (error);
    cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->priv->listbox));
    return;
  }

  g_variant_iter_init (&iter, networks);
  while (g_variant_iter_next (&iter, "(sss)", &uuid, &network_name, &carrier_type)) {
    CcSharingNetwork *net;

    net = g_new0 (CcSharingNetwork, 1);
    net->uuid = uuid;
    net->network_name = network_name;
    net->carrier_type = carrier_type;
    self->priv->networks = g_list_prepend (self->priv->networks, net);
  }
  self->priv->networks = g_list_reverse (self->priv->networks);
  cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->priv->listbox));

  g_variant_unref (networks);
}

static void
cc_sharing_networks_remove_network (GtkWidget         *button,
				    CcSharingNetworks *self)
{
  GtkWidget *row;
  GError *error = NULL;
  gboolean ret;
  const char *uuid;

  row = g_object_get_data (G_OBJECT (button), "row");
  uuid = g_object_get_data (G_OBJECT (row), "uuid");

  ret = gsd_sharing_call_disable_service_sync (self->priv->proxy,
					       self->priv->service_name,
					       uuid,
					       NULL,
					       &error);
  if (!ret) {
    g_warning ("Failed to remove service %s: %s",
	       self->priv->service_name, error->message);
    g_error_free (error);
  }

  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);
}

static gboolean
cc_sharing_networks_enable_network (GtkSwitch *widget,
				    gboolean   state,
				    gpointer   user_data)
{
  CcSharingNetworks *self = user_data;
  GError *error = NULL;
  gboolean ret;

  if (state) {
    ret = gsd_sharing_call_enable_service_sync (self->priv->proxy,
						self->priv->service_name,
						NULL,
						&error);
  } else {
    ret = gsd_sharing_call_disable_service_sync (self->priv->proxy,
						 self->priv->service_name,
						 gsd_sharing_get_current_network (self->priv->proxy),
						 NULL,
						 &error);
  }

  if (ret) {
    gtk_switch_set_state (widget, state);
  } else {
    g_warning ("Failed to %s service %s: %s", state ? "enable" : "disable",
	       self->priv->service_name, error->message);
    g_error_free (error);
    g_signal_handlers_block_by_func (widget,
                                     cc_sharing_networks_enable_network, self);
    gtk_switch_set_active (widget, !state);
    g_signal_handlers_unblock_by_func (widget,
                                       cc_sharing_networks_enable_network, self);
  }

  cc_sharing_update_networks (self);
  cc_sharing_networks_update_status (self);

  return TRUE;
}

static GtkWidget *
cc_sharing_networks_new_row (const char        *uuid,
			     const char        *network_name,
			     const char        *carrier_type,
			     CcSharingNetworks *self)
{
  GtkWidget *row, *box, *w;
  const char *icon_name;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 3);
  gtk_widget_set_margin_start (box, 6);
  gtk_container_add (GTK_CONTAINER (row), box);

  if (g_strcmp0 (carrier_type, "802-11-wireless") == 0) {
    icon_name = "network-wireless-offline-symbolic";
  } else if (g_strcmp0 (carrier_type, "802-3-ethernet") == 0) {
    icon_name = "network-wired-disconnected-symbolic";
  } else {
    icon_name = "network-wired-symbolic";
  }

  w = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_widget_set_margin_end (w, 12);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* Label */
  w = gtk_label_new (network_name);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* Remove button */
  w = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_relief (GTK_BUTTON (w), GTK_RELIEF_NONE);
  gtk_widget_set_margin_top (w, 3);
  gtk_widget_set_margin_bottom (w, 3);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (w), "clicked",
		    G_CALLBACK (cc_sharing_networks_remove_network), self);
  g_object_set_data (G_OBJECT (w), "row", row);

  g_object_set_data_full (G_OBJECT (row), "uuid", g_strdup (uuid), g_free);

  gtk_widget_show_all (row);

  return row;
}

static GtkWidget *
cc_sharing_networks_new_current_row (CcSharingNetworks *self)
{
  GtkWidget *row, *box, *w;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 3);
  gtk_widget_set_margin_start (box, 6);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* Icon */
  w = gtk_image_new_from_icon_name ("image-missing", GTK_ICON_SIZE_MENU);
  gtk_widget_set_margin_end (w, 12);
  gtk_container_add (GTK_CONTAINER (box), w);
  self->priv->current_icon = w;

  /* Label */
  w = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (box), w);
  self->priv->current_label = w;

  w = gtk_switch_new ();
  gtk_widget_set_margin_top (w, 3);
  gtk_widget_set_margin_bottom (w, 3);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (w), "state-set",
		    G_CALLBACK (cc_sharing_networks_enable_network), self);
  self->priv->current_switch = w;
  g_object_set_data (G_OBJECT (w), "row", row);

  gtk_widget_show_all (box);

  return row;
}

static GtkWidget *
cc_sharing_networks_new_no_network_row (CcSharingNetworks *self)
{
  GtkWidget *row, *box, *w;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* Label */
  w = gtk_label_new (_("No networks selected for sharing"));
  gtk_widget_set_hexpand (w, TRUE);
  gtk_widget_set_halign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "dim-label");
  gtk_container_add (GTK_CONTAINER (box), w);

  gtk_widget_show_all (box);

  return row;
}

static void
cc_sharing_update_networks_box (CcSharingNetworks *self)
{
  gboolean current_visible;
  const char *current_network;
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->priv->listbox));
  for (l = children; l != NULL; l = l->next) {
    GtkWidget *row = l->data;

    if (row != self->priv->current_row &&
	row != self->priv->no_network_row)
      gtk_widget_destroy (row);
  }
  g_list_free (children);

  current_network = gsd_sharing_get_current_network (self->priv->proxy);

  if (current_network != NULL &&
      !g_str_equal (current_network, "")) {
    gboolean available;
    const char *carrier_type, *icon_name, *current_network_name;

    gtk_widget_show (self->priv->current_row);
    current_visible = TRUE;

    /* Network name */
    g_object_set_data_full (G_OBJECT (self->priv->current_row),
			    "uuid", g_strdup (current_network), g_free);
    current_network_name = gsd_sharing_get_current_network_name (self->priv->proxy);
    gtk_label_set_label (GTK_LABEL (self->priv->current_label), current_network_name);

    /* Icon */
    carrier_type = gsd_sharing_get_carrier_type (self->priv->proxy);
    if (g_strcmp0 (carrier_type, "802-11-wireless") == 0) {
      icon_name = "network-wireless-signal-excellent-symbolic";
    } else if (g_strcmp0 (carrier_type, "802-3-ethernet") == 0) {
      icon_name = "network-wired-symbolic";
    } else {
      icon_name = "network-wired-symbolic";
    }
    gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->current_icon), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);

    /* State */
    available = gsd_sharing_get_sharing_status (self->priv->proxy) == GSD_SHARING_STATUS_AVAILABLE;
    gtk_widget_set_sensitive (self->priv->current_switch, available);
    //FIXME add a subtitle explaining why it's disabled
  } else {
    gtk_widget_hide (self->priv->current_row);
    current_visible = FALSE;
  }

  for (l = self->priv->networks; l != NULL; l = l->next) {
    CcSharingNetwork *net = l->data;
    GtkWidget *row;

    if (g_strcmp0 (net->uuid, current_network) == 0) {
      g_signal_handlers_block_by_func (self->priv->current_switch,
				       cc_sharing_networks_enable_network, self);
      gtk_switch_set_state (GTK_SWITCH (self->priv->current_switch), TRUE);
      g_signal_handlers_unblock_by_func (self->priv->current_switch,
					 cc_sharing_networks_enable_network, self);
      continue;
    }

    row = cc_sharing_networks_new_row (net->uuid,
				       net->network_name,
				       net->carrier_type,
				       self);
    gtk_list_box_insert (GTK_LIST_BOX (self->priv->listbox), row, -1);
  }

  if (self->priv->networks == NULL &&
      !current_visible) {
    gtk_widget_show (self->priv->no_network_row);
  } else {
    gtk_widget_hide (self->priv->no_network_row);
  }

  cc_sharing_networks_update_status (self);

  cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->priv->listbox));
}

static void
current_network_changed (GObject           *object,
			 GParamSpec        *pspec,
			 CcSharingNetworks *self)
{
  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);
}

static void
cc_sharing_networks_constructed (GObject *object)
{
  CcSharingNetworks *self;

  G_OBJECT_CLASS (cc_sharing_networks_parent_class)->constructed (object);

  self = CC_SHARING_NETWORKS (object);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->priv->listbox),
				cc_list_box_update_header_func, NULL,
				NULL);

  cc_list_box_setup_scrolling (GTK_LIST_BOX (self->priv->listbox), 3);

  self->priv->current_row = cc_sharing_networks_new_current_row (self);
  gtk_list_box_insert (GTK_LIST_BOX (self->priv->listbox), self->priv->current_row, -1);
  g_object_set_data (G_OBJECT (self), "switch", self->priv->current_switch);

  self->priv->no_network_row = cc_sharing_networks_new_no_network_row (self);
  gtk_list_box_insert (GTK_LIST_BOX (self->priv->listbox), self->priv->no_network_row, -1);

  cc_sharing_update_networks (self);
  cc_sharing_update_networks_box (self);

  g_signal_connect (self->priv->proxy, "notify::current-network",
		    G_CALLBACK (current_network_changed), self);
}

static void
cc_sharing_networks_init (CcSharingNetworks *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->priv = cc_sharing_networks_get_instance_private (self);
}

GtkWidget *
cc_sharing_networks_new (GDBusProxy *proxy,
			 const char *service_name)
{
  g_return_val_if_fail (GSD_IS_SHARING (proxy), NULL);
  g_return_val_if_fail (service_name != NULL, NULL);

  return GTK_WIDGET (g_object_new (CC_TYPE_SHARING_NETWORKS,
				   "proxy", proxy,
				   "service-name", service_name,
				   NULL));
}

static void
cc_sharing_networks_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  CcSharingNetworks *self;

  self = CC_SHARING_NETWORKS (object);

  switch (prop_id) {
  case PROP_SERVICE_NAME:
    self->priv->service_name = g_value_dup_string (value);
    break;
  case PROP_PROXY:
    self->priv->proxy = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_sharing_networks_get_property (GObject      *object,
				  guint         prop_id,
				  GValue       *value,
				  GParamSpec   *pspec)
{
  CcSharingNetworks *self;

  self = CC_SHARING_NETWORKS (object);

  switch (prop_id) {
  case PROP_STATUS:
    g_value_set_uint (value, self->priv->status);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_sharing_networks_finalize (GObject *object)
{
  CcSharingNetworks *self;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_SHARING_NETWORKS (object));

  self = CC_SHARING_NETWORKS (object);

  g_return_if_fail (self->priv != NULL);

  g_clear_object (&self->priv->proxy);
  g_clear_pointer (&self->priv->service_name, g_free);

  if (self->priv->networks != NULL) {
    g_list_free_full (self->priv->networks, cc_sharing_network_free);
    self->priv->networks = NULL;
  }

  G_OBJECT_CLASS (cc_sharing_networks_parent_class)->finalize (object);
}


static void
cc_sharing_networks_class_init (CcSharingNetworksClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = cc_sharing_networks_set_property;
  object_class->get_property = cc_sharing_networks_get_property;
  object_class->finalize = cc_sharing_networks_finalize;
  object_class->constructed = cc_sharing_networks_constructed;

  g_object_class_install_property (object_class,
                                   PROP_PROXY,
                                   g_param_spec_object ("proxy",
                                                        "proxy",
                                                        "proxy",
                                                        GSD_TYPE_SHARING_PROXY,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_SERVICE_NAME,
                                   g_param_spec_string ("service-name",
                                                        "service-name",
                                                        "service-name",
                                                        NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_STATUS,
                                   g_param_spec_uint ("status",
                                                      "status",
                                                      "status",
                                                      CC_SHARING_STATUS_UNSET, CC_SHARING_STATUS_ACTIVE + 1, CC_SHARING_STATUS_OFF,
                                                      G_PARAM_READABLE));

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/sharing/networks.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcSharingNetworks, listbox);
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
