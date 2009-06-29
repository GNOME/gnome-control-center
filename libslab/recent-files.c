/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "recent-files.h"

#include <gtk/gtk.h>
#include <gio/gio.h>

G_DEFINE_TYPE (MainMenuRecentMonitor, main_menu_recent_monitor, G_TYPE_OBJECT)
G_DEFINE_TYPE (MainMenuRecentFile,    main_menu_recent_file,    G_TYPE_OBJECT)

#define MAIN_MENU_RECENT_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_RECENT_MONITOR_TYPE, MainMenuRecentMonitorPrivate))
#define MAIN_MENU_RECENT_FILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_RECENT_FILE_TYPE, MainMenuRecentFilePrivate))

typedef struct {
	GFileMonitor *monitor;

	gulong changed_handler_id;
} MainMenuRecentMonitorPrivate;

typedef struct {
	GtkRecentInfo *item_obj;
} MainMenuRecentFilePrivate;

enum {
	STORE_CHANGED,
	LAST_SIGNAL
};

static guint monitor_signals [LAST_SIGNAL] = { 0 };

static void main_menu_recent_monitor_finalize (GObject *);
static void main_menu_recent_file_finalize    (GObject *);

static GList *get_files (MainMenuRecentMonitor *, gboolean);

static void
recent_file_store_monitor_cb (GFileMonitor *, GFile *,
                              GFile *, GFileMonitorEvent, gpointer);

static gint recent_item_mru_comp_func (gconstpointer, gconstpointer);

static void
main_menu_recent_monitor_class_init (MainMenuRecentMonitorClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = main_menu_recent_monitor_finalize;

	this_class->changed = NULL;

	g_type_class_add_private (this_class, sizeof (MainMenuRecentMonitorPrivate));

	monitor_signals [STORE_CHANGED] = g_signal_new (
		"changed", G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (MainMenuRecentMonitorClass, changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
main_menu_recent_file_class_init (MainMenuRecentFileClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = main_menu_recent_file_finalize;

	g_type_class_add_private (this_class, sizeof (MainMenuRecentFilePrivate));
}

static void
main_menu_recent_monitor_init (MainMenuRecentMonitor *this)
{
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

	priv->model = NULL;

	priv->changed_handler_id = 0;
}

static void
main_menu_recent_file_init (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

	priv->item_obj = NULL;

	priv->uri       = NULL;
	priv->mime_type = NULL;
}

static void
main_menu_recent_monitor_finalize (GObject *g_object)
{
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (g_object);

	g_file_monitor_cancel (priv->monitor);
	g_object_unref (priv->monitor);

	(* G_OBJECT_CLASS (main_menu_recent_monitor_parent_class)->finalize) (g_object);
}

static void
main_menu_recent_file_finalize (GObject *g_object)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (g_object);

	if (priv->item_obj)
		gtk_recent_info_unref (priv->item_obj);

	(* G_OBJECT_CLASS (main_menu_recent_file_parent_class)->finalize) (g_object);
}

MainMenuRecentMonitor *
main_menu_recent_monitor_new ()
{
	MainMenuRecentMonitor *this = g_object_new (MAIN_MENU_RECENT_MONITOR_TYPE, NULL);
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

	GtkRecentManager *manager;
	gchar *store_path;
	GFile *store_file;


	manager = gtk_recent_manager_get_default ();
	g_object_get (G_OBJECT (manager), "filename", & store_path, NULL);
	store_file = g_file_new_for_path (store_path);

	priv->monitor = g_file_monitor_file (store_file,
					     0, NULL, NULL);
	if (priv->monitor) {
		g_signal_connect (priv->monitor, "changed",
				  G_CALLBACK (recent_file_store_monitor_cb),
				  this);
	}

	g_free (store_path);
	g_object_unref (store_file);

	return this;
}

static GList *
get_files (MainMenuRecentMonitor *this, gboolean apps)
{
	GList *list;
	GList *items;

	GtkRecentManager *manager = gtk_recent_manager_get_default ();

	GtkRecentInfo *info;

	gboolean include;

	MainMenuRecentFile *item;
	MainMenuRecentFilePrivate *item_priv;

	GList *node;


	list = gtk_recent_manager_get_items (manager);

	items = NULL;

	for (node = list; node; node = node->next) {
		item = g_object_new (MAIN_MENU_RECENT_FILE_TYPE, NULL);
		item_priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (item);

		info = (GtkRecentInfo *) node->data;

		include = (apps && gtk_recent_info_has_group (info, "recently-used-apps")) ||
			(! apps && ! gtk_recent_info_get_private_hint (info));

		if (include) {
			item_priv->item_obj = info;

			items = g_list_insert_sorted (items, item, recent_item_mru_comp_func);
		}
		else
			g_object_unref (item);
	}

	g_list_free (list);

	return items;
}

GList *
main_menu_get_recent_files (MainMenuRecentMonitor *this)
{
	return get_files (this, FALSE);
}


GList *
main_menu_get_recent_apps (MainMenuRecentMonitor *this)
{
	return get_files (this, TRUE);
}

const gchar *
main_menu_recent_file_get_uri (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

	return gtk_recent_info_get_uri (priv->item_obj);
}

const gchar *
main_menu_recent_file_get_mime_type (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

	return gtk_recent_info_get_mime_type (priv->item_obj);
}

time_t
main_menu_recent_file_get_modified (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

	return gtk_recent_info_get_modified (priv->item_obj);
}

void
main_menu_rename_recent_file (MainMenuRecentMonitor *this, const gchar *uri_0, const gchar *uri_1)
{
	GtkRecentManager *manager;

	GError *error = NULL;


	manager = gtk_recent_manager_get_default ();

	gtk_recent_manager_move_item (manager, uri_0, uri_1, & error);

	if (error) {
		g_warning ("unable to update recent file store with renamed file, [%s] -> [%s]\n", uri_0, uri_1);

		g_error_free (error);
	}
}

void
main_menu_remove_recent_file (MainMenuRecentMonitor *this, const gchar *uri)
{
	GtkRecentManager *manager;

	GError *error = NULL;


	manager = gtk_recent_manager_get_default ();

	gtk_recent_manager_remove_item (manager, uri, & error);

	if (error) {
		g_warning ("unable to update recent file store with removed file, [%s]\n", uri);

		g_error_free (error);
	}
}

static void
recent_file_store_monitor_cb (
	GFileMonitor *mon, GFile *f1,
	GFile *f2, GFileMonitorEvent event_type, gpointer user_data)
{
	g_signal_emit ((MainMenuRecentMonitor *) user_data, monitor_signals [STORE_CHANGED], 0);
}

static gint
recent_item_mru_comp_func (gconstpointer a, gconstpointer b)
{
	time_t modified_a, modified_b;


	modified_a = main_menu_recent_file_get_modified ((MainMenuRecentFile *) a);
	modified_b = main_menu_recent_file_get_modified ((MainMenuRecentFile *) b);

	return modified_b - modified_a;
}
