/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2025 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include "cc-network-activity-page.h"
#include "shell/cc-object-storage.h"

#include <glib/gi18n.h>

#include <NetworkManager.h>
struct _CcNetworkActivityPage
{
  AdwNavigationPage    parent_instance;

  AdwSwitchRow        *network_login_dectection_row;

  GCancellable        *cancellable;
  NMClient            *nm_client;

};

G_DEFINE_TYPE (CcNetworkActivityPage, cc_network_activity_page, ADW_TYPE_NAVIGATION_PAGE)

static void
setup_nm_client (CcNetworkActivityPage *self,
                 NMClient              *client)
{
  self->nm_client = client;

  g_object_bind_property (self->nm_client, NM_CLIENT_CONNECTIVITY_CHECK_AVAILABLE,
                          self->network_login_dectection_row, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->nm_client, NM_CLIENT_CONNECTIVITY_CHECK_ENABLED,
                          self->network_login_dectection_row, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
nm_client_ready_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  CcNetworkActivityPage *self;
  NMClient *client;
  g_autoptr(GError) error = NULL;

  client = nm_client_new_finish (res, &error);
  if (!client) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_warning ("Failed to create NetworkManager client: %s", error->message);
      return;
    }
  }

  self = user_data;

  setup_nm_client (self, client);
  cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
}

static void
cc_network_activity_page_finalize (GObject *object)
{
  CcNetworkActivityPage *self = CC_NETWORK_ACTIVITY_PAGE (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (cc_network_activity_page_parent_class)->finalize (object);
}


static void
cc_network_activity_page_class_init (CcNetworkActivityPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_network_activity_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/network-activity/cc-network-activity-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcNetworkActivityPage, network_login_dectection_row);
}

static void
cc_network_activity_page_init (CcNetworkActivityPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  if (cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
    setup_nm_client (self, cc_object_storage_get_object (CC_OBJECT_NMCLIENT));
  } else {
    nm_client_new_async (self->cancellable, nm_client_ready_cb, self);
  }
}
