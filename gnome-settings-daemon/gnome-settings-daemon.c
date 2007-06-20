/*
 * Copyright © 2001 Red Hat, Inc.
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
 * Authors:  Owen Taylor, Havoc Pennington
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
#include "gnome-settings-xmodmap.h"

/*#include "gnome-settings-disk.h"*/
#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-keyboard.h"
#include "gnome-settings-accessibility-keyboard.h"
#include "gnome-settings-keybindings.h"
#include "gnome-settings-multimedia-keys.h"
#include "gnome-settings-xrdb.h"

struct _GnomeSettingsDaemonPrivate {
	GHashTable *loaded_modules;
};

GType gnome_settings_module_background_get_type (void);
GType gnome_settings_module_clipboard_get_type (void);
GType gnome_settings_module_default_editor_get_type (void);
GType gnome_settings_module_font_get_type (void);
GType gnome_settings_module_gtk1_get_type (void);
GType gnome_settings_module_mouse_get_type (void);
GType gnome_settings_module_screensaver_get_type (void);
GType gnome_settings_module_sound_get_type (void);
GType gnome_settings_module_typing_break_get_type (void);
GType gnome_settings_module_xsettings_get_type (void);

static GObjectClass *parent_class = NULL;
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
free_modules_list (gpointer data)
{
	GList *l = (GList *) data;

	while (l) {
		g_object_unref (G_OBJECT (l->data));
		l = g_list_remove (l, l);
	}
}

static void
initialize_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GList *l, *module_list;
	GConfClient *client;

	client = gnome_settings_get_config_client ();

	module_list = g_hash_table_lookup (daemon->priv->loaded_modules, &runlevel);
	for (l = module_list; l != NULL; l = l->next) {		
		if (!gnome_settings_module_initialize (GNOME_SETTINGS_MODULE (l->data), client))
			g_warning ("Module %s could not be initialized", G_OBJECT_TYPE_NAME (G_OBJECT (l->data)));
	}
}

static void
start_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GList *l, *module_list;

	module_list = g_hash_table_lookup (daemon->priv->loaded_modules, &runlevel);
	for (l = module_list; l != NULL; l = l->next) {
		if (!gnome_settings_module_start (GNOME_SETTINGS_MODULE (l->data)))
			g_warning ("Module %s could not be started", G_OBJECT_TYPE_NAME (G_OBJECT (l->data)));
	}
}

static void
stop_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GList *l, *module_list;

	module_list = g_hash_table_lookup (daemon->priv->loaded_modules, &runlevel);
	for (l = module_list; l != NULL; l = l->next)
		gnome_settings_module_stop (GNOME_SETTINGS_MODULE (l->data));
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

		g_hash_table_destroy (daemon->priv->loaded_modules);
		daemon->priv->loaded_modules = NULL;
	}

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

	settings->priv = g_new (GnomeSettingsDaemonPrivate, 1);

	/* register all internal modules types */
	if (!gnome_settings_module_background_get_type ()
	    || !gnome_settings_module_clipboard_get_type ()
	    || !gnome_settings_module_default_editor_get_type ()
	    || !gnome_settings_module_font_get_type ()
	    || !gnome_settings_module_gtk1_get_type ()
	    || !gnome_settings_module_mouse_get_type ()
	    || !gnome_settings_module_screensaver_get_type ()
	    || !gnome_settings_module_sound_get_type ()
	    || !gnome_settings_module_typing_break_get_type ()
	    || !gnome_settings_module_xsettings_get_type ())
		return;

	/* create hash table for loaded modules */
	settings->priv->loaded_modules = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, free_modules_list);

	module_types = g_type_children (GNOME_SETTINGS_TYPE_MODULE, &n_children);
	if (module_types) {
		guint i;

		for (i = 0; i < n_children; i++) {
			GObject *module;
			GnomeSettingsModuleRunlevel runlevel, *ptr_runlevel;
			GList *module_list;

			module = g_object_new (module_types[i], NULL);
			if (!module)
				continue;

			runlevel = gnome_settings_module_get_runlevel (GNOME_SETTINGS_MODULE (module));
			module_list = g_hash_table_lookup (settings->priv->loaded_modules, &runlevel);
			if (module_list)
				module_list = g_list_append (module_list, module);
			else {
				module_list = g_list_append (NULL, module);
				ptr_runlevel = g_new0 (GnomeSettingsModuleRunlevel, 1);
				*ptr_runlevel = runlevel;
				g_hash_table_insert (settings->priv->loaded_modules, ptr_runlevel, module_list);
			}
		}

		g_free (module_types);
	}
}

G_DEFINE_TYPE (GnomeSettingsDaemon, gnome_settings_daemon, G_TYPE_OBJECT)

GObject *
gnome_settings_daemon_new (void)
{
	gboolean terminated = FALSE;
	GConfClient *client;
	GnomeSettingsDaemon *daemon;
	GdkDisplay *display;
	GObject *dbusServer;
	int i;
	int n_screens;

	display = gdk_display_get_default ();
	n_screens = gdk_display_get_n_screens (display);

	daemon = g_object_new (gnome_settings_daemon_get_type (), NULL);

	if (xsettings_manager_check_running (
		    gdk_x11_display_get_xdisplay (display),
		    gdk_screen_get_number (gdk_screen_get_default ()))) {
		fprintf (stderr, "You can only run one xsettings manager at a time; exiting\n");
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
				fprintf (stderr, "Could not create xsettings manager for screen %d!\n", i);
				return NULL;
			}
		}

		g_assert (i == n_screens);
		managers [i] = NULL;
	}

	client = gnome_settings_get_config_client ();

        /*  gnome_settings_disk_init (client);*/
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_set_post_activation_callback ((PostActivationCallback) gnome_settings_load_modmap_files, NULL);
	gnome_settings_keyboard_xkb_init (client);
	gnome_settings_keyboard_init (client);
	gnome_settings_multimedia_keys_init (client);
        /* */
	gnome_settings_accessibility_keyboard_init (client);
	gnome_settings_keybindings_init (client);
	gnome_settings_xrdb_init (client);

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

	/*  gnome_settings_disk_load (client);*/
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_load (client);
	gnome_settings_keyboard_load (client);
	gnome_settings_multimedia_keys_load (client);
	/* */
	gnome_settings_accessibility_keyboard_load (client);
	gnome_settings_keybindings_load (client);
	gnome_settings_xrdb_load (client);

	/* start all modules */
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES);

	/* create DBus server */
	dbusServer = g_object_new (gnome_settings_server_get_type (), NULL);

	return G_OBJECT (daemon);
}
