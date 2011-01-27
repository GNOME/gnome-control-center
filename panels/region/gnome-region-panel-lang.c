/*
 * Copyright (C) 2010 Bastien Nocera
 *
 * Written by: Bastien Nocera <hadess@hadess.net>
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
#include <glib/gi18n.h>

#include "gnome-region-panel-lang.h"
#include "cc-common-language.h"
#include "gdm-languages.h"

static GDBusProxy *proxy = NULL;

static void
add_other_users_language (GHashTable *ht)
{
	GVariant *variant;
	GVariantIter *vi;
	GError *error = NULL;
	const char *str;

	if (proxy == NULL)
		return;

	variant = g_dbus_proxy_call_sync (proxy,
					  "ListCachedUsers",
					  NULL,
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  &error);
	if (variant == NULL) {
		g_warning ("Failed to list existing users: %s", error->message);
		g_error_free (error);
		return;
	}

	g_variant_get (variant, "(ao)", &vi);
	while (g_variant_iter_loop (vi, "o", &str)) {
		GDBusProxy *user;
		GVariant *props;
		const char *name;
		char *language;

		user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						      G_DBUS_PROXY_FLAGS_NONE,
						      NULL,
						      "org.freedesktop.Accounts",
						      str,
						      "org.freedesktop.Accounts.User",
						      NULL,
						      &error);
		if (user == NULL) {
			g_warning ("Failed to get proxy for user '%s': %s",
				   str, error->message);
			g_error_free (error);
			error = NULL;
			continue;
		}
		props = g_dbus_proxy_get_cached_property (user, "Language");
		name = g_variant_get_string (props, NULL);
		language = gdm_get_language_from_name (name, NULL);
		g_hash_table_insert (ht, g_strdup (name), language);
		g_variant_unref (props);
		g_object_unref (user);
	}
	g_variant_iter_free (vi);
	g_variant_unref (variant);
}

static GHashTable *
new_ht_for_user_languages (void)
{
	GHashTable *ht;
	char *name;
	char *language;

	ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	/* Add the languages used by other users on the system */
	add_other_users_language (ht);

	/* Add current locale */
	name = cc_common_language_get_current_language ();
	if (g_hash_table_lookup (ht, name) == NULL) {
		language = gdm_get_language_from_name (name, NULL);
		g_hash_table_insert (ht, name, language);
	} else {
		g_free (name);
	}

	return ht;
}

static void
selection_changed (GtkTreeSelection *selection,
		   GtkTreeView      *list)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *locale;
	GDBusProxy *user;
	GVariant *variant;
	GError *error = NULL;
	char *object_path;

	if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE) {
		g_warning ("No selected languages, this shouldn't happen");
		return;
	}

	user = NULL;
	variant = NULL;

	gtk_tree_model_get (model, &iter,
			    LOCALE_COL, &locale,
			    -1);

	if (proxy == NULL) {
		g_warning ("Would change the language to '%s', but no D-Bus connection available", locale);
		goto bail;
	}

	variant = g_dbus_proxy_call_sync (proxy,
					  "FindUserByName",
					  g_variant_new ("(s)", g_get_user_name ()),
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  &error);
	if (variant == NULL) {
		g_warning ("Could not contact accounts service to look up '%s': %s",
			   g_get_user_name (), error->message);
		g_error_free (error);
		goto bail;
	}

	g_variant_get (variant, "(o)", &object_path);
	user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_NONE,
					      NULL,
					      "org.freedesktop.Accounts",
					      object_path,
					      "org.freedesktop.Accounts.User",
					      NULL,
					      &error);
	g_free (object_path);

	if (user == NULL) {
		g_warning ("Could not create proxy for user '%s': %s",
			   g_variant_get_string (variant, NULL), error->message);
		g_error_free (error);
		goto bail;
	}
	g_variant_unref (variant);

	variant = g_dbus_proxy_call_sync (user,
					  "SetLanguage",
					  g_variant_new ("(s)", locale),
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  &error);
	if (variant == NULL) {
		g_warning ("Failed to set the language '%s': %s", locale, error->message);
		g_error_free (error);
		goto bail;
	}

	/* And done */

bail:
	if (variant != NULL)
		g_variant_unref (variant);
	if (user != NULL)
		g_object_unref (user);
	g_free (locale);
}

static void
remove_timeout (gpointer data,
		GObject *where_the_object_was)
{
	guint timeout = GPOINTER_TO_UINT (data);
	g_source_remove (timeout);
}

static gboolean
finish_language_setup (gpointer user_data)
{
	GtkWidget *list = (GtkWidget *) user_data;
	GtkTreeModel *model;
	GtkWidget *parent;
	GHashTable *user_langs;
	guint timeout;
	GtkTreeSelection *selection;

	/* Did we get called after the widget was destroyed? */
	if (list == NULL)
		return FALSE;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	user_langs = g_object_get_data (G_OBJECT (list), "user-langs");

	cc_common_language_add_available_languages (GTK_LIST_STORE (model), user_langs);

	parent = gtk_widget_get_toplevel (list);
	gdk_window_set_cursor (gtk_widget_get_window (parent), NULL);

	g_object_set_data (G_OBJECT (list), "user-langs", NULL);
	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (list), "timeout"));
	g_object_weak_unref (G_OBJECT (list), (GWeakNotify) remove_timeout, GUINT_TO_POINTER (timeout));

	/* And select the current language */
	cc_common_language_select_current_language (GTK_TREE_VIEW (list));

	/* And now listen for changes */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (selection_changed), list);

	return FALSE;
}

void
setup_language (GtkBuilder *builder)
{
	GtkWidget *treeview;
	GHashTable *user_langs;
	GtkWidget *parent;
	GtkTreeModel *model;
	GdkWindow *window;
	guint timeout;
	GError *error = NULL;

	treeview = GTK_WIDGET (gtk_builder_get_object (builder, "display_language_treeview"));
	parent = gtk_widget_get_toplevel (treeview);

	/* Setup accounts service */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       "org.freedesktop.Accounts",
					       "/org/freedesktop/Accounts",
					       "org.freedesktop.Accounts",
					       NULL,
					       &error);
	if (proxy == NULL) {
		g_warning ("Failed to contact accounts service: %s", error->message);
		g_error_free (error);
	} else {
		g_object_weak_ref (G_OBJECT (treeview), (GWeakNotify) g_object_unref, proxy);
	}

	/* Add user languages */
	user_langs = new_ht_for_user_languages ();
	cc_common_language_setup_list (treeview, user_langs);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

	/* Setup so that the list is populated after the list appears */
	window = gtk_widget_get_window (parent);
	if (window) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gtk_widget_get_window (parent), cursor);
		g_object_unref (cursor);
	}

	g_object_set_data_full (G_OBJECT (treeview), "user-langs",
				user_langs, (GDestroyNotify) g_hash_table_destroy);
	timeout = g_idle_add ((GSourceFunc) finish_language_setup, treeview);
	g_object_set_data (G_OBJECT (treeview), "timeout", GUINT_TO_POINTER (timeout));
	g_object_weak_ref (G_OBJECT (treeview), (GWeakNotify) remove_timeout, GUINT_TO_POINTER (timeout));
}
