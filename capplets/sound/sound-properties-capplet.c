/* -*- mode: c; style: linux -*- */

/* sound-properties-capplet.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#include <gnome.h>
#include <gconf/gconf-client.h>

#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

#include <glade/glade.h>

#include <dbus/dbus-glib-lowlevel.h>
#if USE_HAL
#include <libhal.h>
#endif
#include <gst/gst.h>
#include <gst/interfaces/propertyprobe.h>

/* Needed only for the sound capplet */

#include <stdlib.h>
#include <sys/types.h>

#include "activate-settings-daemon.h"
#include "pipeline-tests.h"
#include "mixer-support.h"
#include "sound-theme.h"

typedef enum {
	AUDIO_PLAYBACK,
	AUDIO_CAPTURE,
	VIDEO_PLAYBACK,
	VIDEO_CAPTURE
} device_type;

typedef enum {
	SYSTEM_ALSA,
	SYSTEM_OSS
} SoundSystemType;

typedef struct _DeviceChooser
{
	const gchar *profile;
	int type;
	GtkWidget *combobox;
	GtkListStore *model;
	const gchar *test_pipeline;
} DeviceChooser;

#define EVENT_SOUNDS_KEY           "/desktop/gnome/sound/event_sounds"
#define INPUT_SOUNDS_KEY           "/desktop/gnome/sound/input_feedback_sounds"
#define DEFAULT_MIXER_DEVICE_KEY   "/desktop/gnome/sound/default_mixer_device"
#define DEFAULT_MIXER_TRACKS_KEY   "/desktop/gnome/sound/default_mixer_tracks"
#define AUDIO_BELL_KEY             "/apps/metacity/general/audible_bell"
#define GST_GCONF_DIR              "/system/gstreamer/0.10"

#define AUDIO_TEST_SOURCE          "audiotestsrc wave=sine freq=512"
#define VIDEO_TEST_SOURCE          "videotestsrc"
#define AUDIO_TEST_IN_BETWEEN      " ! audioconvert ! audioresample ! "
#define VIDEO_TEST_IN_BETWEEN      " ! ffmpegcolorspace ! "

/* Capplet-specific prototypes */

static GConfClient     *gconf_client = NULL;
static GtkWidget       *dialog_win = NULL;
static GList           *device_choosers = NULL;

static gboolean
CheckXKB (void)
{
	gboolean have_xkb = FALSE;
	Display *dpy;
	int opcode, errorBase, major, minor, xkbEventBase;

	gdk_error_trap_push ();
	dpy = GDK_DISPLAY ();
	have_xkb = XkbQueryExtension (dpy, &opcode, &xkbEventBase,
				      &errorBase, &major, &minor)
		   && XkbUseExtension (dpy, &major, &minor);
	XSync (dpy, FALSE);
	gdk_error_trap_pop ();

	return have_xkb;
}

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;
	GtkWidget *image;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/sound-properties.glade", "sound_prefs_dialog", NULL);
	if (dialog == NULL)
		return NULL;

	image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("sounds_playback_test")), image);

	image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("music_playback_test")), image);

	image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("chat_audio_playback_test")), image);

	image = gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("chat_audio_capture_test")), image);

	return dialog;
}

static gboolean
element_available (const gchar *pipeline)
{
	gboolean res = FALSE;
	gchar *p, *first_space;

	if (pipeline == NULL || *pipeline == '\0')
		return FALSE;

	p = g_strdup (pipeline);

	g_strstrip (p);

	/* skip the check and pretend all is fine if it's something that does
	 * not look like an element name (e.g. parentheses to signify a bin) */
	if (!g_ascii_isalpha (*p)) {
		g_free (p);
		return TRUE;
	}

	/* just the element name, no arguments */
	first_space = strchr (p, ' ');
	if (first_space != NULL)
		*first_space = '\0';

	/* check if element is available */
	res = gst_default_registry_check_feature_version (p, GST_VERSION_MAJOR,
		GST_VERSION_MINOR, 0);

	g_free (p);
	return res;
}
static void
add_device (int type, const gchar *pipeline, const gchar *description, const gchar *selection)
{
	GList *l;
	DeviceChooser *device_chooser;
	gchar *row_pipeline;
	gchar *description_mod;
	const gchar *suffix;
	gboolean selected, row_selected;
	gboolean found_pipeline, found_selected;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeIter found_pipeline_iter = { };
	GtkTreeIter found_selected_iter = { };

	/* only display pipelines available on this system */
	if (!element_available (pipeline))
		return;

	for (l = device_choosers; l != NULL; l = l->next) {
		device_chooser = (DeviceChooser *) l->data;

		if (device_chooser->type != type)
			continue;

		/* if we're adding/updating the currently selected device
		 * we're only interested in the DeviceChooser for the
		 * corresponding profile */
		if (selection != NULL && strcmp (selection, device_chooser->profile) != 0)
			continue;

		selected = (selection != NULL);

		found_pipeline = FALSE;
		found_selected = FALSE;

		model = GTK_TREE_MODEL (device_chooser->model);

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gtk_tree_model_get (model, &iter, 0, &row_pipeline, 2, &row_selected, -1);

				if (strcmp (row_pipeline, pipeline) == 0) {
					found_pipeline_iter = iter;
					found_pipeline = TRUE;
				}
				g_free (row_pipeline);

				if (row_selected) {
					found_selected_iter = iter;
					found_selected = TRUE;
				}

			} while (gtk_tree_model_iter_next (model, &iter));
		}

		if (found_pipeline) {
			/* the specified pipeline is already in the model */
			iter = found_pipeline_iter;
			if (!selected) {
				/* if the specified pipeline was only already in
				 * the model because it's currently selected,
				 * clear the corresponding flag in the model
				 * (happens when reconnecting usb audio devices) */
				gtk_list_store_set (device_chooser->model, &iter, 2, FALSE, -1);
			}
		} else if (selected && found_selected) {
			/* we only allow one custom pipeline in the model */
			iter = found_selected_iter;
		} else {
			/* no existing pipeline found that would be appropriate
			 * to reuse, so append a new row */
			gtk_list_store_append (device_chooser->model, &iter);
			gtk_list_store_set (device_chooser->model, &iter, 2, selected, -1);
		}

		gtk_list_store_set (device_chooser->model, &iter, 0, pipeline, -1);

		if (!selected || !found_pipeline) {
			/* either we're appending a new row or we're updating
			 * a row with non-appropriate description */

			if (selected) {
				/* we add the specified pipeline only because
				 * it's the preferred device but it's neither
				 * a device currently detected by HAL nor a
				 * standard pipeline */
				if (g_str_has_prefix (pipeline, "hal")) {
					suffix = _("Not connected");
				} else {
					suffix = _("Custom");
				}
				description_mod = g_strdup_printf ("%s (%s)", description, suffix);
			} else {
				description_mod = g_strdup (description);
			}
			gtk_list_store_set (device_chooser->model, &iter, 1, description_mod, -1);

			/* we need to store the unmodified description to not
			 * modify already modified descriptions */
			gtk_list_store_set (device_chooser->model, &iter, 3, description, -1);

			g_free (description_mod);
		}

		if (selected) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (device_chooser->combobox), &iter);
		}
	}
}

/* Adds an element which supports the GstPropertyProbe interface. Will add
 * entries for all available devices. Elements should support retrieving the
 * device-name in NULL state for this to work properly/nicely.
 *
 * Returns TRUE if probing was successful and one or more devices were found
 */
static gboolean
add_device_with_probe (int type, const gchar *element_name, const gchar *system_description)
{
	GstPropertyProbe *probe;
	GstElement *element;
	GValueArray *vals;
	gboolean res = FALSE;
	guint i;

	/* only display pipelines available on this system */
	element = gst_element_factory_make (element_name, NULL);
	if (element == NULL)
		return FALSE;

	if (!GST_IS_PROPERTY_PROBE (element))
		goto done;

	probe = GST_PROPERTY_PROBE (element);
	vals = gst_property_probe_probe_and_get_values_name (probe, "device");

	if (vals == NULL)
		goto done;

	for (i = 0; i < vals->n_values; ++i) {
		gchar *device_name = NULL;
		gchar *pipeline, *desc;
		const gchar *device;
		GValue *val;

		val = g_value_array_get_nth (vals, i);
		device = g_value_get_string (val);

		g_object_set (element, "device", device, NULL);

		/* we assume the element is able to retrieve the description
		 * even if the device is in NULL state (ie. not open) */
		g_object_get (element, "device-name", &device_name, NULL);

		if (device_name) {
			desc = g_strdup_printf ("%s - %s", system_description, device_name);
		} else {
			/* fallback, shouldn't happen */
			desc = g_strdup_printf ("%s - %s", system_description, device);
		}

		pipeline = g_strdup_printf ("%s device=%s", element_name, device);

		add_device (type, pipeline, desc, NULL);
		res = TRUE;

		g_free (pipeline);
		g_free (desc);
		g_free (device_name);
	}

	g_value_array_free (vals);

done:
	gst_object_unref (element);
	return res;
}

#if USE_HAL
static void
remove_device (int type, const gchar *pipeline)
{
	GList *l;
	DeviceChooser *device_chooser;
	GtkTreeIter iter;
	gchar *row_pipeline;
	gchar *description;
	gchar *description_mod;
	GtkTreeModel *model;

	for (l = device_choosers; l != NULL; l = l->next) {
		device_chooser = (DeviceChooser *) l->data;

		if (device_chooser->type != type)
			continue;

		model = GTK_TREE_MODEL (device_chooser->model);

		gtk_combo_box_get_active_iter (GTK_COMBO_BOX (device_chooser->combobox), &iter);
		gtk_tree_model_get (model, &iter, 0, &row_pipeline, -1);

		/* don't remove currently selected devices */
		if (strcmp (row_pipeline, pipeline) == 0) {
			g_free (row_pipeline);

			/* let the user know that they have to reconnect the
			 * sound device */
			gtk_tree_model_get (model, &iter, 3, &description, -1);
			description_mod = g_strdup_printf ("%s (%s)", description, _("Not connected"));
			g_free (description);
			gtk_list_store_set (device_chooser->model, &iter, 1, description_mod, -1);
			g_free (description_mod);
		} else {
			g_free (row_pipeline);

			if (gtk_tree_model_get_iter_first (model, &iter)) {
				do {
					gtk_tree_model_get (model, &iter, 0, &row_pipeline, -1);
					if (strcmp (row_pipeline, pipeline) == 0) {
						g_free (row_pipeline);
						gtk_list_store_remove (device_chooser->model, &iter);
						break;
					}
					g_free (row_pipeline);
				} while (gtk_tree_model_iter_next (model, &iter));
			}
		}
	}
}

static gboolean
filter_device_generic (LibHalContext *ctx, const char *udi)
{
	gboolean ignore = FALSE;
	gchar *parent_udi;
	gchar *subsystem;

	parent_udi = libhal_device_get_property_string (ctx, udi, "info.parent", NULL);

	if (!parent_udi)
		return FALSE;

	subsystem = libhal_device_get_property_string (ctx, parent_udi, "info.subsystem", NULL);

	/* filter out pc speaker */
	if (subsystem && (!strcmp (subsystem, "platform") || !strcmp (subsystem, "sound"))) {
		gchar *device_id = libhal_device_get_property_string (ctx, parent_udi, "platform.id", NULL);
		ignore = device_id && !strncmp (device_id, "pcspk", 5);
		libhal_free_string (device_id);
	}

	libhal_free_string (parent_udi);
	libhal_free_string (subsystem);

	return ignore;
}

static gboolean
filter_device_alsa (LibHalContext *ctx, const char *udi)
{
	gboolean ignore;
	gchar *class_string;

	/* filter out "digitizer", "modem", "none", "unknown" */
	class_string = libhal_device_get_property_string (ctx, udi, "alsa.pcm_class", NULL);
	ignore = class_string != NULL
		 && strcmp (class_string, "generic") != 0
		 && strcmp (class_string, "multi") != 0;
	libhal_free_string (class_string);

	if (!ignore)
		ignore = filter_device_generic (ctx, udi);

	return ignore;
}

static gboolean
filter_device_oss (LibHalContext *ctx, const char *udi)
{
	gboolean ignore = FALSE;
	gchar *parent_udi;
	gchar *subsystem;

	parent_udi = libhal_device_get_property_string (ctx, udi, "info.parent", NULL);

	if (!parent_udi)
		return FALSE;

	subsystem = libhal_device_get_property_string (ctx, parent_udi, "info.subsystem", NULL);
	/* filter out modem devices */
	if (subsystem && !strcmp (subsystem, "pci")) {
		dbus_int32_t device_class = libhal_device_get_property_int (ctx, parent_udi, "pci.device_class", NULL);
		/* this means "Simple communications controllers". Maybe there is a headerfile with definitions?
		 * visit http://www.acm.uiuc.edu/sigops/roll_your_own/7.c.1.html for further information */
		ignore = (device_class == 0x7);
	}

	libhal_free_string (parent_udi);
	libhal_free_string (subsystem);

	if (!ignore)
		ignore = filter_device_generic (ctx, udi);

	return ignore;
}

static gchar *
get_device_description (LibHalContext *ctx, const char *udi, SoundSystemType snd_sys)
{
	gchar *card_id = NULL, *device_id = NULL, *product, *desc;
	const gchar *indicator = NULL;

	/* the device number should be reported if there is a second (playback)
	 * device with the same description on the same card */
	/* dbus_int32_t device_num = 0; */

	switch (snd_sys) {
	case SYSTEM_ALSA:
	       	card_id = libhal_device_get_property_string (ctx, udi, "alsa.card_id", NULL);
		device_id = libhal_device_get_property_string (ctx, udi, "alsa.device_id", NULL);
		/* device_num = libhal_device_get_property_int (ctx, udi, "alsa.device", NULL); */
		indicator = "(ALSA)";
		break;
	case SYSTEM_OSS:
		card_id = libhal_device_get_property_string (ctx, udi, "oss.card_id", NULL);
		device_id = libhal_device_get_property_string (ctx, udi, "oss.device_id", NULL);
		/* device_num = libhal_device_get_property_int (ctx, udi, "oss.device", NULL); */
		indicator = "(OSS)";
	}

	/* card_id and device_id are not mandatory in OSS and ALSA namespace
	 * according to the specification so it could be possible go get an
	 * empty string */
	if (card_id && device_id) {
		desc = g_strconcat (card_id, " ", device_id, " ", indicator, NULL);
	} else {
		product = libhal_device_get_property_string (ctx, udi, "info.product", NULL);
		desc = g_strconcat (product, " ", indicator, NULL);
		libhal_free_string (product);
	}
	libhal_free_string (card_id);
	libhal_free_string (device_id);

	return desc;
}

static void
device_added_alsa (LibHalContext *ctx, const char *udi)
{
	gchar *type_string;
	int type;
	const gchar *element;
	gchar *pipeline, *description;

	if (filter_device_alsa (ctx, udi))
		return;

	type_string = libhal_device_get_property_string (ctx, udi, "alsa.type", NULL);
	if (strcmp (type_string, "playback") == 0) {
		type = AUDIO_PLAYBACK;
		element = "halaudiosink";
	} else if (strcmp (type_string, "capture") == 0) {
		type = AUDIO_CAPTURE;
		element = "halaudiosrc";
	} else {
		type = -1;
		element = NULL;
	}
	libhal_free_string (type_string);
	if (type == -1) {
		return;
	}

	pipeline = g_strconcat (element, " udi=", udi, NULL);
	description = get_device_description (ctx, udi, SYSTEM_ALSA);

	add_device (type, pipeline, description, NULL);

	g_free (pipeline);
	g_free (description);
}

static void
device_added_oss (LibHalContext *ctx, const char *udi)
{
	gchar *type_string;
	int type;
	const gchar *element;
	gchar *pipeline, *description;

	if (filter_device_oss (ctx, udi))
		return;

	type_string = libhal_device_get_property_string (ctx, udi, "oss.type", NULL);
	if (strcmp (type_string, "pcm") == 0) {
		type = AUDIO_PLAYBACK;
		element = "halaudiosink";
	} else if (strcmp (type_string, "mixer") == 0) {
		type = AUDIO_CAPTURE;
		element = "halaudiosrc";
	} else {
		type = -1;
		element = NULL;
	}
	libhal_free_string (type_string);
	if (type == -1) {
		return;
	}

	pipeline = g_strconcat (element, " udi=", udi, NULL);
	description = get_device_description (ctx, udi, SYSTEM_OSS);

	add_device (type, pipeline, description, NULL);

	g_free (pipeline);
	g_free (description);
}

static void
device_added_callback (LibHalContext *ctx, const char *udi)
{
	if (libhal_device_query_capability (ctx, udi, "alsa", NULL)) {
		device_added_alsa (ctx, udi);
	} else if (libhal_device_query_capability (ctx, udi, "oss", NULL)) {
		device_added_oss (ctx, udi);
	}

}

static void
device_removed_callback (LibHalContext *ctx, const char *udi)
{
	gchar *pipeline;

	pipeline = g_strconcat ("halaudiosink udi=", udi, NULL);
	remove_device (AUDIO_PLAYBACK, pipeline);
	g_free (pipeline);

	pipeline = g_strconcat ("halaudiosrc udi=", udi, NULL);
	remove_device (AUDIO_CAPTURE, pipeline);
	g_free (pipeline);
}

static void
setup_devices_by_capability (LibHalContext *ctx, const gchar *cap)
{
	DBusError error;
	char **devices;
	int num = 0;

	dbus_error_init (&error);

	devices = libhal_find_device_by_capability (ctx, cap, &num, &error);
	if (devices != NULL) {
		int i;

		for (i = 0; i < num; ++i) {
			device_added_callback (ctx, devices[i]);
		}
		dbus_free_string_array (devices);
	} else {
		if (dbus_error_is_set (&error)) {
			g_warning ("Calling a hal function an error occured: %s", error.message);
			dbus_error_free (&error);
		}
	}
}

static void
setup_hal_devices (void)
{
	DBusConnection *connection;
	DBusError error;
	LibHalContext *ctx;

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (connection == NULL) {
		/* cannot get a dbus connection */
		if (dbus_error_is_set (&error)) {
			g_warning ("Getting a system dbus connection an error occured: %s", error.message);
			dbus_error_free (&error);
		}
		return;
	}

	dbus_connection_setup_with_g_main (connection, g_main_context_default ());

	ctx = libhal_ctx_new ();
	g_return_if_fail (ctx != NULL);

	libhal_ctx_set_device_added (ctx, device_added_callback);
	libhal_ctx_set_device_removed (ctx, device_removed_callback);
	libhal_ctx_set_dbus_connection (ctx, connection);

	if (!libhal_ctx_init (ctx, &error)) {
		/* cannot connect to hald */
		if (dbus_error_is_set (&error)) {
			g_warning ("Connecting to hald an error occured: %s", error.message);
			dbus_error_free (&error);
		}
		return;
	}

	setup_devices_by_capability (ctx, "alsa");
	setup_devices_by_capability (ctx, "oss");
}
#endif

static void
device_test_button_clicked (GtkWidget *button, gpointer user_data)
{
	DeviceChooser *device_chooser = (DeviceChooser *) user_data;

	GladeXML *dialog = glade_xml_new (GNOMECC_GLADE_DIR "/sound-properties.glade", NULL, NULL);

	user_test_pipeline (dialog, GTK_WINDOW (dialog_win), device_chooser->test_pipeline);

	g_object_unref (dialog);
}

static gchar *
get_gconf_key_for_profile (const gchar *profile, int type)
{
	const gchar *element_type;

	switch (type) {
		case AUDIO_PLAYBACK:
			element_type = "audiosink";
			break;
		case AUDIO_CAPTURE:
			element_type = "audiosrc";
			break;
		case VIDEO_PLAYBACK:
			element_type = "videosink";
			break;
		case VIDEO_CAPTURE:
			element_type = "videosrc";
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return g_strdup_printf ("%s/default/%s%s", GST_GCONF_DIR, profile, element_type);
}

static void
device_changed (GtkComboBox *widget, gpointer user_data)
{
	DeviceChooser *device_chooser = (DeviceChooser *) user_data;
	GtkTreeIter iter;
	gchar *pipeline, *description;
	gchar *base_key, *key;

	gtk_combo_box_get_active_iter (widget, &iter);
	gtk_tree_model_get (GTK_TREE_MODEL (device_chooser->model), &iter, 0, &pipeline, 3, &description, -1);

	base_key = get_gconf_key_for_profile (device_chooser->profile, device_chooser->type);
	gconf_client_set_string (gconf_client, base_key, pipeline, NULL);
	g_free (pipeline);
	key = g_strconcat (base_key, "_description", NULL);
	g_free (base_key);
	gconf_client_set_string (gconf_client, key, description, NULL);
	g_free (description);
	g_free (key);
}

static gchar *
get_gconf_description_for_profile (const gchar *profile, int type)
{
	gchar *base_key;
	gchar *key;
	gchar *description;

	base_key = get_gconf_key_for_profile (profile, type);
	key = g_strconcat (base_key, "_description", NULL);
	g_free (base_key);

	description = gconf_client_get_string (gconf_client, key, NULL);
	g_free (key);

	return description;
}

static void
gconf_key_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer data)
{
	DeviceChooser *device_chooser = (DeviceChooser *) data;
	gchar *description;

	description = get_gconf_description_for_profile (device_chooser->profile, device_chooser->type);

	add_device (device_chooser->type, gconf_value_get_string (gconf_entry_get_value (entry)), description, device_chooser->profile);

	g_free (description);
}

static void
setup_device_chooser (const gchar *profile, int type, GtkWidget *combobox, GtkWidget *test_button, const gchar *test_pipeline)
{
	DeviceChooser *device_chooser;
	GtkCellRenderer *cell;
	gchar *gconf_key;

	g_return_if_fail (GTK_IS_COMBO_BOX (combobox));
	g_return_if_fail (GTK_IS_BUTTON (test_button));

	device_chooser = g_malloc0 (sizeof (DeviceChooser));

	device_chooser->profile = profile;
	device_chooser->type = type;
	device_chooser->combobox = combobox;
	device_chooser->model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
	device_chooser->test_pipeline = test_pipeline;
	gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (device_chooser->model));
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), cell, "text", 1);

	device_choosers = g_list_prepend (device_choosers, device_chooser);

	gconf_key = get_gconf_key_for_profile (profile, type);
	gconf_client_notify_add (gconf_client, gconf_key, gconf_key_changed,
				 device_chooser, NULL, NULL);
	g_free (gconf_key);

	g_signal_connect (combobox, "changed", G_CALLBACK (device_changed), device_chooser);

	g_signal_connect (test_button, "clicked", G_CALLBACK (device_test_button_clicked), device_chooser);
}

static void
add_selected_device (const gchar *profile, int type)
{
	gchar *gconf_key;
	gchar *description;
	gchar *pipeline;

	gconf_key = get_gconf_key_for_profile (profile, type);
	pipeline = gconf_client_get_string (gconf_client, gconf_key, NULL);
	g_free (gconf_key);

	g_return_if_fail (pipeline != NULL);

	description = get_gconf_description_for_profile (profile, type);

	add_device (type, pipeline, description, profile);

	g_free (description);
	g_free (pipeline);
}

static void
setup_devices (void)
{
	add_device (AUDIO_PLAYBACK, "autoaudiosink", _("Autodetect"), NULL);

#if USE_HAL
	setup_hal_devices ();
#endif
	add_device (AUDIO_PLAYBACK, "alsasink", _("ALSA - Advanced Linux Sound Architecture"), NULL);
	add_device (AUDIO_CAPTURE, "alsasrc", _("ALSA - Advanced Linux Sound Architecture"), NULL);
	add_device (AUDIO_PLAYBACK, "artsdsink", _("Artsd - ART Sound Daemon"), NULL);
	add_device (AUDIO_PLAYBACK, "esdsink", _("ESD - Enlightened Sound Daemon"), NULL);
	add_device (AUDIO_CAPTURE, "esdmon", _("ESD - Enlightened Sound Daemon"), NULL);
	/* only show legacy OSS if OSS4 isn't active and working */
	if (!add_device_with_probe (AUDIO_PLAYBACK, "oss4sink", "OSS4")) {
		add_device (AUDIO_PLAYBACK, "osssink", _("OSS - Open Sound System"), NULL);
	}
	if (!add_device_with_probe (AUDIO_CAPTURE, "oss4src", "OSS4")) {
		add_device (AUDIO_CAPTURE, "osssrc", _("OSS - Open Sound System"), NULL);
	}
	add_device (AUDIO_PLAYBACK, "pulsesink", _("PulseAudio Sound Server"), NULL);
	add_device (AUDIO_CAPTURE, "pulsesrc", _("PulseAudio Sound Server"), NULL);
	add_device (AUDIO_CAPTURE, "audiotestsrc wave=triangle is-live=true", _("Test Sound"), NULL);
	add_device (AUDIO_CAPTURE, "audiotestsrc wave=silence is-live=true", _("Silence"), NULL);

	add_selected_device ("", AUDIO_PLAYBACK);
	add_selected_device ("music", AUDIO_PLAYBACK);
	add_selected_device ("chat", AUDIO_PLAYBACK);
	add_selected_device ("", AUDIO_CAPTURE);
}

static void
mixer_device_combobox_changed (GtkComboBox *widget, gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (widget, &iter)) {
		GtkTreeModel *model;
		gchar *device = NULL;

		model = gtk_combo_box_get_model (widget);
		gtk_tree_model_get (model, &iter,
				MIXER_DEVICE_MODEL_DEVICE_COLUMN, &device, -1);

		gconf_client_set_string (gconf_client, DEFAULT_MIXER_DEVICE_KEY, device, NULL);
		g_free (device);
	}
}

static void
add_track_label_to_list (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GSList **list)
{
	gchar *label;

	gtk_tree_model_get (model, iter,
			MIXER_TRACKS_MODEL_LABEL_COLUMN, &label,
			-1);

	*list = g_slist_prepend (*list, label);
}

static void
mixer_tracks_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GSList *label_list = NULL;

	gtk_tree_selection_selected_foreach (selection,
			(GtkTreeSelectionForeachFunc)add_track_label_to_list,
			&label_list);

	label_list = g_slist_reverse (label_list);
	gconf_client_set_list (gconf_client, DEFAULT_MIXER_TRACKS_KEY, GCONF_VALUE_STRING, label_list, NULL);

	g_slist_foreach (label_list, (GFunc)g_free, NULL);
	g_slist_free (label_list);
}

static void
update_mixer_tracks_selection (GSList *tracks, GladeXML *dialog)
{
	GtkWidget *tracks_widget;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	tracks_widget = WID ("mixer_tracks");

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tracks_widget));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tracks_widget));

	g_signal_handlers_block_by_func (G_OBJECT (selection), G_CALLBACK (mixer_tracks_selection_changed), NULL);

	gtk_tree_selection_unselect_all (selection);

	if (tracks != NULL) {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *label;
				GSList *t;

				gtk_tree_model_get (model, &iter, MIXER_TRACKS_MODEL_LABEL_COLUMN, &label, -1);

				for (t = tracks; t != NULL; t = t->next) {
					if (!strcmp (label, t->data)) {
						gtk_tree_selection_select_iter (selection, &iter);
						break;
					}
				}
				g_free (label);
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}

	g_signal_handlers_unblock_by_func (G_OBJECT (selection), G_CALLBACK (mixer_tracks_selection_changed), NULL);

	/* FIXME: if none selected, select master track */
}

static void
default_mixer_tracks_notify (GConfClient *client, guint cnxn_id, GConfEntry *entry, GladeXML *dialog)
{
	GSList *tracks;

	tracks = gconf_client_get_list (gconf_client, DEFAULT_MIXER_TRACKS_KEY, GCONF_VALUE_STRING, NULL);

	update_mixer_tracks_selection (tracks, dialog);

	g_slist_foreach (tracks, (GFunc) g_free, NULL);
	g_slist_free (tracks);
}

static void
update_mixer_device_combobox (const gchar *mixer_device, GladeXML *dialog)
{
	GtkWidget *device_widget;
	GtkTreeModel *model;
	GtkTreeIter iter;

	device_widget = WID ("mixer_device");
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (device_widget));

	/* try to find stored factory and device in the mixer device list */
	if (mixer_device != NULL && *mixer_device != '\0') {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *device = NULL;

				gtk_tree_model_get (model, &iter,
						MIXER_DEVICE_MODEL_DEVICE_COLUMN, &device,
						-1);

				if (!strcmp (device, mixer_device)) {
					gtk_combo_box_set_active_iter (GTK_COMBO_BOX (device_widget), &iter);

					g_free (device);
					break;
				}

				g_free (device);
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}

	/* try to select first mixer entry */
	/* FIXME use the first with a master track */
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (device_widget), &iter)) {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (device_widget), &iter);
		}
	}

	/* fill track list */
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (device_widget), &iter)) {
		GtkWidget *tracks_widget;
		GstElement *mixer;
		GtkTreeSelection *selection;
		GtkTreeModel *tracks_model;
		GSList *tracks;

		tracks_widget = WID ("mixer_tracks");

		gtk_tree_model_get (model, &iter,
				MIXER_DEVICE_MODEL_MIXER_COLUMN, &mixer,
				-1);

		gst_element_set_state (mixer, GST_STATE_READY);

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tracks_widget));
		g_signal_handlers_block_by_func (G_OBJECT (selection), G_CALLBACK (mixer_tracks_selection_changed), NULL);

		tracks_model = create_mixer_tracks_tree_model_for_mixer (GST_MIXER (mixer));
		gtk_tree_view_set_model (GTK_TREE_VIEW (tracks_widget), tracks_model);
		g_object_unref (tracks_model);

		gst_element_set_state (mixer, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (mixer));

		/* updated mixer tracks selection */
		tracks = gconf_client_get_list (gconf_client, DEFAULT_MIXER_TRACKS_KEY, GCONF_VALUE_STRING, NULL);
		update_mixer_tracks_selection (tracks, dialog);
		g_slist_foreach (tracks, (GFunc) g_free, NULL);
		g_slist_free (tracks);

		g_signal_handlers_unblock_by_func (G_OBJECT (selection), G_CALLBACK (mixer_tracks_selection_changed), NULL);
	}
}

static void
default_mixer_device_notify (GConfClient *client, guint cnxn_id, GConfEntry *entry, GladeXML *dialog)
{
	const gchar *mixer_device;

	mixer_device = gconf_value_get_string (gconf_entry_get_value (entry));

	update_mixer_device_combobox (mixer_device, dialog);
}

static void
setup_default_mixer (GladeXML *dialog)
{
	GtkWidget *device_widget, *tracks_widget;
	GtkTreeModel *model;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	gchar *mixer_device;

	device_widget = WID ("mixer_device");
	tracks_widget = WID ("mixer_tracks");

	model = create_mixer_device_tree_model ();
	gtk_combo_box_set_model (GTK_COMBO_BOX (device_widget), model);
	g_object_unref (G_OBJECT (model));

	renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (device_widget), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (device_widget), renderer,
			"text", MIXER_DEVICE_MODEL_NAME_COLUMN);

	g_signal_connect (G_OBJECT (device_widget), "changed",
			G_CALLBACK (mixer_device_combobox_changed), NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tracks_widget),
			-1, NULL, renderer,
			"text", MIXER_TRACKS_MODEL_LABEL_COLUMN, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tracks_widget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			G_CALLBACK (mixer_tracks_selection_changed), NULL);

	gconf_client_notify_add (gconf_client, DEFAULT_MIXER_DEVICE_KEY,
			(GConfClientNotifyFunc) default_mixer_device_notify, dialog, NULL, NULL);
	gconf_client_notify_add (gconf_client, DEFAULT_MIXER_TRACKS_KEY,
			(GConfClientNotifyFunc) default_mixer_tracks_notify, dialog, NULL, NULL);

	mixer_device = gconf_client_get_string (gconf_client, DEFAULT_MIXER_DEVICE_KEY, NULL);
	update_mixer_device_combobox (mixer_device, dialog);
	g_free (mixer_device);
}

/* setup_dialog
 *
 * Set up the property editors for our dialog
 */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
	GObject *peditor;
	GtkSizeGroup *combobox_size_group;
	GtkSizeGroup *label_size_group;

	gconf_client_add_dir (gconf_client, GST_GCONF_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	combobox_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	label_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (label_size_group, WID ("sounds_playback_label"));
	gtk_size_group_add_widget (label_size_group, WID ("music_playback_label"));
	gtk_size_group_add_widget (label_size_group, WID ("chat_audio_playback_label"));
	gtk_size_group_add_widget (label_size_group, WID ("chat_audio_capture_label"));
	gtk_size_group_add_widget (label_size_group, WID ("mixer_device_label"));
	gtk_size_group_add_widget (combobox_size_group, WID ("sounds_playback_device"));
	gtk_size_group_add_widget (combobox_size_group, WID ("music_playback_device"));
	gtk_size_group_add_widget (combobox_size_group, WID ("chat_audio_playback_device"));
	gtk_size_group_add_widget (combobox_size_group, WID ("chat_audio_capture_device"));

	setup_device_chooser ("", AUDIO_PLAYBACK, WID ("sounds_playback_device"),
			      WID ("sounds_playback_test"),
			      AUDIO_TEST_SOURCE AUDIO_TEST_IN_BETWEEN "gconfaudiosink");
	setup_device_chooser ("music", AUDIO_PLAYBACK, WID ("music_playback_device"),
			      WID ("music_playback_test"),
			      AUDIO_TEST_SOURCE AUDIO_TEST_IN_BETWEEN "gconfaudiosink profile=music");
	setup_device_chooser ("chat", AUDIO_PLAYBACK, WID ("chat_audio_playback_device"),
			      WID ("chat_audio_playback_test"),
			      AUDIO_TEST_SOURCE AUDIO_TEST_IN_BETWEEN "gconfaudiosink profile=chat");
	setup_device_chooser ("", AUDIO_CAPTURE, WID ("chat_audio_capture_device"),
			      WID ("chat_audio_capture_test"),
			      "gconfaudiosrc" AUDIO_TEST_IN_BETWEEN "gconfaudiosink profile=chat");

	peditor = gconf_peditor_new_boolean (NULL, EVENT_SOUNDS_KEY, WID ("events_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("input_feedback_toggle"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("sound_theme_combobox"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("sounds_treeview"));
	gconf_peditor_new_boolean (NULL, INPUT_SOUNDS_KEY, WID ("input_feedback_toggle"), NULL);
	gconf_peditor_new_boolean (NULL, AUDIO_BELL_KEY, WID ("bell_audible_toggle"), NULL);

	setup_default_mixer (dialog);
	setup_sound_theme (dialog);
	setup_sound_theme_custom (dialog, CheckXKB());
}

/* get_legacy_settings
 *
 * Retrieve older gnome_config -style settings and store them in the
 * configuration database.
 *
 * In most cases, it's best to use the COPY_FROM_LEGACY macro defined in
 * capplets/common/capplet-util.h.
 */

static void
get_legacy_settings (void)
{
	GConfClient *client;
	gboolean val_bool, def;

	client = gconf_client_get_default ();
	COPY_FROM_LEGACY (bool, "/desktop/gnome/sound/event_sounds", "/sound/system/settings/event_sounds=false");
	g_object_unref (G_OBJECT (client));
}

static void
dialog_response_cb (GtkWidget *dialog, gint response_id, GConfChangeSet *changeset)
{
	if (response_id == GTK_RESPONSE_HELP) {
		capplet_help (GTK_WINDOW (dialog),
			"user-guide.xml",
			"goscustmulti-2");
		return;
	}

	gtk_widget_destroy (dialog);
}

int
main (int argc, char **argv)
{
	GConfChangeSet *changeset;
	GladeXML       *dialog;
	GnomeProgram   *program;
 	GOptionContext *context;
	gboolean apply_only = FALSE;
	gboolean get_legacy = FALSE;
 	GOptionEntry cap_options[] = {
 		{ "apply", 0, 0, G_OPTION_ARG_NONE, &apply_only,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "init-session-settings", 0, 0, G_OPTION_ARG_NONE, &apply_only,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
 		{ "get-legacy", 0, 0, G_OPTION_ARG_NONE, &get_legacy,
		  N_("Retrieve and store legacy settings"), NULL },
 		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

        /* Since gstreamer and gnome-vfs require threads, we
         * have to initialise threads here as the first call to glib.
         */
        g_thread_init (NULL);

 	context = g_option_context_new (N_("- GNOME Sound Preferences"));
#if GLIB_CHECK_VERSION (2, 12, 0)
        g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
#endif
 	g_option_context_add_main_entries (context, cap_options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gst_init_get_option_group ());

	program = gnome_program_init ("gnome-sound-properties", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
 			    	      GNOME_PARAM_GOPTION_CONTEXT, context,
				      NULL);

	activate_settings_daemon ();

	gconf_client = gconf_client_get_default ();
	gconf_client_add_dir (gconf_client, "/desktop/gnome/sound", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (gconf_client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		dialog = create_dialog ();

		if (dialog) {
			changeset = gconf_change_set_new ();
			setup_dialog (dialog, changeset);
			setup_devices ();

			dialog_win = WID ("sound_prefs_dialog");
			g_signal_connect (dialog_win, "response", G_CALLBACK (dialog_response_cb), changeset);
			g_signal_connect (dialog_win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
			capplet_set_icon (dialog_win, "gnome-sound-properties");
			gtk_widget_show (dialog_win);

			gtk_main ();
			gconf_change_set_unref (changeset);
			g_object_unref (dialog);
		}
	}

	g_object_unref (gconf_client);
	g_object_unref (program);

	return 0;
}
