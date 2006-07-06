#include <dbus/dbus-glib-bindings.h>
#include <gnome-settings-daemon.h>

static GObjectClass *parent_class = NULL;

typedef struct GnomeSettingsServer GnomeSettingsServer;
typedef struct GnomeSettingsServerClass GnomeSettingsServerClass;

struct GnomeSettingsServer {
	GObject parent;
};

struct GnomeSettingsServerClass {
	GObjectClass parent;
	DBusGConnection *connection;
};

#define GNOME_SETTINGS_TYPE_SERVER              (gnome_settings_server_get_type ())
#define GNOME_SETTINGS_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServer))
#define GNOME_SETTINGS_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServerClass))
#define GNOME_SETTINGS_IS_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GNOME_SETTINGS_TYPE_SERVER))
#define GNOME_SETTINGS_IS_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_SETTINGS_TYPE_SERVER))
#define GNOME_SETTINGS_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_SETTINGS_TYPE_SERVER, GnomeSettingsServerClass))

gboolean
settings_daemon_awake (GObject * object, GError ** error)
{
	return TRUE;
}

G_DEFINE_TYPE (GnomeSettingsServer, gnome_settings_server, G_TYPE_OBJECT)
#include "gnome-settings-server.h"

static void
gnome_settings_server_class_init (GnomeSettingsServerClass * klass)
{
	GError *error = NULL;
	GObjectClass *object_class;

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

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
}

static void
gnome_settings_server_init (GnomeSettingsServer * server)
{
	GError *error = NULL;
	DBusGProxy *driver_proxy;
	GnomeSettingsServerClass *klass =
	    GNOME_SETTINGS_SERVER_GET_CLASS (server);
	unsigned request_ret;

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
		g_warning ("Unable to register service: %s",
			   error->message);
		g_error_free (error);
	}

	g_object_unref (driver_proxy);
}
