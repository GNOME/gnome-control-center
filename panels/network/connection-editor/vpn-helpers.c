/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "vpn-helpers.h"

NMVpnEditorPlugin *
vpn_get_plugin_by_service (const char *service)
{
        NMVpnPluginInfo *plugin_info;

        g_return_val_if_fail (service != NULL, NULL);

        plugin_info = nm_vpn_plugin_info_list_find_by_service (vpn_get_plugins (), service);
        if (plugin_info)
                return nm_vpn_plugin_info_get_editor_plugin (plugin_info);
        return NULL;
}

static gint
_sort_vpn_plugins (NMVpnPluginInfo *aa, NMVpnPluginInfo *bb)
{
	return strcmp (nm_vpn_plugin_info_get_name (aa), nm_vpn_plugin_info_get_name (bb));
}

GSList *
vpn_get_plugins (void)
{
	static gboolean plugins_loaded = FALSE;
	static GSList *plugins = NULL;
	GSList *p;

	if (G_LIKELY (plugins_loaded))
		return plugins;
	plugins_loaded = TRUE;

	p = nm_vpn_plugin_info_list_load ();
	plugins = NULL;
	while (p) {
		g_autoptr(NMVpnPluginInfo) plugin_info = NM_VPN_PLUGIN_INFO (p->data);
		g_autoptr(GError) error = NULL;

		/* load the editor plugin, and preserve only those NMVpnPluginInfo that can
		 * successfully load the plugin. */
		if (nm_vpn_plugin_info_load_editor_plugin (plugin_info, &error))
			plugins = g_slist_prepend (plugins, g_steal_pointer (&plugin_info));
		else {
			if (   !nm_vpn_plugin_info_get_plugin (plugin_info)
			    && nm_vpn_plugin_info_lookup_property (plugin_info, NM_VPN_PLUGIN_INFO_KF_GROUP_GNOME, "properties")) {
				g_message ("vpn: (%s,%s) cannot load legacy-only plugin",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info));
			} else if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
				g_message ("vpn: (%s,%s) file \"%s\" not found. Did you install the client package?",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info),
				           nm_vpn_plugin_info_get_plugin (plugin_info));
			} else {
				g_warning ("vpn: (%s,%s) could not load plugin: %s",
				           nm_vpn_plugin_info_get_name (plugin_info),
				           nm_vpn_plugin_info_get_filename (plugin_info),
				           error->message);
			}
		}
		p = g_slist_delete_link (p, p);
	}

	/* sort the list of plugins alphabetically. */
	plugins = g_slist_sort (plugins, (GCompareFunc) _sort_vpn_plugins);
	return plugins;
}

typedef struct {
	VpnImportCallback callback;
	gpointer user_data;
} ActionInfo;

static void
import_vpn_from_file_cb (GtkWidget *dialog, gint response, gpointer user_data)
{
	g_autofree gchar *filename = NULL;
	ActionInfo *info = (ActionInfo *) user_data;
	NMConnection *connection = NULL;
	g_autoptr(GError) error = NULL;
	GSList *iter;

	if (response != GTK_RESPONSE_ACCEPT)
		goto out;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	if (!filename) {
		g_warning ("%s: didn't get a filename back from the chooser!", __func__);
		goto out;
	}

	for (iter = vpn_get_plugins (); !connection && iter; iter = iter->next) {
		NMVpnEditorPlugin *plugin;

		plugin = nm_vpn_plugin_info_get_editor_plugin (iter->data);
		g_clear_error (&error);
		connection = nm_vpn_editor_plugin_import (plugin, filename, &error);
	}

	if (!connection) {
		GtkWidget *err_dialog;
		g_autofree gchar *bname = g_path_get_basename (filename);

		err_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
		                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_MESSAGE_ERROR,
		                                     GTK_BUTTONS_OK,
		                                     _("Cannot import VPN connection"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (err_dialog),
		                                 _("The file “%s” could not be read or does not contain recognized VPN connection information\n\nError: %s."),
		                                 bname, error ? error->message : "unknown error");
		g_signal_connect (err_dialog, "delete-event", G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (err_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_dialog_run (GTK_DIALOG (err_dialog));
	}

out:
	gtk_widget_hide (dialog);
	gtk_widget_destroy (dialog);

	info->callback (connection, info->user_data);
	g_free (info);
}

static void
destroy_import_chooser (GtkWidget *dialog, gpointer user_data)
{
	ActionInfo *info = (ActionInfo *) user_data;

	gtk_widget_destroy (dialog);
	info->callback (NULL, info->user_data);
	g_free (info);
}

void
vpn_import (GtkWindow *parent, VpnImportCallback callback, gpointer user_data)
{
	GtkWidget *dialog;
	ActionInfo *info;
	const char *home_folder;

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      parent,
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Open"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	home_folder = g_get_home_dir ();
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), home_folder);

	info = g_malloc0 (sizeof (ActionInfo));
	info->callback = callback;
	info->user_data = user_data;

	g_signal_connect (G_OBJECT (dialog), "close", G_CALLBACK (destroy_import_chooser), info);
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (import_vpn_from_file_cb), info);
	gtk_widget_show_all (dialog);
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
export_vpn_to_file_cb (GtkWidget *dialog, gint response, gpointer user_data)
{
	g_autoptr(NMConnection) connection = NM_CONNECTION (user_data);
	char *filename = NULL;
	g_autoptr(GError) error = NULL;
	NMVpnEditorPlugin *plugin;
	NMSettingConnection *s_con = NULL;
	NMSettingVpn *s_vpn = NULL;
	const char *service_type;
	const char *id = NULL;
	gboolean success = FALSE;

	if (response != GTK_RESPONSE_ACCEPT)
		goto out;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	if (!filename) {
		g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED, "no filename");
		goto done;
	}

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		int replace_response;
		GtkWidget *replace_dialog;
		g_autofree gchar *bname = NULL;

		bname = g_path_get_basename (filename);
		replace_dialog = gtk_message_dialog_new (NULL,
		                                         GTK_DIALOG_DESTROY_WITH_PARENT,
		                                         GTK_MESSAGE_QUESTION,
		                                         GTK_BUTTONS_CANCEL,
		                                         _("A file named “%s” already exists."),
		                                         bname);
		gtk_dialog_add_buttons (GTK_DIALOG (replace_dialog), _("_Replace"), GTK_RESPONSE_OK, NULL);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (replace_dialog),
							  _("Do you want to replace %s with the VPN connection you are saving?"), bname);
		replace_response = gtk_dialog_run (GTK_DIALOG (replace_dialog));
		gtk_widget_destroy (replace_dialog);
		if (replace_response != GTK_RESPONSE_OK)
			goto out;
	}

	s_con = nm_connection_get_setting_connection (connection);
	id = s_con ? nm_setting_connection_get_id (s_con) : NULL;
	if (!id) {
		g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED, "connection setting invalid");
		goto done;
	}

	s_vpn = nm_connection_get_setting_vpn (connection);
	service_type = s_vpn ? nm_setting_vpn_get_service_type (s_vpn) : NULL;

	if (!service_type) {
		g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED, "VPN setting invalid");
		goto done;
	}

	plugin = vpn_get_plugin_by_service (service_type);
	if (plugin)
		success = nm_vpn_editor_plugin_export (plugin, filename, connection, &error);

done:
	if (!success) {
		GtkWidget *err_dialog;
		g_autofree gchar *bname = filename ? g_path_get_basename (filename) : g_strdup ("(none)");

		err_dialog = gtk_message_dialog_new (NULL,
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_MESSAGE_ERROR,
		                                     GTK_BUTTONS_OK,
		                                     _("Cannot export VPN connection"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (err_dialog),
		                                 _("The VPN connection “%s” could not be exported to %s.\n\nError: %s."),
		                                 id ? id : "(unknown)", bname, error ? error->message : "unknown error");
		g_signal_connect (err_dialog, "delete-event", G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (err_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show_all (err_dialog);
		gtk_window_present (GTK_WINDOW (err_dialog));
	}

out:
	gtk_widget_hide (dialog);
	gtk_widget_destroy (dialog);
}

void
vpn_export (NMConnection *connection)
{
	GtkWidget *dialog;
	NMVpnEditorPlugin *plugin;
	NMSettingVpn *s_vpn = NULL;
	const char *service_type;
	const char *home_folder;

	s_vpn = nm_connection_get_setting_vpn (connection);
	service_type = s_vpn ? nm_setting_vpn_get_service_type (s_vpn) : NULL;

	if (!service_type) {
		g_warning ("%s: invalid VPN connection!", __func__);
		return;
	}

	dialog = gtk_file_chooser_dialog_new (_("Export VPN connection"),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Save"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	home_folder = g_get_home_dir ();
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), home_folder);

	plugin = vpn_get_plugin_by_service (service_type);
	if (plugin) {
		g_autofree gchar *suggested = NULL;

		suggested = nm_vpn_editor_plugin_get_suggested_filename (plugin, connection);
		if (suggested)
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested);
	}

	g_signal_connect (G_OBJECT (dialog), "close", G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (export_vpn_to_file_cb), g_object_ref (connection));
	gtk_widget_show_all (dialog);
	gtk_window_present (GTK_WINDOW (dialog));
}
