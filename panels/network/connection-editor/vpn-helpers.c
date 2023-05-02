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
	static GSList *plugins = NULL;
	GSList *p;

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
	g_autoptr(GFile) file = NULL;
	ActionInfo *info = (ActionInfo *) user_data;
	NMConnection *connection = NULL;
	g_autoptr(GError) error = NULL;
	GSList *iter;

	if (response != GTK_RESPONSE_ACCEPT)
		goto destroy;

	file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
	if (!file) {
		g_warning ("%s: didn't get a filename back from the chooser!", __func__);
		goto destroy;
	}

	filename = g_file_get_path (file);

#if NM_CHECK_VERSION (1,40,0)
	connection = nm_conn_wireguard_import (filename, &error);
#endif

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
		gtk_window_set_transient_for(GTK_WINDOW(err_dialog),
						GTK_WINDOW(dialog));
		g_signal_connect (err_dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
		gtk_widget_show (err_dialog);
		goto out;
	}

destroy:
	gtk_window_destroy (GTK_WINDOW (dialog));

	info->callback (connection, info->user_data);
	g_free (info);

out:
	return;
}

void
vpn_import (GtkWindow *parent, VpnImportCallback callback, gpointer user_data)
{
	g_autoptr(GFile) home_folder = NULL;
	GtkWidget *dialog;
	ActionInfo *info;

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      parent,
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Open"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	home_folder = g_file_new_for_path (g_get_home_dir ());
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), home_folder, NULL);

	info = g_malloc0 (sizeof (ActionInfo));
	info->callback = callback;
	info->user_data = user_data;

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (import_vpn_from_file_cb), info);
	gtk_window_present (GTK_WINDOW (dialog));
}
