/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
 *
 * Licensed under the GNU General Public License Version 2
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
 */

#include <NetworkManager.h>

#include "ce-ip-address-entry.h"
#include "ui-helpers.h"
#include <glib/gi18n.h>

struct _CEIPAddressEntry
{
  GtkEntry parent_instance;

  int family;
  gulong notify_id;
};

static void ce_ip_address_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CEIPAddressEntry, ce_ip_address_entry, GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                ce_ip_address_entry_editable_init))

static void
ce_ip_address_focus (CEIPAddressEntry *self, GParamSpec *pspec, gpointer data)
{
  /* We must check the validity only after the user has entered or exited the widget.
     We can't do this in `ce_ip_address_entry_is_valid()` because it is called
     after each keystroke, and the user would receive constantly notifications
     of "IP address value not valid" until the value is correct.
  */
  if (!ce_ip_address_entry_is_valid (self))
    {
      gtk_accessible_announce (GTK_ACCESSIBLE (self),
                               _("IP address value not valid"),
                               GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH);
    }
}

static void
ce_ip_address_entry_changed (GtkEditable *editable)
{
  CEIPAddressEntry *self = CE_IP_ADDRESS_ENTRY (editable);

  if (ce_ip_address_entry_is_valid (self))
    widget_unset_error (GTK_WIDGET (self));
  else
    widget_set_error (GTK_WIDGET (self));
}

static void
ce_ip_address_entry_dispose (GObject *object)
{
  CEIPAddressEntry *self = CE_IP_ADDRESS_ENTRY (object);
  g_clear_signal_handler (&self->notify_id, self);

  G_OBJECT_CLASS (ce_ip_address_entry_parent_class)->dispose (object);
}

static void
ce_ip_address_entry_init (CEIPAddressEntry *self)
{
}

static void
ce_ip_address_entry_editable_init (GtkEditableInterface *iface)
{
  iface->changed = ce_ip_address_entry_changed;
}

static void
ce_ip_address_entry_class_init (CEIPAddressEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = ce_ip_address_entry_dispose;
}

CEIPAddressEntry *
ce_ip_address_entry_new (int family)
{
  CEIPAddressEntry *self;

  self = g_object_new (CE_TYPE_IP_ADDRESS_ENTRY, NULL);
  self->family = family;
  g_signal_connect (self, "notify::has-focus", (GCallback) ce_ip_address_focus, NULL);

  return self;
}

gboolean
ce_ip_address_entry_is_empty (CEIPAddressEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_IP_ADDRESS_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0';
}

gboolean
ce_ip_address_entry_is_valid (CEIPAddressEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_IP_ADDRESS_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0' || nm_utils_ipaddr_valid (self->family, text);
}
