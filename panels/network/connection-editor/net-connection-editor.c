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

#include <glib-object.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>

#include "list-box-helper.h"
#include "net-connection-editor.h"
#include "net-connection-editor-resources.h"
#include "ce-page-details.h"
#include "ce-page-wifi.h"
#include "ce-page-ip4.h"
#include "ce-page-ip6.h"
#include "ce-page-security.h"
#include "ce-page-ethernet.h"
#include "ce-page-8021x-security.h"
#include "ce-page-vpn.h"
#include "vpn-helpers.h"
#include "eap-method.h"

enum {
        DONE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NetConnectionEditor, net_connection_editor, GTK_TYPE_DIALOG)

static void page_changed (CEPage *page, gpointer user_data);

static void
cancel_editing (NetConnectionEditor *editor)
{
        gtk_widget_hide (GTK_WIDGET (editor));
        g_signal_emit (editor, signals[DONE], 0, FALSE);
}

static void
update_connection (NetConnectionEditor *editor)
{
        GVariant *settings;

        settings = nm_connection_to_dbus (editor->connection, NM_CONNECTION_SERIALIZE_ALL);
        nm_connection_replace_settings (editor->orig_connection, settings, NULL);
        g_variant_unref (settings);
}

static void
update_complete (NetConnectionEditor *editor,
                 gboolean             success)
{
        gtk_widget_hide (GTK_WIDGET (editor));
        g_signal_emit (editor, signals[DONE], 0, success);
}

static void
updated_connection_cb (GObject            *source_object,
                       GAsyncResult       *res,
                       gpointer            user_data)
{
        NetConnectionEditor *editor;
        GError *error = NULL;
        gboolean success = TRUE;

        if (!nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (source_object),
                                                         res, &error)) {
                g_warning ("Failed to commit changes: %s", error->message);
                success = FALSE;
                g_error_free (error);
                //return; FIXME return if cancelled
        }

        nm_connection_clear_secrets (NM_CONNECTION (source_object));

        editor = user_data;
        update_complete (editor, success);
}

static void
added_connection_cb (GObject            *source_object,
                     GAsyncResult       *res,
                     gpointer            user_data)
{
        NetConnectionEditor *editor;
        GError *error = NULL;
        gboolean success = TRUE;

        if (!nm_client_add_connection_finish (NM_CLIENT (source_object), res, &error)) {
                g_warning ("Failed to add connection: %s", error->message);
                success = FALSE;
                g_error_free (error);
                /* Leave the editor open */
                // return; FIXME return if cancelled
        }

        editor = user_data;
        update_complete (editor, success);
}

static void
apply_edits (NetConnectionEditor *editor)
{
        update_connection (editor);

        eap_method_ca_cert_ignore_save (editor->connection);

        if (editor->is_new_connection) {
                nm_client_add_connection_async (editor->client,
                                                editor->orig_connection,
                                                TRUE,
                                                NULL,
                                                added_connection_cb,
                                                editor);
        } else {
                nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (editor->orig_connection),
                                                           TRUE,
                                                           NULL,
                                                           updated_connection_cb, editor);
        }
}

static void
net_connection_editor_init (NetConnectionEditor *editor)
{
        gtk_widget_init_template (GTK_WIDGET (editor));
}

static void
net_connection_editor_finalize (GObject *object)
{
        NetConnectionEditor *editor = NET_CONNECTION_EDITOR (object);
        GSList *l;

        for (l = editor->pages; l != NULL; l = l->next)
                g_signal_handlers_disconnect_by_func (l->data, page_changed, editor);

        if (editor->permission_id > 0 && editor->client)
                g_signal_handler_disconnect (editor->client, editor->permission_id);
        g_clear_object (&editor->connection);
        g_clear_object (&editor->orig_connection);
        g_clear_object (&editor->parent_window);
        g_clear_object (&editor->device);
        g_clear_object (&editor->client);
        g_clear_object (&editor->ap);

        G_OBJECT_CLASS (net_connection_editor_parent_class)->finalize (object);
}

static void
net_connection_editor_class_init (NetConnectionEditorClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

        g_resources_register (net_connection_editor_get_resource ());

        object_class->finalize = net_connection_editor_finalize;

        signals[DONE] = g_signal_new ("done",
                                      G_OBJECT_CLASS_TYPE (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (NetConnectionEditorClass, done),
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/connection-editor.ui");

        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, details_add_connection_frame);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, details_apply_button);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, details_cancel_button);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, details_notebook);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, details_toplevel_notebook);
        gtk_widget_class_bind_template_callback_full (widget_class, "delete_event_cb", G_CALLBACK (cancel_editing));
        gtk_widget_class_bind_template_callback_full (widget_class, "cancel_clicked_cb", G_CALLBACK (cancel_editing));
        gtk_widget_class_bind_template_callback_full (widget_class, "apply_clicked_cb", G_CALLBACK (apply_edits));
}

static void
net_connection_editor_error_dialog (NetConnectionEditor *editor,
                                    const char *primary_text,
                                    const char *secondary_text)
{
        GtkWidget *dialog;
        GtkWindow *parent;

        if (gtk_widget_is_visible (GTK_WIDGET (editor)))
                parent = GTK_WINDOW (editor);
        else
                parent = GTK_WINDOW (editor->parent_window);

        dialog = gtk_message_dialog_new (parent,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         "%s", primary_text);

        if (secondary_text) {
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", secondary_text);
        }

        g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_destroy), NULL);
        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_dialog_run (GTK_DIALOG (dialog));
}

static void
net_connection_editor_do_fallback (NetConnectionEditor *editor, const gchar *type)
{
        gchar *cmdline;
        GError *error = NULL;

        if (editor->is_new_connection) {
                cmdline = g_strdup_printf ("nm-connection-editor --type='%s' --create", type);
        } else {
                cmdline = g_strdup_printf ("nm-connection-editor --edit='%s'",
                                           nm_connection_get_uuid (editor->connection));
        }

        g_spawn_command_line_async (cmdline, &error);
        g_free (cmdline);

        if (error) {
                net_connection_editor_error_dialog (editor,
                                                    _("Unable to open connection editor"),
                                                    error->message);
                g_error_free (error);
        }

        g_signal_emit (editor, signals[DONE], 0, FALSE);
}

static void
net_connection_editor_update_title (NetConnectionEditor *editor)
{
        gchar *id;

        if (editor->title_set)
                return;

        if (editor->is_new_connection) {
                if (editor->device) {
                        id = g_strdup (_("New Profile"));
                } else {
                        /* Leave it set to "Add New Connection" */
                        return;
                }
        } else {
                NMSettingWireless *sw;
                sw = nm_connection_get_setting_wireless (editor->connection);
                if (sw) {
                        GBytes *ssid;
                        ssid = nm_setting_wireless_get_ssid (sw);
                        id = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
                } else {
                        id = g_strdup (nm_connection_get_id (editor->connection));
                }
        }
        gtk_window_set_title (GTK_WINDOW (editor), id);
        g_free (id);
}

static gboolean
editor_is_initialized (NetConnectionEditor *editor)
{
        return editor->initializing_pages == NULL;
}

static void
update_sensitivity (NetConnectionEditor *editor)
{
        NMSettingConnection *sc;
        gboolean sensitive;
        GtkWidget *widget;
        GSList *l;

        if (!editor_is_initialized (editor))
                return;

        sc = nm_connection_get_setting_connection (editor->connection);

        if (nm_setting_connection_get_read_only (sc)) {
                sensitive = FALSE;
        } else {
                sensitive = editor->can_modify;
        }

        for (l = editor->pages; l; l = l->next) {
                widget = ce_page_get_page (CE_PAGE (l->data));
                gtk_widget_set_sensitive (widget, sensitive);
        }
}

static void
validate (NetConnectionEditor *editor)
{
        gboolean valid = FALSE;
        GSList *l;

        if (!editor_is_initialized (editor))
                goto done;

        valid = TRUE;
        for (l = editor->pages; l; l = l->next) {
                GError *error = NULL;

                if (!ce_page_validate (CE_PAGE (l->data), editor->connection, &error)) {
                        valid = FALSE;
                        if (error) {
                                g_debug ("Invalid setting %s: %s", ce_page_get_title (CE_PAGE (l->data)), error->message);
                                g_error_free (error);
                        } else {
                                g_debug ("Invalid setting %s", ce_page_get_title (CE_PAGE (l->data)));
                        }
                }
        }

        update_sensitivity (editor);
done:
        gtk_widget_set_sensitive (editor->details_apply_button, valid && editor->is_changed);
}

static void
page_changed (CEPage *page, gpointer user_data)
{
        NetConnectionEditor *editor= user_data;

        if (editor_is_initialized (editor))
                editor->is_changed = TRUE;
        validate (editor);
}

static gboolean
idle_validate (gpointer user_data)
{
        validate (NET_CONNECTION_EDITOR (user_data));

        return G_SOURCE_REMOVE;
}

static void
recheck_initialization (NetConnectionEditor *editor)
{
        GtkNotebook *notebook;

        if (!editor_is_initialized (editor))
                return;

        notebook = GTK_NOTEBOOK (editor->details_notebook);
        gtk_notebook_set_current_page (notebook, 0);

        if (editor->show_when_initialized)
                gtk_window_present (GTK_WINDOW (editor));

        g_idle_add (idle_validate, editor);
}

static void
page_initialized (CEPage *page, GError *error, NetConnectionEditor *editor)
{
        GtkNotebook *notebook;
        GtkWidget *widget;
        GtkWidget *label;
        gint position;
        GList *children, *l;
        gint i;

        notebook = GTK_NOTEBOOK (editor->details_notebook);
        widget = ce_page_get_page (page);
        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (page), "position"));
        g_object_set_data (G_OBJECT (widget), "position", GINT_TO_POINTER (position));
        children = gtk_container_get_children (GTK_CONTAINER (notebook));
        for (l = children, i = 0; l; l = l->next, i++) {
                gint pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "position"));
                if (pos > position)
                        break;
        }
        g_list_free (children);

        label = gtk_label_new (ce_page_get_title (page));

        gtk_notebook_insert_page (notebook, widget, label, i);

        editor->initializing_pages = g_slist_remove (editor->initializing_pages, page);
        editor->pages = g_slist_append (editor->pages, page);

        recheck_initialization (editor);
}

typedef struct {
        NetConnectionEditor *editor;
        CEPage *page;
        const gchar *setting_name;
} GetSecretsInfo;

static void
get_secrets_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
        NMRemoteConnection *connection;
        GetSecretsInfo *info = user_data;
        GError *error = NULL;
        GVariant *variant;

        connection = NM_REMOTE_CONNECTION (source_object);
        variant = nm_remote_connection_get_secrets_finish (connection, res, &error);

        if (!variant && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                g_error_free (error);
                g_free (info);
                return;
        }

        ce_page_complete_init (info->page, info->setting_name, variant, error);
        g_variant_unref (variant);
        g_free (info);
}

static void
get_secrets_for_page (NetConnectionEditor *editor,
                      CEPage              *page,
                      const gchar         *setting_name)
{
        GetSecretsInfo *info;

        info = g_new0 (GetSecretsInfo, 1);
        info->editor = editor;
        info->page = page;
        info->setting_name = setting_name;

        nm_remote_connection_get_secrets_async (NM_REMOTE_CONNECTION (editor->orig_connection),
                                                setting_name,
                                                NULL, //FIXME
                                                get_secrets_cb,
                                                info);
}

static void
add_page (NetConnectionEditor *editor, CEPage *page)
{
        gint position;

        position = g_slist_length (editor->initializing_pages);
        g_object_set_data (G_OBJECT (page), "position", GINT_TO_POINTER (position));

        editor->initializing_pages = g_slist_append (editor->initializing_pages, page);

        g_signal_connect (page, "changed", G_CALLBACK (page_changed), editor);
        g_signal_connect (page, "initialized", G_CALLBACK (page_initialized), editor);
}

static void
net_connection_editor_set_connection (NetConnectionEditor *editor,
                                      NMConnection        *connection)
{
        GSList *pages, *l;
        NMSettingConnection *sc;
        const gchar *type;
        gboolean is_wired;
        gboolean is_wifi;
        gboolean is_vpn;

        editor->is_new_connection = !nm_client_get_connection_by_uuid (editor->client,
                                                                       nm_connection_get_uuid (connection));

        if (editor->is_new_connection) {
                gtk_button_set_label (GTK_BUTTON (editor->details_apply_button), _("_Add"));
                editor->is_changed = TRUE;
        }

        editor->connection = nm_simple_connection_new_clone (connection);
        editor->orig_connection = g_object_ref (connection);

        net_connection_editor_update_title (editor);

        eap_method_ca_cert_ignore_load (editor->connection);

        sc = nm_connection_get_setting_connection (connection);
        type = nm_setting_connection_get_connection_type (sc);

        is_wired = g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME);
        is_wifi = g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME);
        is_vpn = g_str_equal (type, NM_SETTING_VPN_SETTING_NAME);

        if (!editor->is_new_connection)
                add_page (editor, ce_page_details_new (editor->connection, editor->client, editor->device, editor->ap, editor));

        if (is_wifi)
                add_page (editor, ce_page_wifi_new (editor->connection, editor->client));
        else if (is_wired)
                add_page (editor, ce_page_ethernet_new (editor->connection, editor->client));
        else if (is_vpn)
                add_page (editor, ce_page_vpn_new (editor->connection, editor->client));
        else {
                /* Unsupported type */
                net_connection_editor_do_fallback (editor, type);
                return;
        }

        add_page (editor, ce_page_ip4_new (editor->connection, editor->client));
        add_page (editor, ce_page_ip6_new (editor->connection, editor->client));

        if (is_wifi)
                add_page (editor, ce_page_security_new (editor->connection, editor->client));
        else if (is_wired)
                add_page (editor, ce_page_8021x_security_new (editor->connection, editor->client));

        pages = g_slist_copy (editor->initializing_pages);
        for (l = pages; l; l = l->next) {
                CEPage *page = l->data;
                const gchar *security_setting;

                security_setting = ce_page_get_security_setting (page);
                if (!security_setting || editor->is_new_connection) {
                        ce_page_complete_init (page, NULL, NULL, NULL);
                } else {
                        get_secrets_for_page (editor, page, security_setting);
                }
        }
        g_slist_free (pages);
}

static NMConnection *
complete_vpn_connection (NetConnectionEditor *editor, NMConnection *connection)
{
        NMSettingConnection *s_con;
        NMSetting *s_type;

        if (!connection)
                connection = nm_simple_connection_new ();

        s_con = nm_connection_get_setting_connection (connection);
        if (!s_con) {
                s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
                nm_connection_add_setting (connection, NM_SETTING (s_con));
        }

        if (!nm_setting_connection_get_uuid (s_con)) {
                gchar *uuid = nm_utils_uuid_generate ();
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_UUID, uuid,
                              NULL);
                g_free (uuid);
        }

        if (!nm_setting_connection_get_id (s_con)) {
                const GPtrArray *connections;
                gchar *id;

                connections = nm_client_get_connections (editor->client);
                id = ce_page_get_next_available_name (connections, NAME_FORMAT_TYPE, _("VPN"));
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_ID, id,
                              NULL);
                g_free (id);
        }

        s_type = nm_connection_get_setting (connection, NM_TYPE_SETTING_VPN);
        if (!s_type) {
                s_type = g_object_new (NM_TYPE_SETTING_VPN, NULL);
                nm_connection_add_setting (connection, s_type);
        }

        if (!nm_setting_connection_get_connection_type (s_con)) {
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_TYPE, nm_setting_get_name (s_type),
                              NULL);
        }

        return connection;
}

static void
finish_add_connection (NetConnectionEditor *editor, NMConnection *connection)
{
        GtkStack *stack;
        GtkBin *frame;

        frame = GTK_BIN (editor->details_add_connection_frame);
        gtk_widget_destroy (gtk_bin_get_child (frame));

        stack = GTK_STACK (editor->details_toplevel_notebook);
        gtk_stack_set_visible_child_name (stack, "details");
        gtk_widget_show (editor->details_apply_button);

        if (connection)
                net_connection_editor_set_connection (editor, connection);
}

static void
vpn_import_complete (NMConnection *connection, gpointer user_data)
{
        NetConnectionEditor *editor = user_data;

        if (!connection) {
                /* The import code shows its own error dialogs. */
                g_signal_emit (editor, signals[DONE], 0, FALSE);
                return;
        }

        complete_vpn_connection (editor, connection);
        finish_add_connection (editor, connection);
}

static void
vpn_type_activated (GtkListBox *list, GtkWidget *row, NetConnectionEditor *editor)
{
        const char *service_name = g_object_get_data (G_OBJECT (row), "service_name");
        NMConnection *connection;
        NMSettingVpn *s_vpn;
        NMSettingConnection *s_con;

        if (!strcmp (service_name, "import")) {
                vpn_import (GTK_WINDOW (editor), vpn_import_complete, editor);
                return;
        }

        connection = complete_vpn_connection (editor, NULL);
        s_vpn = nm_connection_get_setting_vpn (connection);
        g_object_set (s_vpn, NM_SETTING_VPN_SERVICE_TYPE, service_name, NULL);

        /* Mark the connection as private to this user, and non-autoconnect */
        s_con = nm_connection_get_setting_connection (connection);
        g_object_set (s_con, NM_SETTING_CONNECTION_AUTOCONNECT, FALSE, NULL);
        nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);

        finish_add_connection (editor, connection);
}

static void
select_vpn_type (NetConnectionEditor *editor, GtkListBox *list)
{
        GSList *vpn_plugins, *iter;
        GList *l;
        GList *children;
        GtkWidget *row, *row_box;
        GtkWidget *name_label, *desc_label;

        /* Get the available VPN types */
        vpn_plugins = vpn_get_plugins ();

        /* Remove the previous menu contents */
        children = gtk_container_get_children (GTK_CONTAINER (list));
        for (l = children; l != NULL; l = l->next)
                gtk_widget_destroy (l->data);

        /* Add the VPN types */
        for (iter = vpn_plugins; iter; iter = iter->next) {
                NMVpnEditorPlugin *plugin = nm_vpn_plugin_info_get_editor_plugin (iter->data);
                char *name, *desc, *desc_markup, *service_name;
                GtkStyleContext *context;

                g_object_get (plugin,
                              NM_VPN_EDITOR_PLUGIN_NAME, &name,
                              NM_VPN_EDITOR_PLUGIN_DESCRIPTION, &desc,
                              NM_VPN_EDITOR_PLUGIN_SERVICE, &service_name,
                              NULL);
                desc_markup = g_markup_printf_escaped ("<span size='smaller'>%s</span>", desc);

                row = gtk_list_box_row_new ();

                row_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_set_margin_start (row_box, 12);
                gtk_widget_set_margin_end (row_box, 12);
                gtk_widget_set_margin_top (row_box, 12);
                gtk_widget_set_margin_bottom (row_box, 12);

                name_label = gtk_label_new (name);
                gtk_widget_set_halign (name_label, GTK_ALIGN_START);
                gtk_box_pack_start (GTK_BOX (row_box), name_label, FALSE, TRUE, 0);

                desc_label = gtk_label_new (NULL);
                gtk_label_set_markup (GTK_LABEL (desc_label), desc_markup);
                gtk_label_set_line_wrap (GTK_LABEL (desc_label), TRUE);
                gtk_widget_set_halign (desc_label, GTK_ALIGN_START);
                context = gtk_widget_get_style_context (desc_label);
                gtk_style_context_add_class (context, "dim-label");
                gtk_box_pack_start (GTK_BOX (row_box), desc_label, FALSE, TRUE, 0);

                g_free (name);
                g_free (desc);
                g_free (desc_markup);

                gtk_container_add (GTK_CONTAINER (row), row_box);
                gtk_widget_show_all (row);
                g_object_set_data_full (G_OBJECT (row), "service_name", service_name, g_free);
                gtk_container_add (GTK_CONTAINER (list), row);
        }

        /* Import */
        row = gtk_list_box_row_new ();

        row_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start (row_box, 12);
        gtk_widget_set_margin_end (row_box, 12);
        gtk_widget_set_margin_top (row_box, 12);
        gtk_widget_set_margin_bottom (row_box, 12);

        name_label = gtk_label_new (_("Import from file…"));
        gtk_widget_set_halign (name_label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (row_box), name_label, FALSE, TRUE, 0);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        g_object_set_data (G_OBJECT (row), "service_name", "import");
        gtk_container_add (GTK_CONTAINER (list), row);

        g_signal_connect (list, "row-activated",
                          G_CALLBACK (vpn_type_activated), editor);
}

static void
net_connection_editor_add_connection (NetConnectionEditor *editor)
{
        GtkStack *stack;
        GtkContainer *frame;
        GtkListBox *list;

        stack = GTK_STACK (editor->details_toplevel_notebook);
        frame = GTK_CONTAINER (editor->details_add_connection_frame);

        list = GTK_LIST_BOX (gtk_list_box_new ());
        gtk_list_box_set_selection_mode (list, GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (list, cc_list_box_update_header_func, NULL, NULL);

        select_vpn_type (editor, list);

        gtk_widget_show_all (GTK_WIDGET (list));
        gtk_container_add (frame, GTK_WIDGET (list));

        gtk_stack_set_visible_child_name (stack, "vpn");
        gtk_widget_hide (editor->details_apply_button);
        gtk_window_set_title (GTK_WINDOW (editor), _("Add VPN"));
}

static void
permission_changed (NMClient                 *client,
                    NMClientPermission        permission,
                    NMClientPermissionResult  result,
                    NetConnectionEditor      *editor)
{
        if (permission != NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM)
                return;

        if (result == NM_CLIENT_PERMISSION_RESULT_YES || result == NM_CLIENT_PERMISSION_RESULT_AUTH)
                editor->can_modify = TRUE;
        else
                editor->can_modify = FALSE;

        validate (editor);
}

NetConnectionEditor *
net_connection_editor_new (GtkWindow        *parent_window,
                           NMConnection     *connection,
                           NMDevice         *device,
                           NMAccessPoint    *ap,
                           NMClient         *client)
{
        NetConnectionEditor *editor;

        editor = g_object_new (NET_TYPE_CONNECTION_EDITOR,
                               /* This doesn't seem to work for a template, so it is also hardcoded. */
                               "use-header-bar", 1,
                               NULL);

        if (parent_window) {
                editor->parent_window = GTK_WIDGET (g_object_ref (parent_window));
                gtk_window_set_transient_for (GTK_WINDOW (editor),
                                              parent_window);
        }
        if (ap)
                editor->ap = g_object_ref (ap);
        if (device)
                editor->device = g_object_ref (device);
        editor->client = g_object_ref (client);

        editor->can_modify = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);
        editor->permission_id = g_signal_connect (editor->client, "permission-changed",
                                                  G_CALLBACK (permission_changed), editor);

        if (connection)
                net_connection_editor_set_connection (editor, connection);
        else
                net_connection_editor_add_connection (editor);

        return editor;
}

void
net_connection_editor_run (NetConnectionEditor *editor)
{
        if (!editor_is_initialized (editor)) {
                editor->show_when_initialized = TRUE;
                return;
        }
        gtk_window_present (GTK_WINDOW (editor));
}

static void
forgotten_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
        NMRemoteConnection *connection = NM_REMOTE_CONNECTION (source_object);
        NetConnectionEditor *editor = user_data;
        GError *error = NULL;

        if (!nm_remote_connection_delete_finish (connection, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to delete connection %s: %s",
                                   nm_connection_get_id (NM_CONNECTION (connection)),
                                   error->message);
                g_error_free (error);
                return;
        }

        cancel_editing (editor);
}

void
net_connection_editor_forget (NetConnectionEditor *editor)
{
        nm_remote_connection_delete_async (NM_REMOTE_CONNECTION (editor->orig_connection),
                                           NULL, forgotten_cb, editor);
}

void
net_connection_editor_reset (NetConnectionEditor *editor)
{
        GVariant *settings;

        settings = nm_connection_to_dbus (editor->orig_connection, NM_CONNECTION_SERIALIZE_ALL);
        nm_connection_replace_settings (editor->connection, settings, NULL);
        g_variant_unref (settings);
}

void
net_connection_editor_set_title (NetConnectionEditor *editor,
                                 const gchar         *title)
{
        gtk_window_set_title (GTK_WINDOW (editor), title);
        editor->title_set = TRUE;
}
