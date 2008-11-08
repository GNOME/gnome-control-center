/* gnome-network-preferences.c: network preferences capplet
 *
 * Copyright (C) 2002 Sun Microsystems Inc.
 *
 * Written by: Mark McLoughlin <mark@skynet.ie>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

enum ProxyMode
{
	PROXYMODE_NONE,
	PROXYMODE_MANUAL,
	PROXYMODE_AUTO
};

static GEnumValue proxytype_values[] = {
	{ PROXYMODE_NONE, "PROXYMODE_NONE", "none"},
	{ PROXYMODE_MANUAL, "PROXYMODE_MANUAL", "manual"},
	{ PROXYMODE_AUTO, "PROXYMODE_AUTO", "auto"},
	{ 0, NULL, NULL }
};

enum {
	COL_NAME,
	COL_STYLE
};

#define USE_PROXY_KEY   "/system/http_proxy/use_http_proxy"
#define USE_SAME_PROXY_KEY   "/system/http_proxy/use_same_proxy"
#define HTTP_PROXY_HOST_KEY  "/system/http_proxy/host"
#define HTTP_PROXY_PORT_KEY  "/system/http_proxy/port"
#define HTTP_USE_AUTH_KEY    "/system/http_proxy/use_authentication"
#define HTTP_AUTH_USER_KEY   "/system/http_proxy/authentication_user"
#define HTTP_AUTH_PASSWD_KEY "/system/http_proxy/authentication_password"
#define IGNORE_HOSTS_KEY	 "/system/http_proxy/ignore_hosts"
#define PROXY_MODE_KEY "/system/proxy/mode"
#define SECURE_PROXY_HOST_KEY  "/system/proxy/secure_host"
#define OLD_SECURE_PROXY_HOST_KEY  "/system/proxy/old_secure_host"
#define SECURE_PROXY_PORT_KEY  "/system/proxy/secure_port"
#define OLD_SECURE_PROXY_PORT_KEY  "/system/proxy/old_secure_port"
#define FTP_PROXY_HOST_KEY  "/system/proxy/ftp_host"
#define OLD_FTP_PROXY_HOST_KEY  "/system/proxy/old_ftp_host"
#define FTP_PROXY_PORT_KEY  "/system/proxy/ftp_port"
#define OLD_FTP_PROXY_PORT_KEY  "/system/proxy/old_ftp_port"
#define SOCKS_PROXY_HOST_KEY  "/system/proxy/socks_host"
#define OLD_SOCKS_PROXY_HOST_KEY  "/system/proxy/old_socks_host"
#define SOCKS_PROXY_PORT_KEY  "/system/proxy/socks_port"
#define OLD_SOCKS_PROXY_PORT_KEY  "/system/proxy/old_socks_port"
#define PROXY_AUTOCONFIG_URL_KEY  "/system/proxy/autoconfig_url"

#define LOCATION_DIR "/apps/control-center/network"
#define CURRENT_LOCATION "/apps/control-center/network/current_location"

static GtkWidget *details_dialog = NULL;
static GSList *ignore_hosts = NULL;
static GtkTreeModel *model = NULL;

static GtkTreeModel *
create_listmodel(void)
{
	GtkListStore *store;

	store = gtk_list_store_new(1, G_TYPE_STRING);

	return GTK_TREE_MODEL(store);
}

static GtkTreeModel *
populate_listmodel(GtkListStore *store, GSList *list)
{
	GtkTreeIter iter;
	GSList *pointer;

	gtk_list_store_clear(store);

	pointer = list;
	while(pointer)
	{
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, (char *) pointer->data, -1);
		pointer = g_slist_next(pointer);
	}

	return GTK_TREE_MODEL(store);
}

static GtkWidget *
config_treeview(GtkTreeView *tree, GtkTreeModel *model)
{
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree),
												-1, "Hosts", renderer,
												"text", 0, NULL);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), model);

	return GTK_WIDGET(tree);
}

static void
cb_add_url (GtkButton *button, gpointer data)
{
	GladeXML *dialog = (GladeXML *) data;
	gchar *new_url = NULL;
	GConfClient *client;

	new_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(WID("entry_url"))));
	if (strlen (new_url) == 0)
		return;
	ignore_hosts = g_slist_append(ignore_hosts, new_url);
	populate_listmodel(GTK_LIST_STORE(model), ignore_hosts);
	gtk_entry_set_text(GTK_ENTRY(WID("entry_url")), "");

	client = gconf_client_get_default ();
	gconf_client_set_list (client, IGNORE_HOSTS_KEY, GCONF_VALUE_STRING, ignore_hosts, NULL);
	g_object_unref (client);
}

static void
cb_remove_url (GtkButton *button, gpointer data)
{
	GladeXML *dialog = (GladeXML *) data;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GConfClient *client;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(WID("treeview_ignore_host")));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gchar *url;
		GSList *pointer;

		gtk_tree_model_get (model, &iter, 0, &url, -1);

		pointer = ignore_hosts;
		while(pointer)
		{
			if(strcmp(url, (char *) pointer->data) == 0)
			{
				g_free (pointer->data);
				ignore_hosts = g_slist_delete_link(ignore_hosts, pointer);
				break;
			}
			pointer = g_slist_next(pointer);
		}

		g_free(url);
		populate_listmodel(GTK_LIST_STORE(model), ignore_hosts);

		client = gconf_client_get_default ();
		gconf_client_set_list(client, IGNORE_HOSTS_KEY, GCONF_VALUE_STRING, ignore_hosts, NULL);
		g_object_unref (client);
	}
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"goscustdesk-50");
	else if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT)
	{
		if (ignore_hosts) {
			g_slist_foreach (ignore_hosts, (GFunc) g_free, NULL);
			g_slist_free (ignore_hosts);
		}

		gtk_main_quit ();
	}
}

static void
cb_details_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "goscustdesk-50");
	else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		details_dialog = NULL;
	}
}

static void
cb_use_auth_toggled (GtkToggleButton *toggle,
		     GtkWidget *table)
{
	gtk_widget_set_sensitive (table, toggle->active);
}

static void
cb_http_details_button_clicked (GtkWidget *button,
			        GtkWidget *parent)
{
	GladeXML  *dialog;
	GtkWidget *widget;
	GConfPropertyEditor *peditor;

	if (details_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (details_dialog));
		gtk_widget_grab_focus (details_dialog);
		return;
	}

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-network-preferences.glade",
				"details_dialog", NULL);

	details_dialog = widget = WID ("details_dialog");

	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (parent));

	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("use_auth_checkbutton"))->child), TRUE);

	g_signal_connect (G_OBJECT (WID ("use_auth_checkbutton")),
			  "toggled",
			  G_CALLBACK (cb_use_auth_toggled),
			  WID ("auth_table"));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (
			NULL, HTTP_USE_AUTH_KEY, WID ("use_auth_checkbutton"),
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_AUTH_USER_KEY, WID ("username_entry"),
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_AUTH_PASSWD_KEY, WID ("password_entry"),
			NULL));

	g_signal_connect (widget, "response",
			  G_CALLBACK (cb_details_dialog_response), NULL);

	capplet_set_icon (widget, "gnome-network-preferences");

	gtk_widget_show_all (widget);
}

static gchar *
copy_location_create_key (const gchar *from, const gchar *what)
{
	if (from[0] == '\0') return g_strdup (what);
	else return g_strconcat (from, what + strlen ("/system"), NULL);
}

static void
copy_location (const gchar *from, const gchar *to, GConfClient *client)
{
	int ti;
	gboolean tb;
	GSList *tl;
	gchar *tstr, *dest, *src;

	if (from[0] != '\0' && !gconf_client_dir_exists (client, from, NULL))
		return;

	/* USE_PROXY */
	dest = copy_location_create_key (to, USE_PROXY_KEY);
	src = copy_location_create_key (from, USE_PROXY_KEY);

	tb = gconf_client_get_bool (client, src, NULL);
	gconf_client_set_bool (client, dest, tb, NULL);

	g_free (dest);
	g_free (src);

	/* USE_SAME_PROXY */
	dest = copy_location_create_key (to, USE_SAME_PROXY_KEY);
	src = copy_location_create_key (from, USE_SAME_PROXY_KEY);

	tb = gconf_client_get_bool (client, src, NULL);
	gconf_client_set_bool (client, dest, tb, NULL);

	g_free (dest);
	g_free (src);

	/* HTTP_PROXY_HOST */
	dest = copy_location_create_key (to, HTTP_PROXY_HOST_KEY);
	src = copy_location_create_key (from, HTTP_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* HTTP_PROXY_PORT */
	dest = copy_location_create_key (to, HTTP_PROXY_PORT_KEY);
	src = copy_location_create_key (from, HTTP_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* HTTP_USE_AUTH */
	dest = copy_location_create_key (to, HTTP_USE_AUTH_KEY);
	src = copy_location_create_key (from, HTTP_USE_AUTH_KEY);

	tb = gconf_client_get_bool (client, src, NULL);
	gconf_client_set_bool (client, dest, tb, NULL);

	g_free (dest);
	g_free (src);

	/* HTTP_AUTH_USER */
	dest = copy_location_create_key (to, HTTP_AUTH_USER_KEY);
	src = copy_location_create_key (from, HTTP_AUTH_USER_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* HTTP_AUTH_PASSWD */
	dest = copy_location_create_key (to, HTTP_AUTH_PASSWD_KEY);
	src = copy_location_create_key (from, HTTP_AUTH_PASSWD_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		 gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* IGNORE_HOSTS */
	dest = copy_location_create_key (to, IGNORE_HOSTS_KEY);
	src = copy_location_create_key (from, IGNORE_HOSTS_KEY);

	tl = gconf_client_get_list (client, src, GCONF_VALUE_STRING, NULL);
	gconf_client_set_list (client, dest, GCONF_VALUE_STRING, tl, NULL);
	g_slist_foreach (tl, (GFunc) g_free, NULL);
	g_slist_free (tl);

	g_free (dest);
	g_free (src);

	/* PROXY_MODE */
	dest = copy_location_create_key (to, PROXY_MODE_KEY);
	src = copy_location_create_key (from, PROXY_MODE_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* SECURE_PROXY_HOST */
	dest = copy_location_create_key (to, SECURE_PROXY_HOST_KEY);
	src = copy_location_create_key (from, SECURE_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* OLD_SECURE_PROXY_HOST */
	dest = copy_location_create_key (to, OLD_SECURE_PROXY_HOST_KEY);
	src = copy_location_create_key (from, OLD_SECURE_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* SECURE_PROXY_PORT */
	dest = copy_location_create_key (to, SECURE_PROXY_PORT_KEY);
	src = copy_location_create_key (from, SECURE_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* OLD_SECURE_PROXY_PORT */
	dest = copy_location_create_key (to, OLD_SECURE_PROXY_PORT_KEY);
	src = copy_location_create_key (from, OLD_SECURE_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* FTP_PROXY_HOST */
	dest = copy_location_create_key (to, FTP_PROXY_HOST_KEY);
	src = copy_location_create_key (from, FTP_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* OLD_FTP_PROXY_HOST */
	dest = copy_location_create_key (to, OLD_FTP_PROXY_HOST_KEY);
	src = copy_location_create_key (from, OLD_FTP_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* FTP_PROXY_PORT */
	dest = copy_location_create_key (to, FTP_PROXY_PORT_KEY);
	src = copy_location_create_key (from, FTP_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* OLD_FTP_PROXY_PORT */
	dest = copy_location_create_key (to, OLD_FTP_PROXY_PORT_KEY);
	src = copy_location_create_key (from, OLD_FTP_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* SOCKS_PROXY_HOST */
	dest = copy_location_create_key (to, SOCKS_PROXY_HOST_KEY);
	src = copy_location_create_key (from, SOCKS_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free	(dest);
	g_free (src);

	/* OLD_SOCKS_PROXY_HOST */
	dest = copy_location_create_key (to, OLD_SOCKS_PROXY_HOST_KEY);
	src = copy_location_create_key (from, OLD_SOCKS_PROXY_HOST_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);

	/* SOCKS_PROXY_PORT */
	dest = copy_location_create_key (to, SOCKS_PROXY_PORT_KEY);
	src = copy_location_create_key (from, SOCKS_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* OLD_SOCKS_PROXY_PORT */
	dest = copy_location_create_key (to, OLD_SOCKS_PROXY_PORT_KEY);
	src = copy_location_create_key (from, OLD_SOCKS_PROXY_PORT_KEY);

	ti = gconf_client_get_int (client, src, NULL);
	gconf_client_set_int (client, dest, ti, NULL);

	g_free (dest);
	g_free (src);

	/* PROXY_AUTOCONFIG_URL */
	dest = copy_location_create_key (to, PROXY_AUTOCONFIG_URL_KEY);
	src = copy_location_create_key (from, PROXY_AUTOCONFIG_URL_KEY);

	tstr = gconf_client_get_string (client, src, NULL);
	if (tstr != NULL)
	{
		gconf_client_set_string (client, dest, tstr, NULL);
		g_free (tstr);
	}

	g_free (dest);
	g_free (src);
}

static gchar *
get_current_location (GConfClient *client)
{
	gchar *result;

	result = gconf_client_get_string (client, CURRENT_LOCATION, NULL);

	if (result == NULL || result[0] == '\0')
	{
		g_free (result);
		result = g_strdup (_("Default"));
	}

	return result;
}

static gboolean
location_combo_separator (GtkTreeModel *model,
			  GtkTreeIter *iter,
			  gpointer data)
{
	gchar *name;
	gboolean ret;

	gtk_tree_model_get (model, iter, COL_NAME, &name, -1);

	ret = name == NULL || name[0] == '\0';

	g_free (name);

	return ret;
}

static void
update_locations (GConfClient *client,
		  GladeXML *dialog);

static void
cb_location_changed (GtkWidget *location,
		     GladeXML *dialog);

static void
cb_current_location (GConfClient *client,
		     guint cnxn_id,
		     GConfEntry *entry,
		     GladeXML *dialog)
{
	GConfValue *value;
	const gchar *newval;

	value = gconf_entry_get_value (entry);
	if (value == NULL)
		return;

	newval = gconf_value_get_string (value);
	if (newval == NULL)
		return;

	/* prevent the current settings from being saved by blocking
	 * the signal handler */
	g_signal_handlers_block_by_func (WID ("location_combobox"),
	                                 cb_location_changed, dialog);
	update_locations (client, dialog);
	g_signal_handlers_unblock_by_func (WID ("location_combobox"),
	                                   cb_location_changed, dialog);
}

static void
update_locations (GConfClient *client,
		  GladeXML *dialog)
{
	int i, select;
	gchar *current;
	GtkComboBox *location = GTK_COMBO_BOX (WID ("location_combobox"));
	GSList *list = gconf_client_all_dirs (client, LOCATION_DIR, NULL);
	GtkTreeIter titer;
	GtkListStore *store;
	GSList *iter, *last;

	store = GTK_LIST_STORE (gtk_combo_box_get_model (location));
	gtk_list_store_clear (store);

	current = get_current_location (client);

	list = g_slist_append (list, g_strconcat (LOCATION_DIR"/", current, NULL));
	list = g_slist_sort (list, (GCompareFunc) strcmp);

	select = -1;

	for (i = 0, iter = list, last = NULL; iter != NULL; last = iter, iter = g_slist_next (iter), ++i)
	{
		if (last == NULL || strcmp (last->data, iter->data) != 0)
		{
			gchar *locp, *key_name;

			locp = iter->data + strlen (LOCATION_DIR) + 1;
			key_name = gconf_unescape_key (locp, -1);

			gtk_list_store_append (store, &titer);
			gtk_list_store_set (store, &titer,
						COL_NAME, key_name,
						COL_STYLE, PANGO_STYLE_NORMAL, -1);

			g_free (key_name);

			if (strcmp (locp, current) == 0)
				select = i;
		}
	}
	if (select == -1)
	{
		gtk_list_store_append (store, &titer);
		gtk_list_store_set (store, &titer,
				    COL_NAME , current,
				    COL_STYLE, PANGO_STYLE_NORMAL, -1);
		select = i++;
	}
	gtk_widget_set_sensitive (WID ("delete_button"), i > 1);

	gtk_list_store_append (store, &titer);
	gtk_list_store_set (store, &titer,
			    COL_NAME, NULL,
			    COL_STYLE, PANGO_STYLE_NORMAL, -1);

	gtk_list_store_append (store, &titer);
	gtk_list_store_set (store, &titer,
			    COL_NAME, _("New Location..."),
			    COL_STYLE, PANGO_STYLE_ITALIC, -1);

	gtk_combo_box_set_row_separator_func (location, location_combo_separator, NULL, NULL);
	gtk_combo_box_set_active (location, select);
	g_free (current);
	g_slist_foreach (list, (GFunc) gconf_entry_free, NULL);
	g_slist_free (list);
}

static void
cb_location_new_text_changed (GtkEntry *entry, GladeXML *dialog)
{
	gboolean exists;
	gchar *current, *esc, *key;
	const gchar *name;
	GConfClient *client;

	client = gconf_client_get_default ();

	name = gtk_entry_get_text (entry);
	if (name != NULL && name[0] != '\0')
	{
		esc = gconf_escape_key (name, -1);

		key = g_strconcat (LOCATION_DIR "/", esc, NULL);
		g_free (esc);

		current = get_current_location (client);

		exists = (strcmp (current, name) == 0) ||
		          gconf_client_dir_exists (client, key, NULL);
		g_free (key);
	} else exists = FALSE;

	g_object_unref (client);

	if (exists)
		gtk_widget_show (WID ("error_label"));
	else
		gtk_widget_hide (WID ("error_label"));

	gtk_widget_set_sensitive (WID ("new_location"), !exists);
}

static void
location_new (GladeXML *capplet, GtkWidget *parent)
{
	GladeXML *dialog;
	GtkWidget *askdialog;
	const gchar *name;
	int response;
	GConfClient *client;

	client = gconf_client_get_default ();

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-network-preferences.glade",
				"location_new_dialog", NULL);

	gtk_button_set_image (GTK_BUTTON (WID ("new_location")),
			      gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	askdialog = WID ("location_new_dialog");
	gtk_window_set_transient_for (GTK_WINDOW (askdialog), GTK_WINDOW (parent));
	g_signal_connect (askdialog, "response",
			  G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (WID ("text"), "changed",
			  G_CALLBACK (cb_location_new_text_changed), dialog);
	response = gtk_dialog_run (GTK_DIALOG (askdialog));
	name = gtk_entry_get_text (GTK_ENTRY (WID ("text")));
	g_object_unref (dialog);

	if (response == GTK_RESPONSE_OK && name[0] != '\0')
	{
		gboolean exists;
		gchar *current, *esc, *key;
		esc = gconf_escape_key (name, -1);
		key = g_strconcat (LOCATION_DIR "/", esc, NULL);
		g_free (esc);

		current = get_current_location (client);

		exists = (strcmp (current, name) == 0) ||
		         gconf_client_dir_exists (client, key, NULL);

		g_free (key);

		if (!exists)
		{
			esc = gconf_escape_key (current, -1);
			g_free (current);
			key = g_strconcat (LOCATION_DIR "/", esc, NULL);
			g_free (esc);

			copy_location ("", key, client);
			g_free (key);

			gconf_client_set_string (client, CURRENT_LOCATION, name, NULL);
			update_locations (client, capplet);
		}
		else
		{
			GtkWidget *err = gtk_message_dialog_new (GTK_WINDOW (askdialog),
			                                  GTK_DIALOG_DESTROY_WITH_PARENT,
			                                  GTK_MESSAGE_ERROR,
			                                  GTK_BUTTONS_CLOSE,
			                                  _("Location already exists"));
			gtk_dialog_run (GTK_DIALOG (err));
			gtk_widget_destroy (err);

			/* switch back to the currently selected location */
			gconf_client_notify (client, CURRENT_LOCATION);
		}
	}
	else
	{
		/* switch back to the currently selected location */
		gconf_client_notify (client, CURRENT_LOCATION);
	}
	gtk_widget_destroy (askdialog);
	g_object_unref (client);
}

static void
cb_location_changed (GtkWidget *location,
		     GladeXML *dialog)
{
	gchar *current;
	gchar *name = gtk_combo_box_get_active_text (GTK_COMBO_BOX (location));
	GConfClient *client;

	if (name == NULL)
		return;

	client = gconf_client_get_default ();

	current = get_current_location (client);

	if (strcmp (current, name) != 0)
	{
		if (strcmp (name, _("New Location...")) == 0)
		{
			location_new (dialog, WID ("network_dialog"));
		}
		else
		{
			gchar *key, *esc;

			/* save current settings */
			esc = gconf_escape_key (current, -1);
			key = g_strconcat (LOCATION_DIR "/", esc, NULL);
			g_free (esc);

			copy_location ("", key, client);
			g_free (key);

			/* load settings */
			esc = gconf_escape_key (name, -1);
			key = g_strconcat (LOCATION_DIR "/", esc, NULL);
			g_free (esc);

			copy_location (key, "", client);
			gconf_client_recursive_unset (client, key,
			                              GCONF_UNSET_INCLUDING_SCHEMA_NAMES, NULL);
			g_free (key);

			gconf_client_set_string (client, CURRENT_LOCATION, name, NULL);
		}
	}

	g_free (current);
	g_free (name);
	g_object_unref (client);
}

static void
cb_delete_button_clicked (GtkWidget *button,
			  GladeXML *dialog)
{
	GConfClient *client;
	GtkComboBox *box = GTK_COMBO_BOX (WID ("location_combobox"));
	int active = gtk_combo_box_get_active (box);
	gchar *current, *key, *esc;

	/* prevent the current settings from being saved by blocking
	 * the signal handler */
	g_signal_handlers_block_by_func (box, cb_location_changed, dialog);
	gtk_combo_box_set_active (box, (active == 0) ? 1 : 0);
	gtk_combo_box_remove_text (box, active);
	g_signal_handlers_unblock_by_func (box, cb_location_changed, dialog);

	/* set the new location */
	client = gconf_client_get_default ();
	current = gtk_combo_box_get_active_text (box);

	esc = gconf_escape_key (current, -1);
	key = g_strconcat (LOCATION_DIR "/", esc, NULL);
	g_free (esc);

	copy_location (key, "", client);
	gconf_client_recursive_unset (client, key,
	                              GCONF_UNSET_INCLUDING_SCHEMA_NAMES, NULL);
	gconf_client_suggest_sync (client, NULL);
	g_free (key);

	gconf_client_set_string (client, CURRENT_LOCATION, current, NULL);

	g_free (current);

	g_object_unref (client);
}

static void
cb_use_same_proxy_checkbutton_clicked (GtkWidget *checkbutton,
					GladeXML *dialog)
{
	GConfClient *client;
	gboolean same_proxy;
	gchar *http_proxy;
	gint http_port;
	gchar *host;

	client = gconf_client_get_default ();
	same_proxy = gconf_client_get_bool (client, USE_SAME_PROXY_KEY, NULL);

	http_proxy = gconf_client_get_string (client, HTTP_PROXY_HOST_KEY, NULL);
	http_port = gconf_client_get_int (client, HTTP_PROXY_PORT_KEY, NULL);

	if (same_proxy)
	{
		/* Save the old values */
		host = gconf_client_get_string (client, SECURE_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, OLD_SECURE_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, OLD_SECURE_PROXY_PORT_KEY,
			gconf_client_get_int (client, SECURE_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);

		host = gconf_client_get_string (client, FTP_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, OLD_FTP_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, OLD_FTP_PROXY_PORT_KEY,
			gconf_client_get_int (client, FTP_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);

		host = gconf_client_get_string (client, SOCKS_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, OLD_SOCKS_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, OLD_SOCKS_PROXY_PORT_KEY,
			gconf_client_get_int (client, SOCKS_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);

		/* Set the new values */
		gconf_client_set_string (client, SECURE_PROXY_HOST_KEY, http_proxy, NULL);
		gconf_client_set_int (client, SECURE_PROXY_PORT_KEY, http_port, NULL);

		gconf_client_set_string (client, FTP_PROXY_HOST_KEY, http_proxy, NULL);
		gconf_client_set_int (client, FTP_PROXY_PORT_KEY, http_port, NULL);

		gconf_client_set_string (client, SOCKS_PROXY_HOST_KEY, http_proxy, NULL);
		gconf_client_set_int (client, SOCKS_PROXY_PORT_KEY, http_port, NULL);
	}
	else
	{
		host = gconf_client_get_string (client, OLD_SECURE_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, SECURE_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, SECURE_PROXY_PORT_KEY,
			gconf_client_get_int (client, OLD_SECURE_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);

		host = gconf_client_get_string (client, OLD_FTP_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, FTP_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, FTP_PROXY_PORT_KEY,
			gconf_client_get_int (client, OLD_FTP_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);

		host = gconf_client_get_string (client, OLD_SOCKS_PROXY_HOST_KEY, NULL);
		gconf_client_set_string (client, SOCKS_PROXY_HOST_KEY, host, NULL);
		gconf_client_set_int (client, SOCKS_PROXY_PORT_KEY,
			gconf_client_get_int (client, OLD_SOCKS_PROXY_PORT_KEY, NULL), NULL);
		g_free (host);
	}

	/* Set the proxy entries insensitive if we are using the same proxy for all */
	gtk_widget_set_sensitive (WID ("secure_host_entry"), !same_proxy);
	gtk_widget_set_sensitive (WID ("secure_port_spinbutton"), !same_proxy);
	gtk_widget_set_sensitive (WID ("ftp_host_entry"), !same_proxy);
	gtk_widget_set_sensitive (WID ("ftp_port_spinbutton"), !same_proxy);
	gtk_widget_set_sensitive (WID ("socks_host_entry"), !same_proxy);
	gtk_widget_set_sensitive (WID ("socks_port_spinbutton"), !same_proxy);

	g_object_unref (client);
}

static gchar *
get_hostname_from_uri (const gchar *uri)
{
	const gchar *start, *end;
	gchar *host;

	if (uri == NULL)
		return NULL;

	/* skip the scheme part */
	start = strchr (uri, ':');
	if (start == NULL)
		return NULL;

	/* forward until after the last '/' */
	do {
		++start;
	} while (*start == '/');

	if (*start == '\0')
	  return NULL;

	/* maybe we have a port? */
	end = strchr (start, ':');
	if (end == NULL)
		end = strchr (start, '/');

	if (end != NULL)
		host = g_strndup (start, end - start);
	else
		host = g_strdup (start);

	return host;
}

static GConfValue *
extract_proxy_host (GConfPropertyEditor *peditor, const GConfValue *orig)
{
	char const *entered_text = gconf_value_get_string (orig);
	GConfValue *res = NULL;

	if (entered_text != NULL) {
		gchar *host = get_hostname_from_uri (entered_text);

		if (host != NULL) {
			res = gconf_value_new (GCONF_VALUE_STRING);
			gconf_value_set_string (res, host);
			g_free (host);
		}
	}

	return (res != NULL) ? res : gconf_value_copy (orig);
}

static void
proxy_mode_radiobutton_clicked_cb (GtkWidget *widget,
				   GladeXML *dialog)
{
	GSList *mode_group;
	int mode;
	GConfClient *client;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
		return;

	mode_group = g_slist_copy (gtk_radio_button_get_group
		(GTK_RADIO_BUTTON (WID ("none_radiobutton"))));
	mode_group = g_slist_reverse (mode_group);
	mode = g_slist_index (mode_group, widget);
	g_slist_free (mode_group);

	gtk_widget_set_sensitive (WID ("manual_box"),
				  mode == PROXYMODE_MANUAL);
	gtk_widget_set_sensitive (WID ("same_proxy_checkbutton"),
				  mode == PROXYMODE_MANUAL);
	gtk_widget_set_sensitive (WID ("auto_box"),
				  mode == PROXYMODE_AUTO);
	client = gconf_client_get_default ();
	gconf_client_set_bool (client, USE_PROXY_KEY,
				  mode == PROXYMODE_AUTO || mode == PROXYMODE_MANUAL, NULL);
	g_object_unref (client);
}

static void
connect_sensitivity_signals (GladeXML *dialog, GSList *mode_group)
{
	for (; mode_group != NULL; mode_group = mode_group->next)
	{
		g_signal_connect (G_OBJECT (mode_group->data), "clicked",
				  G_CALLBACK(proxy_mode_radiobutton_clicked_cb),
				  dialog);
	}
}

static void
setup_dialog (GladeXML *dialog)
{
	GConfPropertyEditor *peditor;
	GSList *mode_group;
	GType mode_type = 0;
	GConfClient *client;
	gint port_value;
	GtkWidget *location_box;
	GtkCellRenderer *location_renderer;
	GtkListStore *store;

	mode_type = g_enum_register_static ("NetworkPreferencesProxyType",
				            proxytype_values);

	/* There's a bug in peditors that cause them to not initialize the entry
	 * correctly. */
	client = gconf_client_get_default ();

	/* Locations */
	location_box = WID ("location_combobox");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	gtk_combo_box_set_model (GTK_COMBO_BOX (location_box), GTK_TREE_MODEL (store));

	update_locations (client, dialog);
	gconf_client_add_dir (client, LOCATION_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (client, CURRENT_LOCATION, (GConfClientNotifyFunc) cb_current_location, dialog, NULL, NULL);

	g_signal_connect (location_box, "changed", G_CALLBACK (cb_location_changed), dialog);
	g_signal_connect (WID ("delete_button"), "clicked", G_CALLBACK (cb_delete_button_clicked), dialog);

	gtk_button_set_image (GTK_BUTTON (WID ("delete_button")),
			      gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_BUTTON));

	location_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_box), location_renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (location_box),
					location_renderer,
					"text", COL_NAME,
					"style", COL_STYLE, NULL);

	/* Hackety hack */
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("none_radiobutton"))->child), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("manual_radiobutton"))->child), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("auto_radiobutton"))->child), TRUE);

	/* Mode */
	mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (WID ("none_radiobutton")));
	connect_sensitivity_signals (dialog, mode_group);

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_select_radio_with_enum (NULL,
			PROXY_MODE_KEY, mode_group, mode_type,
			TRUE, NULL));

	/* Use same proxy for all protocols */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (NULL,
			USE_SAME_PROXY_KEY, WID ("same_proxy_checkbutton"), NULL));

	g_signal_connect (G_OBJECT (WID ("same_proxy_checkbutton")),
			"toggled",
			G_CALLBACK (cb_use_same_proxy_checkbutton_clicked),
			dialog);

	/* Http */
	port_value = gconf_client_get_int (client, HTTP_PROXY_PORT_KEY, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (WID ("http_port_spinbutton")), (gdouble) port_value);
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_PROXY_HOST_KEY, WID ("http_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, HTTP_PROXY_PORT_KEY, WID ("http_port_spinbutton"),
			NULL));
	g_signal_connect (G_OBJECT (WID ("details_button")),
			  "clicked",
			  G_CALLBACK (cb_http_details_button_clicked),
			  WID ("network_dialog"));

	/* Secure */
 	port_value = gconf_client_get_int (client, SECURE_PROXY_PORT_KEY, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (WID ("secure_port_spinbutton")), (gdouble) port_value);
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, SECURE_PROXY_HOST_KEY, WID ("secure_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, SECURE_PROXY_PORT_KEY, WID ("secure_port_spinbutton"),
			NULL));

	/* Ftp */
 	port_value = gconf_client_get_int (client, FTP_PROXY_PORT_KEY, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (WID ("ftp_port_spinbutton")), (gdouble) port_value);
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, FTP_PROXY_HOST_KEY, WID ("ftp_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, FTP_PROXY_PORT_KEY, WID ("ftp_port_spinbutton"),
			NULL));

	/* Socks */
 	port_value = gconf_client_get_int (client, SOCKS_PROXY_PORT_KEY, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (WID ("socks_port_spinbutton")), (gdouble) port_value);
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, SOCKS_PROXY_HOST_KEY, WID ("socks_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, SOCKS_PROXY_PORT_KEY, WID ("socks_port_spinbutton"),
			NULL));

	/* Set the proxy entries insensitive if we are using the same proxy for all */
	if (gconf_client_get_bool (client, USE_SAME_PROXY_KEY, NULL))
	{
		gtk_widget_set_sensitive (WID ("secure_host_entry"), FALSE);
		gtk_widget_set_sensitive (WID ("secure_port_spinbutton"), FALSE);
		gtk_widget_set_sensitive (WID ("ftp_host_entry"), FALSE);
		gtk_widget_set_sensitive (WID ("ftp_port_spinbutton"), FALSE);
		gtk_widget_set_sensitive (WID ("socks_host_entry"), FALSE);
		gtk_widget_set_sensitive (WID ("socks_port_spinbutton"), FALSE);
	}

	/* Autoconfiguration */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, PROXY_AUTOCONFIG_URL_KEY, WID ("autoconfig_entry"),
			NULL));

	g_signal_connect (WID ("network_dialog"), "response",
			  G_CALLBACK (cb_dialog_response), NULL);


	gtk_label_set_use_markup (GTK_LABEL (WID ("label_ignore_host")), TRUE);
	ignore_hosts = gconf_client_get_list(client, IGNORE_HOSTS_KEY, GCONF_VALUE_STRING, NULL);
	g_object_unref (client);

	model = create_listmodel();
	populate_listmodel(GTK_LIST_STORE(model), ignore_hosts);
	config_treeview(GTK_TREE_VIEW(WID("treeview_ignore_host")), model);

	g_signal_connect (WID ("button_add_url"), "clicked",
						G_CALLBACK (cb_add_url), dialog);
	g_signal_connect (WID ("entry_url"), "activate",
						G_CALLBACK (cb_add_url), dialog);
	g_signal_connect (WID ("button_remove_url"), "clicked",
						G_CALLBACK (cb_remove_url), dialog);
}

int
main (int argc, char **argv)
{
	GladeXML    *dialog;
	GConfClient *client;
	GtkWidget   *widget;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/system/http_proxy",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/system/proxy",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-network-preferences.glade",
				"network_dialog", NULL);

	setup_dialog (dialog);
	widget = WID ("network_dialog");
	capplet_set_icon (widget, "gnome-network-preferences");
	gtk_widget_show_all (widget);
	gtk_main ();

	g_object_unref (dialog);
	g_object_unref (client);

	return 0;
}
