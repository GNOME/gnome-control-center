/*
 * Copyright © 2001, 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Owen Taylor, Havoc Pennington, Ray Strode, Rodrigo Moya
 */
#include <config.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <gconf/gconf.h>
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>

#include "xsettings-manager.h"
#include "gnome-settings-daemon.h"
#include "gnome-settings-module.h"

struct _GnomeSettingsDaemonPrivate {
	GSList *loaded_modules;
	GObject *dbus_service;
};

GType gnome_settings_module_accessibility_keyboard_get_type (void);
GType gnome_settings_module_background_get_type (void);
GType gnome_settings_module_clipboard_get_type (void);
GType gnome_settings_module_default_editor_get_type (void);
GType gnome_settings_module_xrandr_get_type (void);
GType gnome_settings_module_font_get_type (void);
GType gnome_settings_module_gtk1_get_type (void);
GType gnome_settings_module_keybindings_get_type (void);
GType gnome_settings_module_keyboard_get_type (void);
GType gnome_settings_module_mouse_get_type (void);
GType gnome_settings_module_multimedia_keys_get_type (void);
GType gnome_settings_module_screensaver_get_type (void);
GType gnome_settings_module_sound_get_type (void);
GType gnome_settings_module_typing_break_get_type (void);
GType gnome_settings_module_xrdb_get_type (void);
GType gnome_settings_module_xsettings_get_type (void);

static GObject *parent_class = NULL;
XSettingsManager **managers = NULL;

static void
terminate_cb (void *data)
{
	gboolean *terminated = data;

	if (*terminated)
		return;

	*terminated = TRUE;
	gtk_main_quit ();
}

static GdkFilterReturn
manager_event_filter (GdkXEvent *xevent,
		      GdkEvent  *event,
		      gpointer   data)
{
	int screen_num = GPOINTER_TO_INT (data);

	g_return_val_if_fail (managers != NULL, GDK_FILTER_CONTINUE);

	if (xsettings_manager_process_event (managers [screen_num], (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
initialize_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GConfClient *client;
	GSList *l;

	client = gnome_settings_get_config_client ();

	for (l = daemon->priv->loaded_modules; l != NULL; l = l->next) {
		GnomeSettingsModule *module = (GnomeSettingsModule *) l->data;

		if (gnome_settings_module_get_runlevel (module) == runlevel &&
		    !gnome_settings_module_initialize (module, client))
			g_warning ("Module %s could not be initialized", G_OBJECT_TYPE_NAME (module));
	}
}

static void
start_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GSList *l;

	for (l = daemon->priv->loaded_modules; l != NULL; l = l->next) {
		GnomeSettingsModule *module = (GnomeSettingsModule *) l->data;

		if (gnome_settings_module_get_runlevel (module) == runlevel &&
		    !gnome_settings_module_start (module))
			g_warning ("Module %s could not be started", G_OBJECT_TYPE_NAME (module));
	}
}

static void
stop_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GSList *l;

	for (l = daemon->priv->loaded_modules; l != NULL; l = l->next) {
		GnomeSettingsModule *module = (GnomeSettingsModule *) l->data;

		if (gnome_settings_module_get_runlevel (module) == runlevel)
			gnome_settings_module_stop (module);
	}
}

static void
finalize (GObject *object)
{
	GnomeSettingsDaemon *daemon;
	int i;

	daemon = GNOME_SETTINGS_DAEMON (object);
	if (daemon->priv == NULL)
		return;

	for (i = 0; managers && managers [i]; i++)
		xsettings_manager_destroy (managers [i]);

	if (daemon->priv->loaded_modules) {
		/* call _stop method on modules, in runlevel-descending order */
		stop_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES);
		stop_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES);
		stop_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS);
		stop_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS);

		g_slist_foreach (daemon->priv->loaded_modules, (GFunc) g_object_unref, NULL);
		g_slist_free (daemon->priv->loaded_modules);
		daemon->priv->loaded_modules = NULL;
	}

	g_object_unref (daemon->priv->dbus_service);
	g_free (daemon->priv);
	daemon->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_settings_daemon_class_init (GnomeSettingsDaemonClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = finalize;

	parent_class = g_type_class_peek_parent (klass);
}

static void
gnome_settings_daemon_init (GnomeSettingsDaemon *settings)
{
	GType *module_types;
	guint n_children;

	settings->priv = g_new0 (GnomeSettingsDaemonPrivate, 1);

	/* register all internal modules types */
	if (!gnome_settings_module_accessibility_keyboard_get_type ()
	    || !gnome_settings_module_background_get_type ()
	    || !gnome_settings_module_clipboard_get_type ()
	    || !gnome_settings_module_default_editor_get_type ()
	    || !gnome_settings_module_xrandr_get_type ()
	    || !gnome_settings_module_font_get_type ()
	    || !gnome_settings_module_gtk1_get_type ()
	    || !gnome_settings_module_keybindings_get_type ()
	    || !gnome_settings_module_keyboard_get_type ()
	    || !gnome_settings_module_mouse_get_type ()
	    || !gnome_settings_module_multimedia_keys_get_type ()
	    || !gnome_settings_module_screensaver_get_type ()
	    || !gnome_settings_module_sound_get_type ()
	    || !gnome_settings_module_typing_break_get_type ()
	    || !gnome_settings_module_xrdb_get_type ()
	    || !gnome_settings_module_xsettings_get_type ())
		return;

	module_types = g_type_children (GNOME_SETTINGS_TYPE_MODULE, &n_children);
	if (module_types) {
		guint i;

		for (i = 0; i < n_children; i++) {
			GObject *module;

			module = g_object_new (module_types[i], NULL);
			if (!module)
				continue;

			settings->priv->loaded_modules =
				g_slist_append (settings->priv->loaded_modules, module);
		}

		g_free (module_types);
	}

	/* create DBus server */
	settings->priv->dbus_service = g_object_new (gnome_settings_server_get_type (), NULL);
}

G_DEFINE_TYPE (GnomeSettingsDaemon, gnome_settings_daemon, G_TYPE_OBJECT)

static gboolean
start_modules_idle_cb (gpointer user_data)
{
	GnomeSettingsDaemon *daemon = user_data;

	/* start all modules */
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES);

	return FALSE;
}

static gboolean
init_modules_idle_cb (gpointer user_data)
{
	int i;
	int n_screens;
	GdkDisplay *display;
	GnomeSettingsDaemon *daemon = user_data;

	display = gdk_display_get_default ();
	n_screens = gdk_display_get_n_screens (display);

	/* load all modules */
	initialize_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS);
	initialize_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS);
	initialize_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES);
	initialize_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES);

	for (i = 0; i < n_screens; i++) {
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);
		gdk_window_add_filter (
			gdk_screen_get_root_window (screen),
			manager_event_filter, GINT_TO_POINTER (i));
	}

	g_idle_add ((GSourceFunc) start_modules_idle_cb, daemon);

	return FALSE;
}

GObject *
gnome_settings_daemon_new (void)
{
	gboolean terminated = FALSE;
	GnomeSettingsDaemon *daemon;
	GdkDisplay *display;
	int i;
	int n_screens;

	display = gdk_display_get_default ();
	n_screens = gdk_display_get_n_screens (display);

	daemon = g_object_new (gnome_settings_daemon_get_type (), NULL);

	if (xsettings_manager_check_running (
		    gdk_x11_display_get_xdisplay (display),
		    gdk_screen_get_number (gdk_screen_get_default ()))) {
		g_error ("You can only run one xsettings manager at a time; exiting\n");
		return NULL;
	}

	if (!terminated) {
		managers = g_new (XSettingsManager *, n_screens + 1);

		for (i = 0; i < n_screens; i++) {
			GdkScreen *screen;

			screen = gdk_display_get_screen (display, i);

			managers [i] = xsettings_manager_new (
				gdk_x11_display_get_xdisplay (display),
				gdk_screen_get_number (screen),
				terminate_cb, &terminated);
			if (!managers [i]) {
				g_error ("Could not create xsettings manager for screen %d!\n", i);
				return NULL;
			}
		}

		managers [i] = NULL;
	}

	g_idle_add ((GSourceFunc) init_modules_idle_cb, daemon);

	return G_OBJECT (daemon);
}
