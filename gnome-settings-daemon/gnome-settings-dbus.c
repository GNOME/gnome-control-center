/*
 * gnome-settings-dbus.c
 *
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 * USA.
 */


#include <string.h>
#include <dbus/dbus-glib-bindings.h>
#include <gnome-settings-daemon.h>
#include "gnome-settings-marshal.h"
#include "gnome-settings-dbus.h"

static GObjectClass *parent_class = NULL;

typedef struct GnomeSettingsServer GnomeSettingsServer;
typedef struct GnomeSettingsServerClass GnomeSettingsServerClass;

struct GnomeSettingsServer {
	GObject parent;

	/* multimedia player keys */
	GList *media_players;
};

struct GnomeSettingsServerClass {
	GObjectClass parent;
	DBusGConnection *connection;

	void (*media_player_key_pressed) (GObject *server, const gchar *application, const gchar *key);
};

enum {
	MEDIA_PLAYER_KEY_PRESSED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	gchar *application;
	guint32 time;
} MediaPlayer;

#define GNOME_SETTINGS_TYPE_SERVER              (gnome_settings_server_get_type ())
#define GNOME_SETTINGS_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServer))
#define GNOME_SETTINGS_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServerClass))
#define GNOME_SETTINGS_IS_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GNOME_SETTINGS_TYPE_SERVER))
#define GNOME_SETTINGS_IS_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_SETTINGS_TYPE_SERVER))
#define GNOME_SETTINGS_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServerClass))

static gint
find_by_application (gconstpointer a, gconstpointer b)
{
	return strcmp (((MediaPlayer *)a)->application, b);
}

static gint
find_by_time (gconstpointer a, gconstpointer b)
{
	return ((MediaPlayer *)a)->time != 0 && ((MediaPlayer *)a)->time < ((MediaPlayer *)b)->time;
}

static gboolean
settings_daemon_grab_media_player_keys (GnomeSettingsServer *server, const gchar *application, guint32 time, GError **error)
{
	GList *iter;
	MediaPlayer *media_player;

	iter = g_list_find_custom (server->media_players, application, find_by_application);

	if (iter != NULL) {
		if (time == 0 || ((MediaPlayer *)iter->data)->time < time) {
			g_free (((MediaPlayer *)iter->data)->application);
			g_free (iter->data);
			server->media_players = g_list_delete_link (server->media_players, iter);
		} else {
			return TRUE;
		}
	}

	media_player = g_new0 (MediaPlayer, 1);
	media_player->application = g_strdup (application);
	media_player->time = time;

	server->media_players = g_list_insert_sorted (server->media_players, media_player, find_by_time);

	return TRUE;
}

static gboolean
settings_daemon_release_media_player_keys (GnomeSettingsServer *server, const gchar *application, GError **error)
{
	GList *iter;

	iter = g_list_find_custom (server->media_players, application, find_by_application);

	if (iter != NULL) {
		g_free (((MediaPlayer *)iter->data)->application);
		g_free (iter->data);
		server->media_players = g_list_delete_link (server->media_players, iter);
	}

	return TRUE;
}

static gboolean
settings_daemon_awake (GObject * object, GError ** error)
{
	return TRUE;
}

G_DEFINE_TYPE (GnomeSettingsServer, gnome_settings_server, G_TYPE_OBJECT)
#include "gnome-settings-server.h"

GObject *
gnome_settings_server_get (void)
{
	return g_object_new (GNOME_SETTINGS_TYPE_SERVER, NULL);
}

gboolean
gnome_settings_server_media_player_key_pressed (GObject *server, const gchar *key)
{
	const gchar *application = NULL;
	gboolean have_listeners = (GNOME_SETTINGS_SERVER (server)->media_players != NULL);

	if (have_listeners) {
		application = ((MediaPlayer *)GNOME_SETTINGS_SERVER (server)->media_players->data)->application;
	}

	g_signal_emit (server, signals[MEDIA_PLAYER_KEY_PRESSED], 0, application, key);

	return !have_listeners;
}

static GObject*
gnome_settings_server_constructor (GType                  type,
				  guint                  n_construct_params,
				  GObjectConstructParam *construct_params)
{
	static GObject *singleton = NULL;
	GObject *object;

	if (!singleton) {
		singleton = G_OBJECT_CLASS (parent_class)->constructor (type, n_construct_params, construct_params);
		object = singleton;
	} else {
		object = g_object_ref (singleton);
	}

	return object;
}


static void
gnome_settings_server_class_init (GnomeSettingsServerClass * klass)
{
	GError *error = NULL;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = gnome_settings_server_constructor;

	signals[MEDIA_PLAYER_KEY_PRESSED] = g_signal_new ("media-player-key-pressed",
			G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GnomeSettingsServerClass, media_player_key_pressed),
			NULL, NULL, gnome_settings_marshal_VOID__STRING_STRING, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

	/* Init the DBus connection, per-klass */
	klass->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (klass->connection == NULL) {
		g_warning ("Unable to connect to dbus: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* &dbus_glib__object_info is provided in the server-bindings.h file */
	dbus_g_object_type_install_info (GNOME_SETTINGS_TYPE_SERVER,
					 &dbus_glib_settings_daemon_object_info);
}

static void
gnome_settings_server_init (GnomeSettingsServer * server)
{
	GError *error = NULL;
	DBusGProxy *driver_proxy;
	GnomeSettingsServerClass *klass =
	    GNOME_SETTINGS_SERVER_GET_CLASS (server);
	unsigned request_ret;

	if (klass->connection == NULL)
		return;

	/* Register DBUS path */
	dbus_g_connection_register_g_object (klass->connection,
					     "/org/gnome/SettingsDaemon",
					     G_OBJECT (server));

	/* Register the service name, the constant here are defined in dbus-glib-bindings.h */
	driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name
	    (driver_proxy, "org.gnome.SettingsDaemon", 0, &request_ret,
	     &error)) {
		if (error) {
			g_warning ("Unable to register service: %s",
				   error->message);
			g_error_free (error);
		} else {
			g_warning ("Unable to register service: Unknown DBUS error"); 
		}
	}

	g_object_unref (driver_proxy);
}
