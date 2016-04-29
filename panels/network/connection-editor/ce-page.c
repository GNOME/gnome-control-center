/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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

#include "config.h"

#include <string.h>

#include <net/if_arp.h>
#include <netinet/ether.h>

#include <NetworkManager.h>

#include <glib/gi18n.h>

#include "ce-page.h"


G_DEFINE_ABSTRACT_TYPE (CEPage, ce_page, G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_CONNECTION,
        PROP_INITIALIZED,
};

enum {
        CHANGED,
        INITIALIZED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

gboolean
ce_page_validate (CEPage *page, NMConnection *connection, GError **error)
{
        g_return_val_if_fail (CE_IS_PAGE (page), FALSE);
        g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

        if (CE_PAGE_GET_CLASS (page)->validate)
                return CE_PAGE_GET_CLASS (page)->validate (page, connection, error);

        return TRUE;
}

static void
dispose (GObject *object)
{
        CEPage *self = CE_PAGE (object);

        g_clear_object (&self->page);
        g_clear_object (&self->builder);
        g_clear_object (&self->connection);

        G_OBJECT_CLASS (ce_page_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
        CEPage *self = CE_PAGE (object);

        g_free (self->title);
        if (self->cancellable) {
                g_cancellable_cancel (self->cancellable);
                g_object_unref (self->cancellable);
        }

        G_OBJECT_CLASS (ce_page_parent_class)->finalize (object);
}

GtkWidget *
ce_page_get_page (CEPage *self)
{
        g_return_val_if_fail (CE_IS_PAGE (self), NULL);

        return self->page;
}

const char *
ce_page_get_title (CEPage *self)
{
        g_return_val_if_fail (CE_IS_PAGE (self), NULL);

        return self->title;
}

gboolean
ce_page_get_initialized (CEPage *self)
{
        g_return_val_if_fail (CE_IS_PAGE (self), FALSE);

        return self->initialized;
}

void
ce_page_changed (CEPage *self)
{
        g_return_if_fail (CE_IS_PAGE (self));

        g_signal_emit (self, signals[CHANGED], 0);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
        CEPage *self = CE_PAGE (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                g_value_set_object (value, self->connection);
                break;
        case PROP_INITIALIZED:
                g_value_set_boolean (value, self->initialized);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
        CEPage *self = CE_PAGE (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                if (self->connection)
                        g_object_unref (self->connection);
                self->connection = g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
ce_page_init (CEPage *self)
{
        self->builder = gtk_builder_new ();
        self->cancellable = g_cancellable_new ();
}

static void
ce_page_class_init (CEPageClass *page_class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (page_class);

        /* virtual methods */
        object_class->dispose      = dispose;
        object_class->finalize     = finalize;
        object_class->get_property = get_property;
        object_class->set_property = set_property;

        /* Properties */
        g_object_class_install_property
                (object_class, PROP_CONNECTION,
                 g_param_spec_object ("connection",
                                      "Connection",
                                      "Connection",
                                      NM_TYPE_CONNECTION,
                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property
                (object_class, PROP_INITIALIZED,
                 g_param_spec_boolean ("initialized",
                                       "Initialized",
                                       "Initialized",
                                       FALSE,
                                       G_PARAM_READABLE));

        signals[CHANGED] =
                g_signal_new ("changed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (CEPageClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[INITIALIZED] =
                g_signal_new ("initialized",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (CEPageClass, initialized),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
}

CEPage *
ce_page_new (GType             type,
             NMConnection     *connection,
             NMClient         *client,
             const gchar      *ui_resource,
             const gchar      *title)
{
        CEPage *page;
        GError *error = NULL;

        page = CE_PAGE (g_object_new (type,
                                      "connection", connection,
                                      NULL));
        page->title = g_strdup (title);
        page->client = client;

        if (ui_resource) {
                if (!gtk_builder_add_from_resource (page->builder, ui_resource, &error)) {
                        g_warning ("Couldn't load builder file: %s", error->message);
                        g_error_free (error);
                        g_object_unref (page);
                        return NULL;
                }
                page->page = GTK_WIDGET (gtk_builder_get_object (page->builder, "page"));
                if (!page->page) {
                        g_warning ("Couldn't load page widget from %s", ui_resource);
                        g_object_unref (page);
                        return NULL;
                }

                g_object_ref_sink (page->page);
        }

        return page;
}

static void
emit_initialized (CEPage *page,
                  GError *error)
{
        page->initialized = TRUE;
        g_signal_emit (page, signals[INITIALIZED], 0, error);
        g_clear_error (&error);
}

void
ce_page_complete_init (CEPage      *page,
                       const gchar *setting_name,
                       GVariant    *secrets,
                       GError      *error)
{
	GError *update_error = NULL;
	GVariant *setting_dict;
	gboolean ignore_error = FALSE;

	g_return_if_fail (page != NULL);
	g_return_if_fail (CE_IS_PAGE (page));

	if (error) {
		ignore_error = g_error_matches (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_SETTING_NOT_FOUND) ||
			g_error_matches (error, NM_SECRET_AGENT_ERROR, NM_SECRET_AGENT_ERROR_NO_SECRETS);
	}

	/* Ignore missing settings errors */
	if (error && !ignore_error) {
		emit_initialized (page, error);
		return;
	} else if (!setting_name || !secrets || g_variant_n_children (secrets) == 0) {
		/* Success, no secrets */
		emit_initialized (page, NULL);
		return;
	}

	g_assert (setting_name);
	g_assert (secrets);

	setting_dict = g_variant_lookup_value (secrets, setting_name, NM_VARIANT_TYPE_SETTING);
	if (!setting_dict) {
		/* Success, no secrets */
		emit_initialized (page, NULL);
		return;
	}
	g_variant_unref (setting_dict);

	/* Update the connection with the new secrets */
	if (nm_connection_update_secrets (page->connection,
	                                  setting_name,
	                                  secrets,
	                                  &update_error)) {
		/* Success */
		emit_initialized (page, NULL);
		return;
	}

	g_warning ("Failed to update connection secrets due to an unknown error.");
	emit_initialized (page, NULL);
}

gchar **
ce_page_get_mac_list (NMClient    *client,
                      GType        device_type,
                      const gchar *mac_property)
{
        const GPtrArray *devices;
        GPtrArray *macs;
        int i;

        macs = g_ptr_array_new ();
        devices = nm_client_get_devices (client);
        for (i = 0; devices && (i < devices->len); i++) {
                NMDevice *dev = g_ptr_array_index (devices, i);
                const char *iface;
                char *mac, *item;

                if (!G_TYPE_CHECK_INSTANCE_TYPE (dev, device_type))
                        continue;

                g_object_get (G_OBJECT (dev), mac_property, &mac, NULL);
                iface = nm_device_get_iface (NM_DEVICE (dev));
                item = g_strdup_printf ("%s (%s)", mac, iface);
                g_free (mac);
                g_ptr_array_add (macs, item);
        }

        g_ptr_array_add (macs, NULL);
        return (char **)g_ptr_array_free (macs, FALSE);
}

void
ce_page_setup_mac_combo (GtkComboBoxText  *combo,
                         const gchar      *current_mac,
                         gchar           **mac_list)
{
        gchar **m, *active_mac = NULL;
        gint current_mac_len;
        GtkWidget *entry;

        if (current_mac)
                current_mac_len = strlen (current_mac);
        else
                current_mac_len = -1;

        for (m= mac_list; m && *m; m++) {
                gtk_combo_box_text_append_text (combo, *m);
                if (current_mac &&
                    g_ascii_strncasecmp (*m, current_mac, current_mac_len) == 0
                    && ((*m)[current_mac_len] == '\0' || (*m)[current_mac_len] == ' '))
                        active_mac = *m;
        }

        if (current_mac) {
                if (!active_mac) {
                        gtk_combo_box_text_prepend_text (combo, current_mac);
                }

                entry = gtk_bin_get_child (GTK_BIN (combo));
                if (entry)
                        gtk_entry_set_text (GTK_ENTRY (entry), active_mac ? active_mac : current_mac);
        }
}

gchar *
ce_page_trim_address (const gchar *addr)
{
        char *space;

        if (!addr || *addr == '\0')
                return NULL;

        space = strchr (addr, ' ');
        if (space != NULL)
                return g_strndup (addr, space - addr);
        return g_strdup (addr);
}

gboolean
ce_page_address_is_valid (const gchar *addr)
{
        guint8 invalid_addr[4][ETH_ALEN] = {
                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
                {0x00, 0x30, 0xb4, 0x00, 0x00, 0x00}, /* prism54 dummy MAC */
        };
        guint8 addr_bin[ETH_ALEN];
        char *trimmed_addr;
        guint i;

        if (!addr || *addr == '\0')
                return TRUE;

        trimmed_addr = ce_page_trim_address (addr);

        if (!nm_utils_hwaddr_valid (trimmed_addr, -1)) {
                g_free (trimmed_addr);
                return FALSE;
        }

        if (!nm_utils_hwaddr_aton (trimmed_addr, addr_bin, ETH_ALEN)) {
                g_free (trimmed_addr);
                return FALSE;
        }

        g_free (trimmed_addr);

        /* Check for multicast address */
        if ((((guint8 *) addr_bin)[0]) & 0x01)
                return FALSE;

        for (i = 0; i < G_N_ELEMENTS (invalid_addr); i++) {
                if (nm_utils_hwaddr_matches (addr_bin, ETH_ALEN, invalid_addr[i], ETH_ALEN))
                        return FALSE;
        }

        return TRUE;
}

const gchar *
ce_page_get_security_setting (CEPage *page)
{
        return page->security_setting;
}

gint
ce_get_property_default (NMSetting *setting, const gchar *property_name)
{
        GParamSpec *spec;
        GValue value = { 0, };

        spec = g_object_class_find_property (G_OBJECT_GET_CLASS (setting), property_name);
        g_return_val_if_fail (spec != NULL, -1);

        g_value_init (&value, spec->value_type);
        g_param_value_set_default (spec, &value);

        if (G_VALUE_HOLDS_CHAR (&value))
                return (int) g_value_get_schar (&value);
        else if (G_VALUE_HOLDS_INT (&value))
                return g_value_get_int (&value);
        else if (G_VALUE_HOLDS_INT64 (&value))
                return (int) g_value_get_int64 (&value);
        else if (G_VALUE_HOLDS_LONG (&value))
                return (int) g_value_get_long (&value);
        else if (G_VALUE_HOLDS_UINT (&value))
                return (int) g_value_get_uint (&value);
        else if (G_VALUE_HOLDS_UINT64 (&value))
                return (int) g_value_get_uint64 (&value);
        else if (G_VALUE_HOLDS_ULONG (&value))
                return (int) g_value_get_ulong (&value);
        else if (G_VALUE_HOLDS_UCHAR (&value))
                return (int) g_value_get_uchar (&value);
        g_return_val_if_fail (FALSE, 0);
        return 0;
}

gint
ce_spin_output_with_default (GtkSpinButton *spin, gpointer user_data)
{
        gint defvalue = GPOINTER_TO_INT (user_data);
        gint val;
        gchar *buf = NULL;

        val = gtk_spin_button_get_value_as_int (spin);
        if (val == defvalue)
                buf = g_strdup (_("automatic"));
        else
                buf = g_strdup_printf ("%d", val);

        if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin))))
                gtk_entry_set_text (GTK_ENTRY (spin), buf);

        g_free (buf);
        return TRUE;
}

gchar *
ce_page_get_next_available_name (const GPtrArray *connections,
                                 NameFormat format,
                                 const gchar *type_name)
{
        GSList *names = NULL, *l;
        gchar *cname = NULL;
        gint i = 0;
        guint con_idx;

        for (con_idx = 0; con_idx < connections->len; con_idx++) {
                NMConnection *connection = g_ptr_array_index (connections, con_idx);
                const gchar *id;

                id = nm_connection_get_id (connection);
                g_assert (id);
                names = g_slist_append (names, (gpointer) id);
        }

        /* Find the next available unique connection name */
        while (!cname && (i++ < 10000)) {
                gchar *temp;
                gboolean found = FALSE;

                switch (format) {
                        case NAME_FORMAT_TYPE:
                                temp = g_strdup_printf ("%s %d", type_name, i);
                                break;
                        case NAME_FORMAT_PROFILE:
                                temp = g_strdup_printf (_("Profile %d"), i);
                                break;
                        default:
                                g_assert_not_reached ();
                }

                for (l = names; l; l = l->next) {
                        if (!strcmp (l->data, temp)) {
                                found = TRUE;
                                break;
                        }
                }
                if (!found)
                        cname = temp;
                else
                        g_free (temp);
        }
        g_slist_free (names);

        return cname;
}
