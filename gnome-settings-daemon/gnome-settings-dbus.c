#include <stdio.h>
#include <X11/Xlib.h>
#include <dbus/dbus-glib-bindings.h>

#include "gnome-settings-keyboard-xkb.h"

static GObjectClass *parent_class = NULL;

typedef struct KeyboardConfigRegistry KeyboardConfigRegistry;
typedef struct KeyboardConfigRegistryClass KeyboardConfigRegistryClass;

struct KeyboardConfigRegistry {
	GObject parent;

	XklConfigRegistry *registry;
};

struct KeyboardConfigRegistryClass {
	GObjectClass parent;
	DBusGConnection *connection;
};

#define KEYBOARD_CONFIG_TYPE_REGISTRY              (keyboard_config_registry_get_type ())
#define KEYBOARD_CONFIG_REGISTRY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), KEYBOARD_CONFIG_TYPE_REGISTRY, KeyboardConfigRegistry))
#define KEYBOARD_CONFIG_REGISTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), KEYBOARD_CONFIG_TYPE_REGISTRY, KeyboardConfigRegistryClass))
#define KEYBOARD_CONFIG_IS_REGISTRY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), KEYBOARD_CONFIG_TYPE_REGISTRY))
#define KEYBOARD_CONFIG_IS_REGISTRY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), KEYBOARD_CONFIG_TYPE_REGISTRY))
#define KEYBOARD_CONFIG_REGISTRY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), KEYBOARD_CONFIG_TYPE_REGISTRY, KeyboardConfigRegistryClass))

gboolean
settings_daemon_awake (GObject * object, GError ** error)
{
	return TRUE;
}

gboolean
    keyboard_config_registry_get_current_descriptions_as_utf8
    (KeyboardConfigRegistry * registry,
     gchar *** short_layout_descriptions,
     gchar *** long_layout_descriptions,
     gchar *** short_variant_descriptions,
     gchar *** long_variant_descriptions, GError ** error) {
	if (!
	    (xkl_engine_get_features (xkl_engine) &
	     XKLF_MULTIPLE_LAYOUTS_SUPPORTED))
		return FALSE;

	XklConfigRec *xkl_config = xkl_config_rec_new ();

	if (!xkl_config_rec_get_from_server (xkl_config, xkl_engine))
		return FALSE;

	char **pl = xkl_config->layouts;
	char **pv = xkl_config->variants;
	guint total_layouts = g_strv_length (xkl_config->layouts);
	gchar **sld = *short_layout_descriptions =
	    g_new0 (char *, total_layouts + 1);
	gchar **lld = *long_layout_descriptions =
	    g_new0 (char *, total_layouts + 1);
	gchar **svd = *short_variant_descriptions =
	    g_new0 (char *, total_layouts + 1);
	gchar **lvd = *long_variant_descriptions =
	    g_new0 (char *, total_layouts + 1);

	while (pl != NULL && *pl != NULL) {
		XklConfigItem item;

		g_snprintf (item.name, sizeof item.name, "%s", *pl);
		if (xkl_config_registry_find_layout
		    (registry->registry, &item)) {
			*sld++ = g_strdup (item.short_description);
			*lld++ = g_strdup (item.description);
		} else {
			*sld++ = g_strdup ("");
			*lld++ = g_strdup ("");
		}

		if (*pv != NULL) {
			g_snprintf (item.name, sizeof item.name, "%s",
				    *pv);
			if (xkl_config_registry_find_variant
			    (registry->registry, *pl, &item)) {
				*svd = g_strdup (item.short_description);
				*lvd = g_strdup (item.description);
			} else {
				*svd++ = g_strdup ("");
				*lvd++ = g_strdup ("");
			}
		} else {
			*svd++ = g_strdup ("");
			*lvd++ = g_strdup ("");
		}

		*pl++, *pv++;
	}
	g_object_unref (G_OBJECT (xkl_config));

	return TRUE;
}

G_DEFINE_TYPE (KeyboardConfigRegistry, keyboard_config_registry,
	       G_TYPE_OBJECT)
#include "gnome-settings-server.h"
static void
finalize (GObject * object)
{
	KeyboardConfigRegistry *registry;

	registry = KEYBOARD_CONFIG_REGISTRY (object);
	if (registry->registry == NULL) {
		return;
	}

	g_object_unref (registry->registry);
	registry->registry = NULL;

	g_object_unref (xkl_engine);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
keyboard_config_registry_class_init (KeyboardConfigRegistryClass * klass)
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
	dbus_g_object_type_install_info (KEYBOARD_CONFIG_TYPE_REGISTRY,
					 &dbus_glib_settings_daemon_object_info);

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = finalize;

	parent_class = g_type_class_peek_parent (klass);
}

static void
keyboard_config_registry_init (KeyboardConfigRegistry * registry)
{
	GError *error = NULL;
	DBusGProxy *driver_proxy;
	KeyboardConfigRegistryClass *klass =
	    KEYBOARD_CONFIG_REGISTRY_GET_CLASS (registry);
	unsigned request_ret;

	/* Register DBUS path */
	dbus_g_connection_register_g_object (klass->connection,
					     "/org/gnome/SettingsDaemon",
					     G_OBJECT (registry));

	/* Register the service name, the constant here are defined in dbus-glib-bindings.h */
	driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (driver_proxy, "org.gnome.SettingsDaemon", 0, &request_ret,	/* See tutorial for more infos about these */
						&error)) {
		g_warning ("Unable to register service: %s",
			   error->message);
		g_error_free (error);
	}
	g_object_unref (driver_proxy);

	/* Init libxklavier stuff */
	g_object_ref (xkl_engine);

	registry->registry = xkl_config_registry_get_instance (xkl_engine);

	xkl_config_registry_load (registry->registry);
}
