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
#include "gnome-settings-font.h"
#include "gnome-settings-xsettings.h"
#include "gnome-settings-mouse.h"
#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-keyboard.h"
#include "gnome-settings-background.h"
#include "gnome-settings-sound.h"
#include "gnome-settings-accessibility-keyboard.h"
#include "gnome-settings-screensaver.h"
#include "gnome-settings-default-editor.h"
#include "gnome-settings-keybindings.h"
#include "gnome-settings-multimedia-keys.h"
#include "gnome-settings-gtk1theme.h"
#include "gnome-settings-xrdb.h"
#include "gnome-settings-typing-break.h"

#include "clipboard-manager.h"

static GObjectClass *parent_class = NULL;

struct _GnomeSettingsDaemonPrivate {
	GHashTable *loaded_modules;
};

XSettingsManager **managers = NULL;
static ClipboardManager *clipboard_manager;

static void
clipboard_manager_terminate_cb (void *data)
{
	/* Do nothing */
}

static GdkFilterReturn 
clipboard_manager_event_filter (GdkXEvent *xevent,
				GdkEvent  *event,
				gpointer   data)
{
	if (clipboard_manager_process_event (clipboard_manager,
					     (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
clipboard_manager_watch_cb (Window  window,
			    Bool    is_start,
			    long    mask,
			    void   *cb_data)
{
	GdkWindow *gdkwin;
	GdkDisplay *display;

	display = gdk_display_get_default ();
	gdkwin = gdk_window_lookup_for_display (display, window);

	if (is_start) {
		if (!gdkwin)
			gdkwin = gdk_window_foreign_new_for_display (display, window);
		else
			g_object_ref (gdkwin);
      
		gdk_window_add_filter (gdkwin, clipboard_manager_event_filter, NULL);
	} else {
		if (!gdkwin)
			return;
		gdk_window_remove_filter (gdkwin, clipboard_manager_event_filter, NULL);
		g_object_unref (gdkwin);
	}
}

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
		gnome_settings_module_initialize (GNOME_SETTINGS_MODULE (l->data), client);
	}
}

static void
start_modules (GnomeSettingsDaemon *daemon, GnomeSettingsModuleRunlevel runlevel)
{
	GList *l, *module_list;

	module_list = g_hash_table_lookup (daemon->priv->loaded_modules, &runlevel);
	for (l = module_list; l != NULL; l = l->next)
		gnome_settings_module_start (GNOME_SETTINGS_MODULE (l->data));
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

	clipboard_manager_destroy (clipboard_manager);

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
	if (!gnome_settings_module_background_get_type ())
		return;

	/* create hash table for loaded modules */
	settings->priv->loaded_modules = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, free_modules_list);

	module_types = g_type_children (GNOME_SETTINGS_TYPE_MODULE, &n_children);
	if (module_types) {
		guint i;

		for (i = 0; i < n_children; i++) {
			GObject *module;
			GnomeSettingsModuleRunlevel runlevel;
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
				g_hash_table_insert (settings->priv->loaded_modules, &runlevel, module_list);
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

	if (!clipboard_manager_check_running (GDK_DISPLAY_XDISPLAY (display))) {
		clipboard_manager = clipboard_manager_new (GDK_DISPLAY_XDISPLAY (display),
							   gdk_error_trap_push,
							   gdk_error_trap_pop,
							   clipboard_manager_terminate_cb,
							   clipboard_manager_watch_cb,
							   NULL);
	}

	client = gnome_settings_get_config_client ();

        /*  gnome_settings_disk_init (client);*/
	gnome_settings_font_init (client);
	gnome_settings_xsettings_init (client);
	gnome_settings_mouse_init (client);
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_set_post_activation_callback ((PostActivationCallback)gnome_settings_load_modmap_files, NULL);
	gnome_settings_keyboard_xkb_init (client);
	gnome_settings_keyboard_init (client);
	gnome_settings_multimedia_keys_init (client);
        /* */
	gnome_settings_sound_init (client);
	gnome_settings_accessibility_keyboard_init (client);
	gnome_settings_screensaver_init (client);
	gnome_settings_default_editor_init (client);
	gnome_settings_keybindings_init (client);
	gnome_settings_gtk1_theme_init (client);
	gnome_settings_xrdb_init (client);
	gnome_settings_typing_break_init (client);

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
	gnome_settings_font_load (client);
	gnome_settings_xsettings_load (client);
	gnome_settings_mouse_load (client);
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_load (client);
	gnome_settings_keyboard_load (client);
	gnome_settings_multimedia_keys_load (client);
	/* */
	gnome_settings_sound_load (client);
	gnome_settings_accessibility_keyboard_load (client);
	gnome_settings_screensaver_load (client);
	gnome_settings_default_editor_load (client);
	gnome_settings_keybindings_load (client);
	gnome_settings_gtk1_theme_load (client);
	gnome_settings_xrdb_load (client);
	gnome_settings_typing_break_load (client);

	/* start all modules */
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES);
	start_modules (daemon, GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES);

	/* create DBus server */
	dbusServer = g_object_new (gnome_settings_server_get_type (), NULL);

	return G_OBJECT (daemon);
}
