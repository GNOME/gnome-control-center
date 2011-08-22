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
#include "gnome-region-panel-formats.h"
#include "gnome-region-panel-system.h"
#include "cc-common-language.h"
#include "cc-language-chooser.h"
#include "gdm-languages.h"

static GDBusProxy *proxy = NULL;

static void
selection_changed (GtkTreeSelection *selection,
                   GtkBuilder       *builder)
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

        /* Update the other tabs */
        formats_update_language (builder, locale);
        system_update_language (builder, locale);

	/* And done */

bail:
	if (variant != NULL)
		g_variant_unref (variant);
	if (user != NULL)
		g_object_unref (user);
	g_free (locale);
}

static void
language_response (GtkDialog *dialog,
                   gint       response_id,
                   GtkWidget *treeview)
{
        gchar *lang;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

        gtk_widget_hide (GTK_WIDGET (dialog));

        if (response_id != GTK_RESPONSE_OK) {
		return;
	}

        lang = cc_language_chooser_get_language (GTK_WIDGET (dialog));

	if (lang == NULL) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (cc_common_language_get_iter_for_language (model, lang, &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
        }

	gtk_widget_grab_focus (treeview);

        g_free (lang);
}

static void
add_language (GtkWidget *button, GtkWidget *treeview)
{
  	GtkWidget *toplevel;
	GtkWidget *chooser;

	toplevel = gtk_widget_get_toplevel (button);
	chooser = g_object_get_data (G_OBJECT (button), "chooser");
	if (chooser == NULL) {
		chooser = cc_language_chooser_new (toplevel, FALSE);

       		g_signal_connect (chooser, "response",
                	          G_CALLBACK (language_response), treeview);
	        g_signal_connect (chooser, "delete-event",
        	                  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

		g_object_set_data_full (G_OBJECT (button), "chooser",
					chooser, (GDestroyNotify)gtk_widget_destroy);
	}
	else {
		cc_language_chooser_clear_filter (chooser);
	}

        gdk_window_set_cursor (gtk_widget_get_window (toplevel), NULL);
        gtk_window_present (GTK_WINDOW (chooser));
}

void
setup_language (GtkBuilder *builder)
{
	GtkWidget *treeview;
	GHashTable *user_langs;
	GError *error = NULL;
	GtkWidget *widget;
	GtkStyleContext *context;
	GtkTreeSelection *selection;

        /* Setup junction between toolbar and treeview */
        widget = (GtkWidget *)gtk_builder_get_object (builder, "language-swindow");
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
        widget = (GtkWidget *)gtk_builder_get_object (builder, "language-toolbar");
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);
	
	treeview = GTK_WIDGET (gtk_builder_get_object (builder, "display_language_treeview"));

	/* Connect buttons */
	widget = (GtkWidget *)gtk_builder_get_object (builder, "language_add");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (add_language), treeview);	

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
	user_langs = cc_common_language_get_initial_languages ();
	cc_common_language_setup_list (treeview, user_langs);

        /* And select the current language */
        cc_common_language_select_current_language (GTK_TREE_VIEW (treeview));

        /* And now listen for changes */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_signal_connect (G_OBJECT (selection), "changed",
                          G_CALLBACK (selection_changed), builder);

	gtk_widget_grab_focus (treeview);
}
