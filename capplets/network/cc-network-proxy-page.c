/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Sun Microsystems Inc.
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>

#include "cc-network-proxy-page.h"

#include "gconf-property-editor.h"

#define CC_NETWORK_PROXY_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_NETWORK_PROXY_PAGE, CcNetworkProxyPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

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
#define IGNORE_HOSTS_KEY         "/system/http_proxy/ignore_hosts"
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

#define LOCATION_DIR     "/apps/control-center/network"
#define CURRENT_LOCATION "/apps/control-center/network/current_location"

#define GNOMECC_GNP_UI_FILE (GNOMECC_UI_DIR "/gnome-network-properties.ui")

struct CcNetworkProxyPagePrivate
{
        GtkWidget    *details_dialog;
        GtkWidget    *none_radiobutton;
        GtkWidget    *location_combobox;
        GtkWidget    *delete_button;
        GtkWidget    *same_proxy_checkbutton;
        GtkWidget    *http_port_spinbutton;
        GtkWidget    *http_host_entry;
        GtkWidget    *details_button;
        GtkWidget    *secure_port_spinbutton;
        GtkWidget    *secure_host_entry;
        GtkWidget    *ftp_port_spinbutton;
        GtkWidget    *ftp_host_entry;
        GtkWidget    *socks_port_spinbutton;
        GtkWidget    *socks_host_entry;
        GtkWidget    *autoconfig_entry;
        GtkWidget    *ignored_hosts_treeview;
        GtkWidget    *ignored_host_add_button;
        GtkWidget    *ignored_host_remove_button;
        GtkWidget    *ignored_host_entry;
        GtkWidget    *error_label;
        GtkWidget    *new_location;
        GtkWidget    *auto_box;
        GtkWidget    *manual_box;

        GSList       *ignore_hosts;
        GtkTreeModel *model;
};

enum {
        PROP_0,
};

static void     cc_network_proxy_page_class_init     (CcNetworkProxyPageClass *klass);
static void     cc_network_proxy_page_init           (CcNetworkProxyPage      *network_proxy_page);
static void     cc_network_proxy_page_finalize       (GObject             *object);

G_DEFINE_TYPE (CcNetworkProxyPage, cc_network_proxy_page, CC_TYPE_PAGE)

static void
cc_network_proxy_page_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_network_proxy_page_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
populate_ignored_hosts_model (CcNetworkProxyPage *page,
                              GSList             *list)
{
        GtkTreeIter   iter;
        GSList       *pointer;
        GtkTreeModel *model;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (page->priv->ignored_hosts_treeview));

        gtk_list_store_clear (GTK_LIST_STORE (model));

        pointer = list;
        while (pointer) {
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    0, (char *) pointer->data,
                                    -1);
                pointer = g_slist_next (pointer);
        }
}

static void
add_url (CcNetworkProxyPage *page)
{
        char        *new_url = NULL;
        GConfClient *client;

        new_url = g_strdup (gtk_entry_get_text (GTK_ENTRY (page->priv->ignored_host_entry)));
        if (strlen (new_url) == 0)
                return;

        page->priv->ignore_hosts = g_slist_append (page->priv->ignore_hosts, new_url);
        populate_ignored_hosts_model (page, page->priv->ignore_hosts);
        gtk_entry_set_text (GTK_ENTRY (page->priv->ignored_host_entry), "");

        client = gconf_client_get_default ();
        gconf_client_set_list (client,
                               IGNORE_HOSTS_KEY,
                               GCONF_VALUE_STRING,
                               page->priv->ignore_hosts,
                               NULL);
        g_object_unref (client);
}

static void
on_add_button_clicked (GtkButton          *button,
                       CcNetworkProxyPage *page)
{
        add_url (page);
}

static void
on_ignored_host_entry_activate (GtkEntry           *entry,
                                CcNetworkProxyPage *page)
{
        add_url (page);
}

static void
on_remove_button_clicked (GtkButton          *button,
                          CcNetworkProxyPage *page)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        GConfClient      *client;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page->priv->ignored_hosts_treeview));
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (page->priv->ignored_hosts_treeview));
        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                char   *url;
                GSList *pointer;

                gtk_tree_model_get (model, &iter, 0, &url, -1);

                pointer = page->priv->ignore_hosts;
                while (pointer) {
                        if (strcmp(url, (char *) pointer->data) == 0) {
                                g_free (pointer->data);
                                page->priv->ignore_hosts = g_slist_delete_link (page->priv->ignore_hosts, pointer);
                                break;
                        }
                        pointer = g_slist_next (pointer);
                }

                g_free (url);
                populate_ignored_hosts_model (page, page->priv->ignore_hosts);

                client = gconf_client_get_default ();
                gconf_client_set_list (client, IGNORE_HOSTS_KEY, GCONF_VALUE_STRING, page->priv->ignore_hosts, NULL);
                g_object_unref (client);
        }
}

static void
on_details_dialog_response (GtkDialog          *dialog,
                            int                 response_id,
                            CcNetworkProxyPage *page)
{
        gtk_widget_destroy (GTK_WIDGET (dialog));
        page->priv->details_dialog = NULL;
}

static void
on_use_auth_toggled (GtkToggleButton *toggle,
                     GtkWidget       *table)
{
        gtk_widget_set_sensitive (table, toggle->active);
}

static void
on_http_details_button_clicked (GtkWidget          *button,
                                CcNetworkProxyPage *page)
{
        GtkBuilder          *builder;
        char                *builder_widgets[] = { "details_dialog", NULL };
        GError              *error = NULL;
        GtkWidget           *widget;
        GConfPropertyEditor *peditor;
        GtkWidget           *toplevel;

        if (page->priv->details_dialog != NULL) {
                gtk_window_present (GTK_WINDOW (page->priv->details_dialog));
                gtk_widget_grab_focus (page->priv->details_dialog);
                return;
        }

        builder = gtk_builder_new ();
        if (gtk_builder_add_objects_from_file (builder,
                                               GNOMECC_GNP_UI_FILE,
                                               builder_widgets,
                                               &error) == 0) {
                g_warning ("Could not load details dialog: %s", error->message);
                g_error_free (error);
                g_object_unref (builder);
                return;
        }

        page->priv->details_dialog = widget = WID ("details_dialog");

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        gtk_window_set_transient_for (GTK_WINDOW (widget),
                                      GTK_WINDOW (toplevel));

        gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("use_auth_checkbutton"))->child), TRUE);

        g_signal_connect (WID ("use_auth_checkbutton"),
                          "toggled",
                          G_CALLBACK (on_use_auth_toggled),
                          WID ("auth_table"));

        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (NULL,
                                                                    HTTP_USE_AUTH_KEY,
                                                                    WID ("use_auth_checkbutton"),
                                                                    NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   HTTP_AUTH_USER_KEY,
                                                                   WID ("username_entry"),
                                                                   NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   HTTP_AUTH_PASSWD_KEY,
                                                                   WID ("password_entry"),
                                                                   NULL));

        g_signal_connect (widget,
                          "response",
                          G_CALLBACK (on_details_dialog_response),
                          page);

        gtk_window_set_icon_name (GTK_WINDOW (widget), "gnome-network-properties");

        gtk_widget_show_all (widget);
}

static char *
copy_location_create_key (const char *from,
                          const char *what)
{
        if (from[0] == '\0')
                return g_strdup (what);
        else
                return g_strconcat (from, what + strlen ("/system"), NULL);
}

static void
copy_location (const char  *from,
               const char  *to,
               GConfClient *client)
{
        int      ti;
        gboolean tb;
        GSList  *tl;
        char    *tstr, *dest, *src;

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
        if (tstr != NULL) {
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
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free (dest);
        g_free (src);

        /* HTTP_AUTH_PASSWD */
        dest = copy_location_create_key (to, HTTP_AUTH_PASSWD_KEY);
        src = copy_location_create_key (from, HTTP_AUTH_PASSWD_KEY);

        tstr = gconf_client_get_string (client, src, NULL);
        if (tstr != NULL) {
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
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free (dest);
        g_free (src);

        /* SECURE_PROXY_HOST */
        dest = copy_location_create_key (to, SECURE_PROXY_HOST_KEY);
        src = copy_location_create_key (from, SECURE_PROXY_HOST_KEY);

        tstr = gconf_client_get_string (client, src, NULL);
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free (dest);
        g_free (src);

        /* OLD_SECURE_PROXY_HOST */
        dest = copy_location_create_key (to, OLD_SECURE_PROXY_HOST_KEY);
        src = copy_location_create_key (from, OLD_SECURE_PROXY_HOST_KEY);

        tstr = gconf_client_get_string (client, src, NULL);
        if (tstr != NULL) {
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
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free (dest);
        g_free (src);

        /* OLD_FTP_PROXY_HOST */
        dest = copy_location_create_key (to, OLD_FTP_PROXY_HOST_KEY);
        src = copy_location_create_key (from, OLD_FTP_PROXY_HOST_KEY);

        tstr = gconf_client_get_string (client, src, NULL);
        if (tstr != NULL) {
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
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free  (dest);
        g_free (src);

        /* OLD_SOCKS_PROXY_HOST */
        dest = copy_location_create_key (to, OLD_SOCKS_PROXY_HOST_KEY);
        src = copy_location_create_key (from, OLD_SOCKS_PROXY_HOST_KEY);

        tstr = gconf_client_get_string (client, src, NULL);
        if (tstr != NULL) {
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
        if (tstr != NULL) {
                gconf_client_set_string (client, dest, tstr, NULL);
                g_free (tstr);
        }

        g_free (dest);
        g_free (src);
}

static char *
get_current_location (GConfClient *client)
{
        char *result;

        result = gconf_client_get_string (client, CURRENT_LOCATION, NULL);

        if (result == NULL || result[0] == '\0') {
                g_free (result);
                result = g_strdup (_("Default"));
        }

        return result;
}

static gboolean
location_combo_separator (GtkTreeModel *model,
                          GtkTreeIter  *iter,
                          gpointer      data)
{
        char    *name;
        gboolean ret;

        gtk_tree_model_get (model, iter, COL_NAME, &name, -1);

        ret = name == NULL || name[0] == '\0';

        g_free (name);

        return ret;
}

static void
update_locations (CcNetworkProxyPage *page);

static void
on_location_changed (GtkWidget          *location,
                     CcNetworkProxyPage *page);

static void
on_current_location_gconf_changed (GConfClient        *client,
                                   guint               cnxn_id,
                                   GConfEntry         *entry,
                                   CcNetworkProxyPage *page)
{
        GConfValue *value;
        const char *newval;

        g_debug ("Current location changed");
        value = gconf_entry_get_value (entry);
        if (value == NULL)
                return;

        newval = gconf_value_get_string (value);
        if (newval == NULL)
                return;

        /* prevent the current settings from being saved by blocking
         * the signal handler */
        g_signal_handlers_block_by_func (page->priv->location_combobox,
                                         on_location_changed,
                                         page);
        update_locations (page);
        g_signal_handlers_unblock_by_func (page->priv->location_combobox,
                                           on_location_changed,
                                           page);
}

static void
update_locations (CcNetworkProxyPage *page)
{
        int           i;
        int           select;
        char         *current;
        GtkComboBox  *location = GTK_COMBO_BOX (page->priv->location_combobox);
        GSList       *list;
        GtkTreeIter   titer;
        GtkListStore *store;
        GSList       *l;
        GSList       *last;
        GConfClient  *client;

        client = gconf_client_get_default ();
        list = gconf_client_all_dirs (client, LOCATION_DIR, NULL);
        store = GTK_LIST_STORE (gtk_combo_box_get_model (location));
        gtk_list_store_clear (store);

        current = get_current_location (client);

        list = g_slist_append (list, g_strconcat (LOCATION_DIR "/", current, NULL));
        list = g_slist_sort (list, (GCompareFunc) strcmp);

        select = -1;

        for (i = 0, l = list, last = NULL; l != NULL; last = l, l = l->next, ++i) {

                if (last == NULL || strcmp (last->data, l->data) != 0) {
                        char *dirname;
                        char *locp;
                        char *key_name;

                        dirname = l->data;
                        locp = dirname + strlen (LOCATION_DIR) + 1;
                        key_name = gconf_unescape_key (locp, -1);

                        gtk_list_store_append (store, &titer);
                        gtk_list_store_set (store,
                                            &titer,
                                            COL_NAME, key_name,
                                            COL_STYLE, PANGO_STYLE_NORMAL,
                                            -1);

                        g_free (key_name);

                        if (strcmp (locp, current) == 0)
                                select = i;
                }
        }
        if (select == -1) {
                gtk_list_store_append (store, &titer);
                gtk_list_store_set (store,
                                    &titer,
                                    COL_NAME , current,
                                    COL_STYLE, PANGO_STYLE_NORMAL,
                                    -1);
                select = i++;
        }
        gtk_widget_set_sensitive (page->priv->delete_button, i > 1);

        gtk_list_store_append (store, &titer);
        gtk_list_store_set (store,
                            &titer,
                            COL_NAME, NULL,
                            COL_STYLE, PANGO_STYLE_NORMAL,
                            -1);

        gtk_list_store_append (store, &titer);
        gtk_list_store_set (store,
                            &titer,
                            COL_NAME, _("New Location..."),
                            COL_STYLE, PANGO_STYLE_ITALIC,
                            -1);

        gtk_combo_box_set_row_separator_func (location, location_combo_separator, NULL, NULL);
        gtk_combo_box_set_active (location, select);
        g_free (current);
        g_slist_foreach (list, (GFunc) gconf_entry_free, NULL);
        g_slist_free (list);

        g_object_unref (client);
}

static void
on_location_new_text_changed (GtkEntry           *entry,
                              CcNetworkProxyPage *page)
{
        gboolean     exists;
        char        *current, *esc, *key;
        const char  *name;
        GConfClient *client;

        client = gconf_client_get_default ();

        name = gtk_entry_get_text (entry);
        if (name != NULL && name[0] != '\0') {
                esc = gconf_escape_key (name, -1);

                key = g_strconcat (LOCATION_DIR "/", esc, NULL);
                g_free (esc);

                current = get_current_location (client);

                exists = (strcmp (current, name) == 0) ||
                        gconf_client_dir_exists (client, key, NULL);
                g_free (key);
        } else {
                exists = FALSE;
        }

        g_object_unref (client);

        if (exists) {
                gtk_widget_show (page->priv->error_label);
        } else {
                gtk_widget_hide (page->priv->error_label);
        }

        gtk_widget_set_sensitive (page->priv->new_location,
                                  !exists);
}

static void
location_new (CcNetworkProxyPage *page)
{
        GtkBuilder  *builder;
        GError      *error = NULL;
        char        *builder_widgets[] = { "location_new_dialog",
                                           "new_location_btn_img", NULL };
        GtkWidget   *askdialog;
        const char  *name;
        int          response;
        GConfClient *client;
        GtkWidget   *toplevel;

        client = gconf_client_get_default ();

        builder = gtk_builder_new ();
        if (gtk_builder_add_objects_from_file (builder,
                                               GNOMECC_GNP_UI_FILE,
                                               builder_widgets,
                                               &error) == 0) {
                g_warning ("Could not load location dialog: %s",
                           error->message);
                g_error_free (error);
                g_object_unref (builder);
                return;
        }

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        askdialog = WID ("location_new_dialog");
        gtk_window_set_transient_for (GTK_WINDOW (askdialog),
                                      GTK_WINDOW (toplevel));

        g_signal_connect (askdialog,
                          "response",
                          G_CALLBACK (gtk_widget_hide),
                          NULL);

        g_signal_connect (WID ("text"),
                          "changed",
                          G_CALLBACK (on_location_new_text_changed),
                          page);
        response = gtk_dialog_run (GTK_DIALOG (askdialog));
        name = gtk_entry_get_text (GTK_ENTRY (WID ("text")));
        g_object_unref (builder);

        if (response == GTK_RESPONSE_OK && name[0] != '\0') {
                gboolean exists;
                char    *current, *esc, *key;

                esc = gconf_escape_key (name, -1);
                key = g_strconcat (LOCATION_DIR "/", esc, NULL);
                g_free (esc);

                current = get_current_location (client);

                exists = (strcmp (current, name) == 0)
                        || gconf_client_dir_exists (client, key, NULL);

                g_free (key);

                if (!exists) {
                        esc = gconf_escape_key (current, -1);
                        g_free (current);
                        key = g_strconcat (LOCATION_DIR "/", esc, NULL);
                        g_free (esc);

                        copy_location ("", key, client);
                        g_free (key);

                        gconf_client_set_string (client, CURRENT_LOCATION, name, NULL);
                        update_locations (page);
                } else {
                        GtkWidget *err;

                        err = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_CLOSE,
                                                      _("Location already exists"));
                        gtk_dialog_run (GTK_DIALOG (err));
                        gtk_widget_destroy (err);

                        /* switch back to the currently selected location */
                        gconf_client_notify (client, CURRENT_LOCATION);
                }
        } else {
                /* switch back to the currently selected location */
                gconf_client_notify (client, CURRENT_LOCATION);
        }

        gtk_widget_destroy (askdialog);
        g_object_unref (client);
}

static void
on_location_changed (GtkWidget          *location,
                     CcNetworkProxyPage *page)
{
        char *current;
        char *name = gtk_combo_box_get_active_text (GTK_COMBO_BOX (location));
        GConfClient *client;

        if (name == NULL)
                return;

        client = gconf_client_get_default ();

        current = get_current_location (client);

        if (strcmp (current, name) != 0) {
                if (strcmp (name, _("New Location...")) == 0) {
                        location_new (page);
                } else {
                        char *key, *esc;

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
on_delete_button_clicked (GtkWidget          *button,
                          CcNetworkProxyPage *page)
{
        GConfClient *client;
        GtkComboBox *box = GTK_COMBO_BOX (page->priv->location_combobox);
        int          active = gtk_combo_box_get_active (box);
        char        *current, *key, *esc;

        /* prevent the current settings from being saved by blocking
         * the signal handler */
        g_signal_handlers_block_by_func (box, on_location_changed, page);
        gtk_combo_box_set_active (box, (active == 0) ? 1 : 0);
        gtk_combo_box_remove_text (box, active);
        g_signal_handlers_unblock_by_func (box, on_location_changed, page);

        /* set the new location */
        client = gconf_client_get_default ();
        current = gtk_combo_box_get_active_text (box);

        esc = gconf_escape_key (current, -1);
        key = g_strconcat (LOCATION_DIR "/", esc, NULL);
        g_free (esc);

        copy_location (key, "", client);
        gconf_client_recursive_unset (client,
                                      key,
                                      GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
                                      NULL);
        gconf_client_suggest_sync (client, NULL);
        g_free (key);

        gconf_client_set_string (client, CURRENT_LOCATION, current, NULL);

        g_free (current);

        g_object_unref (client);
}

/* When using the same proxy for all protocols, updates every host_entry
 * as the user types along */
static void
synchronize_hosts (GtkEntry           *entry,
                   CcNetworkProxyPage *page)
{
        GtkWidget *hosts[] = {
                page->priv->secure_host_entry,
                page->priv->ftp_host_entry,
                page->priv->socks_host_entry,
                NULL };
        int         i;
        const char *http_host;

        http_host = gtk_entry_get_text (entry);

        for (i = 0; hosts[i] != NULL; i++) {
                gtk_entry_set_text (GTK_ENTRY (hosts[i]), http_host);
        }
}

/* When using the same proxy for all protocols, copies the value of the
 * http port to the other spinbuttons */
static void
synchronize_ports (GtkSpinButton      *widget,
                   CcNetworkProxyPage *page)
{
        GtkWidget *ports[] = {
                page->priv->secure_port_spinbutton,
                page->priv->ftp_port_spinbutton,
                page->priv->socks_port_spinbutton,
                NULL };
        gdouble http_port;
        int     i;

        http_port = gtk_spin_button_get_value (widget);

        for (i = 0; ports[i] != NULL; i++) {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (ports[i]), http_port);
        }
}

/* Synchronizes all hosts and ports */
static void
synchronize_entries (CcNetworkProxyPage *page)
{
        g_signal_connect (page->priv->http_host_entry,
                          "changed",
                          G_CALLBACK (synchronize_hosts),
                          page);
        g_signal_connect (page->priv->http_port_spinbutton,
                          "value-changed",
                          G_CALLBACK (synchronize_ports),
                          page);
}

/* Unsynchronize hosts and ports */
static void
unsynchronize_entries (CcNetworkProxyPage *page)
{
        g_signal_handlers_disconnect_by_func (page->priv->http_host_entry,
                                              synchronize_hosts,
                                              page);
        g_signal_handlers_disconnect_by_func (page->priv->http_port_spinbutton,
                                              synchronize_ports,
                                              page);
}

static void
on_use_same_proxy_checkbutton_clicked (GtkWidget          *checkbutton,
                                       CcNetworkProxyPage *page)
{
        GConfClient *client;
        gboolean     same_proxy;
        char        *http_proxy;
        gint         http_port;
        char        *host;

        client = gconf_client_get_default ();
        same_proxy = gconf_client_get_bool (client, USE_SAME_PROXY_KEY, NULL);

        http_proxy = gconf_client_get_string (client, HTTP_PROXY_HOST_KEY, NULL);
        http_port = gconf_client_get_int (client, HTTP_PROXY_PORT_KEY, NULL);

        if (same_proxy) {
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

                /* Synchronize entries */
                synchronize_entries (page);
        } else {
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

                /* Hosts and ports should not be synchronized any more */
                unsynchronize_entries (page);
        }

        /* Set the proxy entries insensitive if we are using the same proxy for all */
        gtk_widget_set_sensitive (page->priv->secure_host_entry,
                                  !same_proxy);
        gtk_widget_set_sensitive (page->priv->secure_port_spinbutton,
                                  !same_proxy);
        gtk_widget_set_sensitive (page->priv->ftp_host_entry,
                                  !same_proxy);
        gtk_widget_set_sensitive (page->priv->ftp_port_spinbutton,
                                  !same_proxy);
        gtk_widget_set_sensitive (page->priv->socks_host_entry,
                                  !same_proxy);
        gtk_widget_set_sensitive (page->priv->socks_port_spinbutton,
                                  !same_proxy);

        g_object_unref (client);
}

static char *
get_hostname_from_uri (const char *uri)
{
        const char *start, *end;
        char *host;

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
extract_proxy_host (GConfPropertyEditor *peditor,
                    const GConfValue *orig)
{
        char const *entered_text = gconf_value_get_string (orig);
        GConfValue *res = NULL;

        if (entered_text != NULL) {
                char *host = get_hostname_from_uri (entered_text);

                if (host != NULL) {
                        res = gconf_value_new (GCONF_VALUE_STRING);
                        gconf_value_set_string (res, host);
                        g_free (host);
                }
        }

        return (res != NULL) ? res : gconf_value_copy (orig);
}

static void
on_proxy_mode_radiobutton_clicked (GtkWidget          *widget,
                                   CcNetworkProxyPage *page)
{
        GSList      *mode_group;
        int          mode;
        GConfClient *client;

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
                return;

        mode_group = g_slist_copy (gtk_radio_button_get_group
                                   (GTK_RADIO_BUTTON (page->priv->none_radiobutton)));
        mode_group = g_slist_reverse (mode_group);
        mode = g_slist_index (mode_group, widget);
        g_slist_free (mode_group);

        gtk_widget_set_sensitive (page->priv->manual_box,
                                  mode == PROXYMODE_MANUAL);
        gtk_widget_set_sensitive (page->priv->same_proxy_checkbutton,
                                  mode == PROXYMODE_MANUAL);
        gtk_widget_set_sensitive (page->priv->auto_box,
                                  mode == PROXYMODE_AUTO);

        client = gconf_client_get_default ();
        gconf_client_set_bool (client, USE_PROXY_KEY,
                               mode == PROXYMODE_AUTO
                               || mode == PROXYMODE_MANUAL,
                               NULL);
        g_object_unref (client);
}

static void
connect_sensitivity_signals (CcNetworkProxyPage *page,
                             GSList             *mode_group)
{
        for (; mode_group != NULL; mode_group = mode_group->next) {
                g_signal_connect (G_OBJECT (mode_group->data),
                                  "clicked",
                                  G_CALLBACK (on_proxy_mode_radiobutton_clicked),
                                  page);
        }
}

static void
on_ignore_hosts_gconf_changed (GConfClient        *client,
                               guint               cnxn_id,
                               GConfEntry         *entry,
                               CcNetworkProxyPage *page)
{
        g_slist_foreach (page->priv->ignore_hosts, (GFunc) g_free, NULL);
        g_slist_free (page->priv->ignore_hosts);

        page->priv->ignore_hosts = gconf_client_get_list (client,
                                                          IGNORE_HOSTS_KEY,
                                                          GCONF_VALUE_STRING,
                                                          NULL);

        populate_ignored_hosts_model (page, page->priv->ignore_hosts);
}

static void
setup_page (CcNetworkProxyPage *page)
{
        GtkBuilder          *builder;
        GtkWidget           *widget;
        GtkWidget           *hbox;
        GError              *error;
        GConfClient         *client;
        GConfPropertyEditor *peditor;
        GSList              *mode_group;
        GType                mode_type = 0;
        int                  port_value;
        GtkCellRenderer     *location_renderer;
        GtkListStore        *store;
        GtkCellRenderer     *renderer;

        client = gconf_client_get_default ();

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_GNP_UI_FILE,
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        gconf_client_add_dir (client,
                              "/system/http_proxy",
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
        gconf_client_add_dir (client,
                              "/system/proxy",
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);


        mode_type = g_enum_register_static ("NetworkPreferencesProxyType",
                                            proxytype_values);

        /* Locations */
        page->priv->delete_button = gtk_button_new_with_label (_("Delete Location"));
        gtk_widget_show (page->priv->delete_button);

        page->priv->location_combobox = WID ("location_combobox");
        store = gtk_list_store_new (2,
                                    G_TYPE_STRING,
                                    G_TYPE_INT);
        gtk_combo_box_set_model (GTK_COMBO_BOX (page->priv->location_combobox),
                                 GTK_TREE_MODEL (store));

        update_locations (page);
        gconf_client_add_dir (client,
                              LOCATION_DIR,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);

        gconf_client_notify_add (client,
                                 CURRENT_LOCATION,
                                 (GConfClientNotifyFunc) on_current_location_gconf_changed,
                                 page,
                                 NULL,
                                 NULL);

        gconf_client_notify_add (client,
                                 IGNORE_HOSTS_KEY,
                                 (GConfClientNotifyFunc) on_ignore_hosts_gconf_changed,
                                 page,
                                 NULL,
                                 NULL);

        g_signal_connect (page->priv->location_combobox,
                          "changed",
                          G_CALLBACK (on_location_changed),
                          page);


        location_renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (page->priv->location_combobox),
                                    location_renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (page->priv->location_combobox),
                                        location_renderer,
                                        "text", COL_NAME,
                                        "style", COL_STYLE,
                                        NULL);

        page->priv->new_location = WID ("new_location");
        page->priv->error_label = WID ("error_label");
        page->priv->manual_box = WID ("manual_box");
        page->priv->auto_box = WID ("auto_box");

        /* Mode */
        page->priv->none_radiobutton = WID ("none_radiobutton");
        mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (page->priv->none_radiobutton));

        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_select_radio_with_enum (NULL,
                                                                                   PROXY_MODE_KEY,
                                                                                   mode_group,
                                                                                   mode_type,
                                                                                   TRUE,
                                                                                   NULL));

        /* Use same proxy for all protocols */
        page->priv->same_proxy_checkbutton = WID ("same_proxy_checkbutton");
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (NULL,
                                                                    USE_SAME_PROXY_KEY,
                                                                    page->priv->same_proxy_checkbutton,
                                                                    NULL));

        g_signal_connect (page->priv->same_proxy_checkbutton,
                          "toggled",
                          G_CALLBACK (on_use_same_proxy_checkbutton_clicked),
                          page);

        /* Http */
        page->priv->http_port_spinbutton = WID ("http_port_spinbutton");
        page->priv->http_host_entry = WID ("http_host_entry");

        port_value = gconf_client_get_int (client, HTTP_PROXY_PORT_KEY, NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->priv->http_port_spinbutton),
                                   (gdouble) port_value);
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   HTTP_PROXY_HOST_KEY,
                                                                   page->priv->http_host_entry,
                                                                   "conv-from-widget-cb",
                                                                   extract_proxy_host,
                                                                   NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (NULL,
                                                                    HTTP_PROXY_PORT_KEY,
                                                                    page->priv->http_port_spinbutton,
                                                                    NULL));
        page->priv->details_button = WID ("details_button");
        g_signal_connect (page->priv->details_button,
                          "clicked",
                          G_CALLBACK (on_http_details_button_clicked),
                          page);

        /* Secure */
        page->priv->secure_port_spinbutton = WID ("secure_port_spinbutton");
        page->priv->secure_host_entry = WID ("secure_host_entry");

        port_value = gconf_client_get_int (client,
                                           SECURE_PROXY_PORT_KEY,
                                           NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->priv->secure_port_spinbutton),
                                   (gdouble) port_value);
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   SECURE_PROXY_HOST_KEY,
                                                                   page->priv->secure_host_entry,
                                                                   "conv-from-widget-cb",
                                                                   extract_proxy_host,
                                                                   NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (NULL,
                                                                    SECURE_PROXY_PORT_KEY,
                                                                    page->priv->secure_port_spinbutton,
                                                                    NULL));

        /* Ftp */
        page->priv->ftp_port_spinbutton = WID ("ftp_port_spinbutton");
        page->priv->ftp_host_entry = WID ("ftp_host_entry");

        port_value = gconf_client_get_int (client, FTP_PROXY_PORT_KEY, NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->priv->ftp_port_spinbutton),
                                   (gdouble) port_value);
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   FTP_PROXY_HOST_KEY,
                                                                   page->priv->ftp_host_entry,
                                                                   "conv-from-widget-cb",
                                                                   extract_proxy_host,
                                                                   NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (NULL,
                                                                    FTP_PROXY_PORT_KEY,
                                                                    page->priv->ftp_port_spinbutton,
                                                                    NULL));

        /* Socks */
        page->priv->socks_port_spinbutton = WID ("socks_port_spinbutton");
        page->priv->socks_host_entry = WID ("socks_host_entry");

        port_value = gconf_client_get_int (client, SOCKS_PROXY_PORT_KEY, NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (page->priv->socks_port_spinbutton),
                                   (gdouble) port_value);
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   SOCKS_PROXY_HOST_KEY,
                                                                   page->priv->socks_host_entry,
                                                                   "conv-from-widget-cb",
                                                                   extract_proxy_host,
                                                                   NULL));
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (NULL,
                                                                    SOCKS_PROXY_PORT_KEY,
                                                                    page->priv->socks_port_spinbutton,
                                                                    NULL));

        /* Set the proxy entries insensitive if we are using the same proxy for all,
           and make sure they are all synchronized */
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->priv->same_proxy_checkbutton))) {
                gtk_widget_set_sensitive (page->priv->secure_host_entry, FALSE);
                gtk_widget_set_sensitive (page->priv->secure_port_spinbutton, FALSE);
                gtk_widget_set_sensitive (page->priv->ftp_host_entry, FALSE);
                gtk_widget_set_sensitive (page->priv->ftp_port_spinbutton, FALSE);
                gtk_widget_set_sensitive (page->priv->socks_host_entry, FALSE);
                gtk_widget_set_sensitive (page->priv->socks_port_spinbutton, FALSE);

                synchronize_entries (page);
        }

        /* Autoconfiguration */
        page->priv->autoconfig_entry = WID ("autoconfig_entry");
        peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (NULL,
                                                                   PROXY_AUTOCONFIG_URL_KEY,
                                                                   page->priv->autoconfig_entry,
                                                                   NULL));

        page->priv->ignore_hosts = gconf_client_get_list (client,
                                                          IGNORE_HOSTS_KEY,
                                                          GCONF_VALUE_STRING,
                                                          NULL);
        g_object_unref (client);

        page->priv->ignored_hosts_treeview = WID ("treeview_ignore_host");
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (page->priv->ignored_hosts_treeview),
                                                     -1,
                                                     "Hosts", renderer,
                                                     "text", 0,
                                                     NULL);
        store = gtk_list_store_new (1, G_TYPE_STRING);
        gtk_tree_view_set_model (GTK_TREE_VIEW (page->priv->ignored_hosts_treeview),
                                 GTK_TREE_MODEL (store));
        populate_ignored_hosts_model (page, page->priv->ignore_hosts);

        page->priv->ignored_host_add_button = WID ("button_add_url");
        page->priv->ignored_host_remove_button = WID ("button_remove_url");
        page->priv->ignored_host_entry = WID ("entry_url");
        g_signal_connect (page->priv->ignored_host_add_button,
                          "clicked",
                          G_CALLBACK (on_add_button_clicked),
                          page);
        g_signal_connect (page->priv->ignored_host_entry,
                          "activate",
                          G_CALLBACK (on_ignored_host_entry_activate),
                          page);
        g_signal_connect (page->priv->ignored_host_remove_button,
                          "clicked",
                          G_CALLBACK (on_remove_button_clicked),
                          page);

        connect_sensitivity_signals (page, mode_group);

        /* FIXME: move to .ui once it isn't shared */
        hbox = WID ("hbox2");

        gtk_box_pack_end (GTK_BOX (hbox), page->priv->delete_button, FALSE,
                          FALSE, 0);
        g_signal_connect (page->priv->delete_button,
                          "clicked",
                          G_CALLBACK (on_delete_button_clicked),
                          page);

        widget = WID ("network_proxy_vbox");

        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);

        g_object_unref (client);
        g_object_unref (builder);
}

static GObject *
cc_network_proxy_page_constructor (GType                  type,
                                   guint                  n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
        CcNetworkProxyPage      *network_proxy_page;

        network_proxy_page = CC_NETWORK_PROXY_PAGE (G_OBJECT_CLASS (cc_network_proxy_page_parent_class)->constructor (type,
                                                                                                                      n_construct_properties,
                                                                                                                      construct_properties));

        g_object_set (network_proxy_page,
                      "display-name", _("Proxy"),
                      "id", "proxy",
                      NULL);

        setup_page (network_proxy_page);

        return G_OBJECT (network_proxy_page);
}

static void
start_working (CcNetworkProxyPage *page)
{
        static gboolean once = FALSE;

        if (!once) {
                once = TRUE;
        }
}

static void
stop_working (CcNetworkProxyPage *page)
{

}

static void
cc_network_proxy_page_active_changed (CcPage  *base_page,
                                      gboolean is_active)
{
        CcNetworkProxyPage *page = CC_NETWORK_PROXY_PAGE (base_page);

        if (is_active)
                start_working (page);
        else
                stop_working (page);

        CC_PAGE_CLASS (cc_network_proxy_page_parent_class)->active_changed (base_page, is_active);

}

static void
cc_network_proxy_page_class_init (CcNetworkProxyPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
        CcPageClass   *page_class = CC_PAGE_CLASS (klass);

        object_class->get_property = cc_network_proxy_page_get_property;
        object_class->set_property = cc_network_proxy_page_set_property;
        object_class->constructor = cc_network_proxy_page_constructor;
        object_class->finalize = cc_network_proxy_page_finalize;

        page_class->active_changed = cc_network_proxy_page_active_changed;

        g_type_class_add_private (klass, sizeof (CcNetworkProxyPagePrivate));
}

static void
cc_network_proxy_page_init (CcNetworkProxyPage *page)
{
        page->priv = CC_NETWORK_PROXY_PAGE_GET_PRIVATE (page);
}

static void
cc_network_proxy_page_finalize (GObject *object)
{
        CcNetworkProxyPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_NETWORK_PROXY_PAGE (object));

        page = CC_NETWORK_PROXY_PAGE (object);

        g_return_if_fail (page->priv != NULL);


        G_OBJECT_CLASS (cc_network_proxy_page_parent_class)->finalize (object);
}

CcPage *
cc_network_proxy_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_NETWORK_PROXY_PAGE, NULL);

        return CC_PAGE (object);
}
