/* gnome-network-properties.c: network preferences capplet
 *
 * Copyright (C) 2002 Sun Microsystems Inc.
 *
 * Written by: Mark McLoughlin <mark@skynet.ie>
 *             Rodrigo Moya <rodrigo@gnome.org>
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

#include "cc-network-panel.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gconf/gconf-client.h> // FIXME: still needed for locations

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

#define OLD_SECURE_PROXY_HOST_KEY  "/system/proxy/old_secure_host"
#define OLD_SECURE_PROXY_PORT_KEY  "/system/proxy/old_secure_port"
#define OLD_FTP_PROXY_HOST_KEY  "/system/proxy/old_ftp_host"
#define OLD_FTP_PROXY_PORT_KEY  "/system/proxy/old_ftp_port"
#define OLD_SOCKS_PROXY_HOST_KEY  "/system/proxy/old_socks_host"
#define OLD_SOCKS_PROXY_PORT_KEY  "/system/proxy/old_socks_port"

#define LOCATION_DIR "/apps/control-center/network"
#define CURRENT_LOCATION "/apps/control-center/network/current_location"

#define GNOMECC_GNP_UI_FILE (GNOMECC_UI_DIR "/gnome-network-properties.ui")

static GtkWidget *details_dialog = NULL;
static GPtrArray *ignore_hosts = NULL;
static GtkTreeModel *model = NULL;
static GSettings *proxy_settings = NULL;
static GSettings *http_proxy_settings = NULL;
static GSettings *https_proxy_settings = NULL;
static GSettings *ftp_proxy_settings = NULL;
static GSettings *socks_proxy_settings = NULL;

static GtkTreeModel *
create_listmodel(void)
{
	GtkListStore *store;

	store = gtk_list_store_new(1, G_TYPE_STRING);

	return GTK_TREE_MODEL(store);
}

static GtkTreeModel *
populate_listmodel (GtkListStore *store, GPtrArray *list)
{
	GtkTreeIter iter;
	gint i;

	gtk_list_store_clear (store);

	for (i = 0; i < list->len; i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, (char *) g_ptr_array_index (list, i), -1);
	}

	return GTK_TREE_MODEL (store);
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

static GtkWidget*
_gtk_builder_get_widget (GtkBuilder *builder, const gchar *name)
{
	return GTK_WIDGET (gtk_builder_get_object (builder, name));
}


static void
cb_add_url (GtkButton *button, gpointer data)
{
	GtkBuilder *builder = GTK_BUILDER (data);
	gchar *new_url = NULL;

	new_url = g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_builder_get_object (builder, "entry_url"))));
	if (strlen (new_url) == 0)
		return;

	g_ptr_array_add (ignore_hosts, new_url);
	populate_listmodel (GTK_LIST_STORE (model), ignore_hosts);
	gtk_entry_set_text (GTK_ENTRY (gtk_builder_get_object (builder,
							       "entry_url")), "");

	g_settings_set_strv (proxy_settings, "ignore-hosts", (const gchar **) ignore_hosts->pdata);
}

static void
cb_remove_url (GtkButton *button, gpointer data)
{
	GtkBuilder *builder = GTK_BUILDER (data);
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "treeview_ignore_host")));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gchar *url;
		guint i;

		gtk_tree_model_get (model, &iter, 0, &url, -1);

		for (i = 0; i < ignore_hosts->len; i++) {
			if (g_str_equal (url, (char *) g_ptr_array_index (ignore_hosts, i))) {
				g_ptr_array_remove_index (ignore_hosts, i);
				break;
			}
		}

		g_free (url);
		populate_listmodel (GTK_LIST_STORE(model), ignore_hosts);

		g_settings_set_strv (proxy_settings, "ignore-hosts", (const gchar **) ignore_hosts->pdata);
	}
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	/* if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"goscustdesk-50");
	else */ if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT)
	{
		if (ignore_hosts != NULL)
			g_ptr_array_free (ignore_hosts, TRUE);

		gtk_main_quit ();
	}
}

static void
cb_details_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP) {
		/* capplet_help (GTK_WINDOW (dialog),
			      "goscustdesk-50"); */
        } else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		details_dialog = NULL;
	}
}

static void
cb_use_auth_toggled (GtkToggleButton *toggle,
		     GtkWidget *table)
{
	gtk_widget_set_sensitive (table, gtk_toggle_button_get_active (toggle));
}

static void
cb_http_details_button_clicked (GtkWidget *button,
			        GtkWidget *parent)
{
	GtkBuilder *builder;
	gchar *builder_widgets[] = { "details_dialog", NULL };
	GError *error = NULL;
	GtkWidget *widget;

	if (details_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (details_dialog));
		gtk_widget_grab_focus (details_dialog);
		return;
	}

	builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_file (builder, GNOMECC_GNP_UI_FILE,
					       builder_widgets, &error) == 0) {
		g_warning ("Could not load details dialog: %s", error->message);
		g_error_free (error);
		g_object_unref (builder);
		return;
	}

	details_dialog = widget = _gtk_builder_get_widget (builder,
							   "details_dialog");

	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (gtk_widget_get_toplevel (parent)));

	g_signal_connect (gtk_builder_get_object (builder, "use_auth_checkbutton"),
			  "toggled",
			  G_CALLBACK (cb_use_auth_toggled),
			  _gtk_builder_get_widget (builder, "auth_table"));

	g_settings_bind (http_proxy_settings, "use-authentication",
			 _gtk_builder_get_widget (builder, "use_auth_checkbutton"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (http_proxy_settings, "authentication-user",
			 _gtk_builder_get_widget (builder, "username_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (http_proxy_settings, "authentication-password",
			 _gtk_builder_get_widget (builder, "password_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (widget, "response",
			  G_CALLBACK (cb_details_dialog_response), NULL);

	/* capplet_set_icon (widget, "gnome-network-properties"); */

	gtk_widget_show_all (widget);
}

/* static gchar * */
/* copy_location_create_key (const gchar *from, const gchar *what) */
/* { */
/* 	if (from[0] == '\0') return g_strdup (what); */
/* 	else return g_strconcat (from, what + strlen ("/system"), NULL); */
/* } */

/* static void */
/* copy_location (const gchar *from, const gchar *to, GConfClient *client) */
/* { */
/* 	int ti; */
/* 	gboolean tb; */
/* 	GSList *tl; */
/* 	gchar *tstr, *dest, *src; */

/* 	if (from[0] != '\0' && !gconf_client_dir_exists (client, from, NULL)) */
/* 		return; */

/* 	/\* USE_PROXY *\/ */
/* 	dest = copy_location_create_key (to, USE_PROXY_KEY); */
/* 	src = copy_location_create_key (from, USE_PROXY_KEY); */

/* 	tb = gconf_client_get_bool (client, src, NULL); */
/* 	gconf_client_set_bool (client, dest, tb, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* USE_SAME_PROXY *\/ */
/* 	dest = copy_location_create_key (to, USE_SAME_PROXY_KEY); */
/* 	src = copy_location_create_key (from, USE_SAME_PROXY_KEY); */

/* 	tb = gconf_client_get_bool (client, src, NULL); */
/* 	gconf_client_set_bool (client, dest, tb, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* HTTP_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, HTTP_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, HTTP_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* HTTP_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, HTTP_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, HTTP_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* HTTP_USE_AUTH *\/ */
/* 	dest = copy_location_create_key (to, HTTP_USE_AUTH_KEY); */
/* 	src = copy_location_create_key (from, HTTP_USE_AUTH_KEY); */

/* 	tb = gconf_client_get_bool (client, src, NULL); */
/* 	gconf_client_set_bool (client, dest, tb, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* HTTP_AUTH_USER *\/ */
/* 	dest = copy_location_create_key (to, HTTP_AUTH_USER_KEY); */
/* 	src = copy_location_create_key (from, HTTP_AUTH_USER_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* HTTP_AUTH_PASSWD *\/ */
/* 	dest = copy_location_create_key (to, HTTP_AUTH_PASSWD_KEY); */
/* 	src = copy_location_create_key (from, HTTP_AUTH_PASSWD_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		 gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* IGNORE_HOSTS *\/ */
/* 	dest = copy_location_create_key (to, IGNORE_HOSTS_KEY); */
/* 	src = copy_location_create_key (from, IGNORE_HOSTS_KEY); */

/* 	tl = gconf_client_get_list (client, src, GCONF_VALUE_STRING, NULL); */
/* 	gconf_client_set_list (client, dest, GCONF_VALUE_STRING, tl, NULL); */
/* 	g_slist_foreach (tl, (GFunc) g_free, NULL); */
/* 	g_slist_free (tl); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* PROXY_MODE *\/ */
/* 	dest = copy_location_create_key (to, PROXY_MODE_KEY); */
/* 	src = copy_location_create_key (from, PROXY_MODE_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* SECURE_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, SECURE_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, SECURE_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* OLD_SECURE_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, OLD_SECURE_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, OLD_SECURE_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* SECURE_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, SECURE_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, SECURE_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* OLD_SECURE_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, OLD_SECURE_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, OLD_SECURE_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* FTP_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, FTP_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, FTP_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* OLD_FTP_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, OLD_FTP_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, OLD_FTP_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* FTP_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, FTP_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, FTP_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* OLD_FTP_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, OLD_FTP_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, OLD_FTP_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* SOCKS_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, SOCKS_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, SOCKS_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free	(dest); */
/* 	g_free (src); */

/* 	/\* OLD_SOCKS_PROXY_HOST *\/ */
/* 	dest = copy_location_create_key (to, OLD_SOCKS_PROXY_HOST_KEY); */
/* 	src = copy_location_create_key (from, OLD_SOCKS_PROXY_HOST_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* SOCKS_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, SOCKS_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, SOCKS_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* OLD_SOCKS_PROXY_PORT *\/ */
/* 	dest = copy_location_create_key (to, OLD_SOCKS_PROXY_PORT_KEY); */
/* 	src = copy_location_create_key (from, OLD_SOCKS_PROXY_PORT_KEY); */

/* 	ti = gconf_client_get_int (client, src, NULL); */
/* 	gconf_client_set_int (client, dest, ti, NULL); */

/* 	g_free (dest); */
/* 	g_free (src); */

/* 	/\* PROXY_AUTOCONFIG_URL *\/ */
/* 	dest = copy_location_create_key (to, PROXY_AUTOCONFIG_URL_KEY); */
/* 	src = copy_location_create_key (from, PROXY_AUTOCONFIG_URL_KEY); */

/* 	tstr = gconf_client_get_string (client, src, NULL); */
/* 	if (tstr != NULL) */
/* 	{ */
/* 		gconf_client_set_string (client, dest, tstr, NULL); */
/* 		g_free (tstr); */
/* 	} */

/* 	g_free (dest); */
/* 	g_free (src); */
/* } */

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
		  GtkBuilder *builder);

static void
cb_location_changed (GtkWidget *location,
		     GtkBuilder *builder);

static void
cb_current_location (GConfClient *client,
		     guint cnxn_id,
		     GConfEntry *entry,
		     GtkBuilder *builder)
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
	g_signal_handlers_block_by_func (gtk_builder_get_object (builder, "location_combobox"),
	                                 cb_location_changed, builder);
	update_locations (client, builder);
	g_signal_handlers_unblock_by_func (gtk_builder_get_object (builder, "location_combobox"),
	                                   cb_location_changed, builder);
}

static void
update_locations (GConfClient *client,
		  GtkBuilder *builder)
{
	int i, select;
	gchar *current;
	GtkComboBox *location = GTK_COMBO_BOX (gtk_builder_get_object (builder, "location_combobox"));
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

			locp = (char *) (iter->data) + strlen (LOCATION_DIR) + 1;
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
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "delete_button"),
				  i > 1);

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
cb_location_new_text_changed (GtkEntry *entry, GtkBuilder *builder)
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
		gtk_widget_show (_gtk_builder_get_widget (builder,
							  "error_label"));
	else
		gtk_widget_hide (_gtk_builder_get_widget (builder,
							  "error_label"));

	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "new_location"),
				  !exists);
}

static void
location_new (GtkBuilder *capplet_builder, GtkWidget *parent)
{
	GtkBuilder *builder;
	GError *error = NULL;
	gchar *builder_widgets[] = { "location_new_dialog",
				     "new_location_btn_img", NULL };
	GtkWidget *askdialog;
	const gchar *name;
	int response;
	GConfClient *client;

	client = gconf_client_get_default ();

	builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_file (builder, GNOMECC_GNP_UI_FILE,
					       builder_widgets, &error) == 0) {
		g_warning ("Could not load location dialog: %s",
			   error->message);
		g_error_free (error);
		g_object_unref (builder);
		return;
	}

	askdialog =  _gtk_builder_get_widget (builder, "location_new_dialog");
	gtk_window_set_transient_for (GTK_WINDOW (askdialog), GTK_WINDOW (parent));
	g_signal_connect (askdialog, "response",
			  G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (gtk_builder_get_object (builder, "text"), "changed",
			  G_CALLBACK (cb_location_new_text_changed), builder);
	response = gtk_dialog_run (GTK_DIALOG (askdialog));
	name = gtk_entry_get_text (GTK_ENTRY (gtk_builder_get_object (builder, "text")));
	g_object_unref (builder);

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

			/* FIXME: need to enable this when GSettingsList is ready copy_location ("", key, client); */
			g_free (key);

			gconf_client_set_string (client, CURRENT_LOCATION, name, NULL);
			update_locations (client, capplet_builder);
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

static char *
get_active_location (GtkComboBox *box)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	char *location;

	model = gtk_combo_box_get_model (box);
	if (gtk_combo_box_get_active_iter (box, &iter) == FALSE)
		return NULL;
	gtk_tree_model_get (model, &iter, COL_NAME, &location, -1);

	return location;
}

static void
cb_location_changed (GtkWidget *location,
		     GtkBuilder *builder)
{
	/* gchar *current; */
	/* gchar *name = get_active_location (GTK_COMBO_BOX (location)); */

	/* if (name == NULL) */
	/* 	return; */

	/* current = get_current_location (client); */

	/* if (strcmp (current, name) != 0) */
	/* { */
	/* 	if (strcmp (name, _("New Location...")) == 0) */
	/* 	{ */
	/* 		location_new (builder, */
	/* 			     gtk_widget_get_toplevel (_gtk_builder_get_widget (builder, "network-panel"))); */
	/* 	} */
	/* 	else */
	/* 	{ */
	/* 		gchar *key, *esc; */

	/* 		/\* save current settings *\/ */
	/* 		esc = gconf_escape_key (current, -1); */
	/* 		key = g_strconcat (LOCATION_DIR "/", esc, NULL); */
	/* 		g_free (esc); */

	/* 		copy_location ("", key, client); */
	/* 		g_free (key); */

	/* 		/\* load settings *\/ */
	/* 		esc = gconf_escape_key (name, -1); */
	/* 		key = g_strconcat (LOCATION_DIR "/", esc, NULL); */
	/* 		g_free (esc); */

	/* 		copy_location (key, "", client); */
	/* 		gconf_client_recursive_unset (client, key, */
	/* 		                              GCONF_UNSET_INCLUDING_SCHEMA_NAMES, NULL); */
	/* 		g_free (key); */

	/* 		gconf_client_set_string (client, CURRENT_LOCATION, name, NULL); */
	/* 	} */
	/* } */

	/* g_free (current); */
	/* g_free (name); */
	/* g_object_unref (client); */
}

static void
cb_delete_button_clicked (GtkWidget *button,
			  GtkBuilder *builder)
{
	GConfClient *client;
	GtkComboBox *box = GTK_COMBO_BOX (gtk_builder_get_object (builder,
								  "location_combobox"));
	int active = gtk_combo_box_get_active (box);
	gchar *current, *key, *esc;
	GtkTreeIter iter;

	/* prevent the current settings from being saved by blocking
	 * the signal handler */
	g_signal_handlers_block_by_func (box, cb_location_changed, builder);
	if (gtk_combo_box_get_active_iter (box, &iter) != FALSE) {
		gtk_list_store_remove (GTK_LIST_STORE (gtk_combo_box_get_model (box)),
				       &iter);
	}
	gtk_combo_box_set_active (box, (active == 0) ? 1 : 0);
	g_signal_handlers_unblock_by_func (box, cb_location_changed, builder);

	/* set the new location */
	client = gconf_client_get_default ();
	current = get_active_location (GTK_COMBO_BOX (box));

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

/* When using the same proxy for all protocols, updates every host_entry
 * as the user types along */
static void
synchronize_hosts (GtkEntry *widget,
		   GtkBuilder *builder)
{
	const gchar *hosts[] = {
		"secure_host_entry",
		"ftp_host_entry",
		"socks_host_entry",
		NULL };
	const gchar **host, *http_host;

	http_host = gtk_entry_get_text (widget);

	for (host = hosts; *host != NULL; ++host)
	{
		widget = GTK_ENTRY (gtk_builder_get_object (builder, *host));
		gtk_entry_set_text (widget, http_host);
	}
}

/* When using the same proxy for all protocols, copies the value of the
 * http port to the other spinbuttons */
static void
synchronize_ports (GtkSpinButton *widget,
		   GtkBuilder *builder)
{
	const gchar *ports[] = {
		"secure_port_spinbutton",
		"ftp_port_spinbutton",
		"socks_port_spinbutton",
		NULL };
	gdouble http_port;
	const gchar **port;

	http_port = gtk_spin_button_get_value (widget);

	for (port = ports; *port != NULL; ++port)
	{
		widget = GTK_SPIN_BUTTON (
			gtk_builder_get_object (builder, *port));
		gtk_spin_button_set_value (widget, http_port);
	}
}

/* Synchronizes all hosts and ports */
static void
synchronize_entries (GtkBuilder *builder)
{
	g_signal_connect (
		gtk_builder_get_object (builder, "http_host_entry"),
		"changed",
		G_CALLBACK (synchronize_hosts),
		builder);
	g_signal_connect (
		gtk_builder_get_object (builder, "http_port_spinbutton"),
		"value-changed",
		G_CALLBACK (synchronize_ports),
		builder);
}

/* Unsynchronize hosts and ports */
static void
unsynchronize_entries (GtkBuilder *builder)
{
	g_signal_handlers_disconnect_by_func (
		gtk_builder_get_object (builder, "http_host_entry"),
		synchronize_hosts,
		builder);
	g_signal_handlers_disconnect_by_func (
		gtk_builder_get_object (builder, "http_port_spinbutton"),
		synchronize_ports,
		builder);
}

static void
cb_use_same_proxy_checkbutton_clicked (GtkWidget *checkbutton,
				       GtkBuilder *builder)
{
	gboolean same_proxy;
	gchar *http_proxy;
	gint http_port;

	same_proxy = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));

	http_proxy = g_settings_get_string (http_proxy_settings, "host");
	http_port = g_settings_get_int (http_proxy_settings, "port");

	if (same_proxy) {
		/* Set the new values */
		gtk_entry_set_text (GTK_ENTRY (gtk_builder_get_object (builder, "secure_host_entry")), http_proxy);
		gtk_range_set_value (GTK_RANGE (gtk_builder_get_object (builder, "secure_host_spinbutton")), (gdouble) http_port);

		gtk_entry_set_text (GTK_ENTRY (gtk_builder_get_object (builder, "ftp_host_entry")), http_proxy);
		gtk_range_set_value (GTK_RANGE (gtk_builder_get_object (builder, "ftp_host_spinbutton")), (gdouble) http_port);

		gtk_entry_set_text (GTK_ENTRY (gtk_builder_get_object (builder, "socks_host_entry")), http_proxy);
		gtk_range_set_value (GTK_RANGE (gtk_builder_get_object (builder, "socks_host_spinbutton")), (gdouble) http_port);

		/* Synchronize entries */
		synchronize_entries (builder);
	} else {
		/* Hosts and ports should not be synchronized any more */
		unsynchronize_entries (builder);
	}

	/* Set the proxy entries insensitive if we are using the same proxy for all */
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "secure_host_entry"),
				  !same_proxy);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "secure_port_spinbutton"),
				  !same_proxy);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "ftp_host_entry"),
				  !same_proxy);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "ftp_port_spinbutton"),
				  !same_proxy);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "socks_host_entry"),
				  !same_proxy);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder,
							   "socks_port_spinbutton"),
				  !same_proxy);
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

static void
proxy_mode_radiobutton_clicked_cb (GtkWidget *widget,
				   GtkBuilder *builder)
{
	GSList *mode_group;
	int mode;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
		return;

	mode_group = g_slist_copy (gtk_radio_button_get_group
		(GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "none_radiobutton"))));
	mode_group = g_slist_reverse (mode_group);
	mode = g_slist_index (mode_group, widget);
	g_slist_free (mode_group);

	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "manual_box"),
				  mode == PROXYMODE_MANUAL);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "same_proxy_checkbutton"),
				  mode == PROXYMODE_MANUAL);
	gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "auto_box"),
				  mode == PROXYMODE_AUTO);

	g_settings_set_string (proxy_settings, "mode", proxytype_values[mode].value_nick);
	g_settings_set_boolean (http_proxy_settings, "enabled",
				mode == PROXYMODE_AUTO || mode == PROXYMODE_MANUAL);
}

static void
connect_sensitivity_signals (GtkBuilder *builder, GSList *mode_group)
{
	for (; mode_group != NULL; mode_group = mode_group->next) {
		g_signal_connect (G_OBJECT (mode_group->data), "clicked",
				  G_CALLBACK(proxy_mode_radiobutton_clicked_cb),
				  builder);
	}
}

static void
proxy_settings_changed_cb (GSettings *settings,
			   const gchar *key,
			   gpointer user_data)
{
	if (g_str_equal (key, "ignore-hosts")) {
		gchar **hosts;
		guint i = 0;

		g_ptr_array_free (ignore_hosts, TRUE);

		hosts = g_settings_get_strv (proxy_settings, "ignore-hosts");
		while (hosts[i] != NULL) {
			g_ptr_array_add (ignore_hosts, hosts[i]);
			i++;
		}

		populate_listmodel (GTK_LIST_STORE (model), ignore_hosts);
		g_strfreev (hosts);
	}
}

static void
setup_dialog (GtkBuilder *builder)
{
	GSList *mode_group;
	GType mode_type = 0;
	GConfClient *client;
	gint port_value;
	GtkWidget *location_box, *same_proxy_toggle;
	GtkCellRenderer *location_renderer;
	GtkListStore *store;

        mode_type = g_type_from_name ("NetworkPreferencesProxyType");
	if (!mode_type)
		mode_type = g_enum_register_static ("NetworkPreferencesProxyType",
						    proxytype_values);

	client = gconf_client_get_default ();

	/* Locations */
	location_box = _gtk_builder_get_widget (builder, "location_combobox");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	gtk_combo_box_set_model (GTK_COMBO_BOX (location_box), GTK_TREE_MODEL (store));

	update_locations (client, builder);
	gconf_client_add_dir (client, LOCATION_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (client, CURRENT_LOCATION, (GConfClientNotifyFunc) cb_current_location, builder, NULL, NULL);

	g_signal_connect (location_box, "changed", G_CALLBACK (cb_location_changed), builder);
	g_signal_connect (gtk_builder_get_object (builder, "delete_button"), "clicked", G_CALLBACK (cb_delete_button_clicked), builder);

	location_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_box), location_renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (location_box),
					location_renderer,
					"text", COL_NAME,
					"style", COL_STYLE, NULL);

	/* Mode */
	mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (gtk_builder_get_object (builder, "none_radiobutton")));
	connect_sensitivity_signals (builder, mode_group);

	/* Use same proxy for all protocols */
	same_proxy_toggle = _gtk_builder_get_widget (builder, "same_proxy_checkbutton");
	g_settings_bind (proxy_settings, "use-same-proxy",
			 same_proxy_toggle, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (same_proxy_toggle,
			"toggled",
			G_CALLBACK (cb_use_same_proxy_checkbutton_clicked),
			builder);

	/* Http */
	port_value = g_settings_get_int (http_proxy_settings, "port");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "http_port_spinbutton")), (gdouble) port_value);

	g_settings_bind (http_proxy_settings, "port",
			 gtk_range_get_adjustment (GTK_RANGE (gtk_builder_get_object (builder, "http_port_spinbutton"))), "value",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (http_proxy_settings, "host",
			 _gtk_builder_get_widget (builder, "http_host_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (gtk_builder_get_object (builder, "details_button"),
			  "clicked",
			  G_CALLBACK (cb_http_details_button_clicked),
			  _gtk_builder_get_widget (builder, "network-panel"));

	/* Secure */
 	port_value = g_settings_get_int (https_proxy_settings, "port");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "secure_port_spinbutton")), (gdouble) port_value);

	g_settings_bind (https_proxy_settings, "host",
			 _gtk_builder_get_widget (builder, "secure_host_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (https_proxy_settings, "port",
			 gtk_range_get_adjustment (GTK_RANGE (_gtk_builder_get_widget (builder, "secure_port_spinbutton"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* Ftp */
 	port_value = g_settings_get_int (ftp_proxy_settings, "port");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "ftp_port_spinbutton")), (gdouble) port_value);
	g_settings_bind (ftp_proxy_settings, "host",
			 _gtk_builder_get_widget (builder, "ftp_host_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (ftp_proxy_settings, "port",
			 gtk_range_get_adjustment (GTK_RANGE (_gtk_builder_get_widget (builder, "ftp_port_spinbutton"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* Socks */
 	port_value = g_settings_get_int (socks_proxy_settings, "port");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "socks_port_spinbutton")), (gdouble) port_value);
	g_settings_bind (socks_proxy_settings, "host",
			 _gtk_builder_get_widget (builder, "socks_host_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (socks_proxy_settings, "port",
			 gtk_range_get_adjustment (GTK_RANGE (_gtk_builder_get_widget (builder, "socks_port_spinbutton"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* Set the proxy entries insensitive if we are using the same proxy for all,
	   and make sure they are all synchronized */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (same_proxy_toggle))) {
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "secure_host_entry"), FALSE);
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "secure_port_spinbutton"), FALSE);
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "ftp_host_entry"), FALSE);
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "ftp_port_spinbutton"), FALSE);
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "socks_host_entry"), FALSE);
		gtk_widget_set_sensitive (_gtk_builder_get_widget (builder, "socks_port_spinbutton"), FALSE);

		synchronize_entries (builder);
	}

	/* Autoconfiguration */
	g_settings_bind (proxy_settings, "autoconfig-url",
			 _gtk_builder_get_widget (builder, "autoconfig_entry"), "text",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (gtk_builder_get_object (builder, "network_dialog"),
			  "response", G_CALLBACK (cb_dialog_response), NULL);


	proxy_settings_changed_cb (proxy_settings, "ignore-hosts", builder);

	model = create_listmodel();
	populate_listmodel(GTK_LIST_STORE(model), ignore_hosts);
	config_treeview(GTK_TREE_VIEW(gtk_builder_get_object (builder, "treeview_ignore_host")), model);

	g_signal_connect (gtk_builder_get_object (builder, "button_add_url"),
			  "clicked", G_CALLBACK (cb_add_url), builder);
	g_signal_connect (gtk_builder_get_object (builder, "entry_url"),
			  "activate", G_CALLBACK (cb_add_url), builder);
	g_signal_connect (gtk_builder_get_object (builder, "button_remove_url"),
			  "clicked", G_CALLBACK (cb_remove_url), builder);
}

int
gnome_network_properties_init (GtkBuilder  *builder)
{
	GError *error = NULL;
	gchar *builder_widgets[] = {"network_dialog", "adjustment1",
				    "adjustment2", "adjustment3", "adjustment4",
				    "delete_button_img", NULL};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	proxy_settings = g_settings_new ("org.gnome.system.proxy");
	g_signal_connect (proxy_settings, "changed",
			  G_CALLBACK (proxy_settings_changed_cb), builder);

	http_proxy_settings = g_settings_new ("org.gnome.system.proxy.http");
	https_proxy_settings = g_settings_new ("org.gnome.system.proxy.https");
	ftp_proxy_settings = g_settings_new ("org.gnome.system.proxy.ftp");
	socks_proxy_settings = g_settings_new ("org.gnome.system.proxy.socks");

	if (gtk_builder_add_objects_from_file (builder, GNOMECC_GNP_UI_FILE,
					       builder_widgets, &error) == 0) {
		g_warning ("Could not load main dialog: %s",
			   error->message);
		g_error_free (error);
		g_object_unref (builder);
		return 1;
	}

	setup_dialog (builder);

	return 0;
}
