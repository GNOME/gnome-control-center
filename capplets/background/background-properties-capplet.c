/* -*- mode: c; style: linux tab-width: 8; c-basic-offset: 8 -*- */

/* background-properties-capplet.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>,
 *             Richard Hestilow <hestilow@ximian.com>
 *             Seth Nickell     <snickell@stanford.edu>
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

#include <config.h>

#include <string.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "applier.h"

typedef enum {
	BACKGROUND_TYPE_NO_PICTURE = 0,
	BACKGROUND_TYPE_WALLPAPER,
	BACKGROUND_TYPE_CENTERED,
	BACKGROUND_TYPE_SCALED,
	BACKGROUND_TYPE_STRETCHED,
	NUMBER_BACKGROUND_TYPES
} BackgroundType;

enum {
	TARGET_URI_LIST
};

static GtkTargetEntry drop_types[] = {
	{"text/uri-list", 0, TARGET_URI_LIST}
};
static gint n_drop_types = sizeof (drop_types) / sizeof (GtkTargetEntry);

BGApplier *appliers[NUMBER_BACKGROUND_TYPES];
GtkWidget *toggle_array[NUMBER_BACKGROUND_TYPES];

GObject *bg_root_applier = NULL;
GObject *bg_preferences = NULL;
GConfClient *gconf_client;

GtkWidget *background_image_preview;
GtkWidget *background_image_label;

GtkWidget *border_shading_label;

GladeXML       *dialog;

static void set_background_picture (const char *filename);

static void
set_picture_is_present (gboolean present)
{
	int i;

	for (i=1; i < NUMBER_BACKGROUND_TYPES; i++) {
		gtk_widget_set_sensitive (GTK_WIDGET (toggle_array[i]), present);
	}
}

static BackgroundType
string_to_background_type (char *string)
{
        BackgroundType type;
      
	if (!strncmp (string, "wallpaper", sizeof ("wallpaper"))) {
	        type =  BACKGROUND_TYPE_WALLPAPER;
	} else if (!strncmp (string, "centered", sizeof ("centered"))) {
	        type =  BACKGROUND_TYPE_CENTERED;
	} else if (!strncmp (string, "scaled", sizeof ("scaled"))) {
	        type =  BACKGROUND_TYPE_SCALED;
	} else if (!strncmp (string, "stretched", sizeof ("stretched"))) {
	        type =  BACKGROUND_TYPE_STRETCHED;
	} else {
	        type = BACKGROUND_TYPE_NO_PICTURE;
	}

	g_free (string);

	return type;
}

static const char *
background_type_to_string (BackgroundType type)
{
	const char *tmp_string;

	switch (type) {
	case 0 /* NO_PICTURE */: tmp_string = "none"; break;
	case 1 /* WALLPAPER  */: tmp_string = "wallpaper"; break;
	case 2 /* CENTERED   */: tmp_string = "centered"; break;
	case 3 /* SCALED     */: tmp_string = "scaled"; break;
	case 4 /* STRETCHED  */: tmp_string = "stretched"; break;
	default: 
		tmp_string = "scaled";
	}

	return tmp_string;
}

static const char *
orientation_to_string (orientation_t orientation)
{
	const char *tmp_string;

	switch (orientation) {
	case ORIENTATION_HORIZ: tmp_string = "horizontal-gradient"; break;
	case ORIENTATION_VERT: tmp_string = "vertical-gradient"; break;
	default: 
		tmp_string = "solid";
	}

	return tmp_string;
}

static orientation_t
string_to_orientation (gchar *string)
{
        orientation_t type;

	if (!strncmp (string, "vertical-gradient", sizeof ("vertical-gradient"))) {
	        type = ORIENTATION_VERT;
	} else if (!strncmp (string, "horizontal-gradient", sizeof ("horizontal-gradient"))) {
	        type = ORIENTATION_HORIZ;
	} else {
	        type = ORIENTATION_SOLID;
	}
	   
	g_free (string);
	return type;
}

static GConfValue *
peditor_string_to_orientation (GConfValue *value)
{
	GConfValue *new_value;
	const char *shading_string;

	shading_string = gconf_value_get_string (value);

	new_value = gconf_value_new (GCONF_VALUE_INT);

	gconf_value_set_int (new_value, string_to_orientation (strdup (shading_string)));
	return new_value;
}

static GConfValue *
peditor_orientation_to_string (GConfValue *value)
{
	GConfValue *new_value;
	int orientation;

	orientation = gconf_value_get_int (value);

	new_value = gconf_value_new (GCONF_VALUE_STRING);

	gconf_value_set_string (new_value, orientation_to_string (orientation));
	return new_value;
}

static gboolean
drag_motion_cb (GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, guint time, gpointer data)
{
	return FALSE;
}

static void
drag_leave_cb (GtkWidget *widget, GdkDragContext *context,
	       guint time, gpointer data)
{
	gtk_widget_queue_draw (widget);
}

static void
drag_data_received_cb (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y,
		       GtkSelectionData *selection_data,
		       guint info, guint time, gpointer data)
{
	GList *list;
	GList *uris;
	GnomeVFSURI *uri;

	if (info == TARGET_URI_LIST) {
		uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->
						 data);
		for (list = uris; list; list = list->next) {
			uri = (GnomeVFSURI *) list->data;
			set_background_picture (gnome_vfs_uri_get_path (uri));
		}
		gnome_vfs_uri_list_free (uris);
	}

}

static void
update_preview_widgets (const BGPreferences *preferences, BGApplier **appliers, BGApplier *bg_root_applier)
{
	BGPreferences *tmp_prefs;

	tmp_prefs = BG_PREFERENCES (bg_preferences_clone (preferences));

	if (!GTK_WIDGET_REALIZED (bg_applier_get_preview_widget (appliers[BACKGROUND_TYPE_NO_PICTURE]))) {
		return;
	}

	/* BACKGROUND_TYPE_NO_PICTURE */
	tmp_prefs->wallpaper_enabled = FALSE;
	bg_applier_apply_prefs (appliers[BACKGROUND_TYPE_NO_PICTURE], tmp_prefs);
	tmp_prefs->wallpaper_enabled = TRUE;

	/* BACKGROUND_TYPE_WALLPAPER */
	tmp_prefs->wallpaper_type = WPTYPE_TILED;
	bg_applier_apply_prefs (appliers[BACKGROUND_TYPE_WALLPAPER], tmp_prefs);

	/* BACKGROUND_TYPE_CENTERED */
	tmp_prefs->wallpaper_type = WPTYPE_CENTERED;
	bg_applier_apply_prefs (appliers[BACKGROUND_TYPE_CENTERED], tmp_prefs);

	/* BACKGROUND_TYPE_SCALED */
	tmp_prefs->wallpaper_type = WPTYPE_SCALED;
	bg_applier_apply_prefs (appliers[BACKGROUND_TYPE_SCALED], tmp_prefs);

	/* BACKGROUND_TYPE_STRETCHED */
	tmp_prefs->wallpaper_type = WPTYPE_STRETCHED;
	bg_applier_apply_prefs (appliers[BACKGROUND_TYPE_STRETCHED], tmp_prefs);

	//bg_applier_apply_prefs (bg_root_applier, preferences);

	g_object_unref (G_OBJECT (tmp_prefs));
}

/* Retrieve legacy gnome_config settings and store them in the GConf
 * database. This involves some translation of the settings' meanings.
 */

static void
get_legacy_settings (void) 
{
	int          val_int;
	const char  *tmp_string;
	char        *val_string;
	gboolean     val_boolean;
	gboolean     def;
	gchar       *val_filename;

	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_bool (client, BG_PREFERENCES_DRAW_BACKGROUND,
			       gnome_config_get_bool ("/Background/Default/Enabled=true"), NULL);

	val_filename = gnome_config_get_string ("/Background/Default/wallpaper=(none)");
	
	if (val_filename != NULL && strcmp (val_filename, "(none)"))
		gconf_client_set_string (client, BG_PREFERENCES_PICTURE_OPTIONS, "none", NULL);
	else if (val_filename != NULL)
		gconf_client_set_string (client, BG_PREFERENCES_PICTURE_FILENAME,
					 val_filename, NULL);
	g_free (val_filename);

	val_int = gnome_config_get_int ("/Background/Default/wallpaperAlign=0");
	gconf_client_set_string (client, BG_PREFERENCES_PICTURE_OPTIONS,
				 background_type_to_string (val_int), NULL);

	gconf_client_set_string (client, BG_PREFERENCES_PRIMARY_COLOR,
				 gnome_config_get_string ("/Background/Default/color1"), NULL);
	gconf_client_set_string (client, BG_PREFERENCES_SECONDARY_COLOR,
				 gnome_config_get_string ("/Background/Default/color2"), NULL);

	/* Code to deal with new enum - messy */
	tmp_string = NULL;
	val_string = gnome_config_get_string_with_default ("/Background/Default/simple=solid", &def);
	if (!def) {
		if (!strcmp (val_string, "solid")) {
			tmp_string = "solid";
		} else {
			g_free (val_string);
			val_string = gnome_config_get_string_with_default ("/Background/Default/gradient=vertical", &def);
			if (!def)
				tmp_string = (!strcmp (val_string, "vertical")) ? "vertical-gradient" : "horizontal-gradient";
		}
	}

	g_free (val_string);

	if (tmp_string != NULL)
		gconf_client_set_string (client, BG_PREFERENCES_COLOR_SHADING_TYPE, tmp_string, NULL);

	val_boolean = gnome_config_get_bool_with_default ("/Background/Default/adjustOpacity=true", &def);

	if (!def && val_boolean)
		gconf_client_set_int (client, BG_PREFERENCES_PICTURE_OPACITY,
				      gnome_config_get_int ("/Background/Default/opacity=100"), NULL);
}

/* Initial apply to the preview, and setting of the color frame's sensitivity.
 *
 * We use a double-delay mechanism: first waiting 100 ms, then working in an
 * idle handler so that the preview gets rendered after the rest of the dialog
 * is displayed. This prevents the program from appearing to stall on startup,
 * making it feel more natural to the user.
 */

#if 0
static gboolean
real_realize_cb (BGPreferences *prefs) 
{
	GtkWidget *color_frame;
	BGApplier *bg_applier;

	g_return_val_if_fail (prefs != NULL, TRUE);
	g_return_val_if_fail (IS_BG_PREFERENCES (prefs), TRUE);

	if (G_OBJECT (prefs)->ref_count == 0)
		return FALSE;

	bg_applier = g_object_get_data (G_OBJECT (prefs), "applier");
	color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");

	bg_applier_apply_prefs (bg_applier, prefs);

	gtk_widget_set_sensitive (color_frame, bg_applier_render_color_p (bg_applier, prefs));

	return FALSE;
}
#endif

#if 0
static gboolean
realize_2_cb (BGPreferences *prefs) 
{
	gtk_idle_add ((GtkFunction) real_realize_cb, prefs);
	return FALSE;
}
#endif

#if 0
static void
realize_cb (GtkWidget *widget, BGPreferences *prefs)
{
	gtk_timeout_add (100, (GtkFunction) realize_2_cb, prefs);
}
#endif

static void
setup_color_widgets (int orientation)
{
	gboolean two_colors = TRUE;
	char *color1_string = NULL; 
	char *color2_string = NULL;

	GtkWidget *color1_label;
	GtkWidget *color2_label;
	GtkWidget *color2_box;

	switch (orientation) {
	case 0: /* solid */ 
		color1_string = "Color"; 
		two_colors = FALSE; 
		break;
	case 1: /* horiz */ 
		color1_string = "Left Color"; 
		color2_string = "Right Color"; 
		break;
	case 2: /* vert  */
		color1_string = "Top Color";
		color2_string = "Bottom Color"; 
		break;
	default:
		break;
	}


	color1_label = glade_xml_get_widget (dialog, "color1_label");
	color2_label = glade_xml_get_widget (dialog, "color2_label");
	color2_box = glade_xml_get_widget (dialog, "color2_box");

	g_assert (color1_label);
	gtk_label_set_text (GTK_LABEL(color1_label), color1_string);

	if (two_colors) {
		gtk_widget_show (color2_box);

		g_assert (color2_label);
		gtk_label_set_text (GTK_LABEL(color2_label), color2_string);
	} else {
		gtk_widget_hide (color2_box);
	}
}

/* Callback issued when some value changes in a property editor. This merges the
 * value with the preferences object and applies the settings to the preview. It
 * also sets the sensitivity of the color frame depending on whether the base
 * colors are visible through the wallpaper (or whether the wallpaper is
 * enabled). This cannot be done with a guard as it depends on a much more
 * complex criterion than a simple boolean configuration property.
 */

static void
peditor_value_changed (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, BGPreferences *prefs) 
{
	bg_preferences_load (prefs);
	update_preview_widgets (prefs, appliers, BG_APPLIER (bg_root_applier));
	
	if (strncmp (key, BG_PREFERENCES_COLOR_SHADING_TYPE, sizeof (BG_PREFERENCES_COLOR_SHADING_TYPE)) == 0) {
		int orientation;

		orientation = string_to_orientation (strdup (gconf_value_get_string (value)));
		setup_color_widgets (orientation);
	}
}

static gboolean
set_background_image_preview (const char *filename)
{
	int i, length;
	gboolean found = FALSE;
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	int width, height;
	float aspect_ratio;
	char *message;
	GtkWidget *box;

	g_assert (background_image_label != NULL);
	g_assert (background_image_preview != NULL);

	if ((filename == NULL) || (!g_file_test (filename, G_FILE_TEST_EXISTS))) {
		gtk_label_set_text (GTK_LABEL (background_image_label), "No Picture");
		gtk_image_set_from_stock (GTK_IMAGE (background_image_preview), GTK_STOCK_MISSING_IMAGE,
					  GTK_ICON_SIZE_DIALOG);
		set_picture_is_present (FALSE);

		if (filename) {
			gconf_client_set_string (gconf_client, BG_PREFERENCES_PICTURE_OPTIONS, "none", NULL);
			message = g_strdup_printf (_("Couldn't find the file '%s'.\n\nPlease make "
						     "sure it exists and try again, " 
						     "or choose a different background picture."),
						   filename);
			
			box = gtk_message_dialog_new (NULL,
						      GTK_DIALOG_MODAL,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_OK,
						      message);
			gtk_dialog_run (GTK_DIALOG (box));
			gtk_widget_destroy (box);
			g_free (message);
		}
		return FALSE;
	} else {
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	}

	if (pixbuf == NULL) {
		gtk_label_set_text (GTK_LABEL (background_image_label), "No Picture");
		gtk_image_set_from_stock (GTK_IMAGE (background_image_preview), GTK_STOCK_MISSING_IMAGE,
					  GTK_ICON_SIZE_DIALOG);
		set_picture_is_present (FALSE);
		gconf_client_set_string (gconf_client, BG_PREFERENCES_PICTURE_OPTIONS, "none", NULL);
		message = g_strdup_printf (_("I don't know how to open the file '%s'.\n"
					     "Perhaps its "
					     "a kind of picture that is not yet supported.\n\n"
					     "Please select a different picture instead."),
					   filename);

		box = gtk_message_dialog_new (NULL,
					      GTK_DIALOG_MODAL,
					      GTK_MESSAGE_ERROR,
					      GTK_BUTTONS_OK,
					      message);
		gtk_dialog_run (GTK_DIALOG (box));
		gtk_widget_destroy (box);
		g_free (message);
		return FALSE;
	}

	set_picture_is_present (TRUE);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	aspect_ratio = ((float)width) / ((float)height);
  
	if (aspect_ratio > (4.0f / 3.0f)) {
		height = (int)((160.0f / (float)width) * (float)height);
		width = 160;
	} else {
		width = (int)((120.0f / (float)height) * (float)width);
		height = 120;
	}
  
	scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
	gtk_image_set_from_pixbuf (GTK_IMAGE (background_image_preview), scaled_pixbuf);
	g_object_unref (G_OBJECT (scaled_pixbuf));
	g_object_unref (G_OBJECT (pixbuf));

	length = strlen (filename);

	for (i=length; (i >= 0) && !found; i--) {
		found = (filename[i] == '/');
	}

	g_assert (found);

	gtk_label_set_text (GTK_LABEL (background_image_label), &(filename[i + 2]));

	return TRUE;
}

void file_selector_cb (GtkWidget *widget, gpointer user_data) {
	const char *filename;

	GtkFileSelection *selector = GTK_FILE_SELECTION (user_data);

	filename = gtk_file_selection_get_filename (selector);

	gtk_widget_destroy (GTK_WIDGET (selector));

	set_background_picture (filename);
}

static void
image_filename_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *file_selector;
	char *old_filename;

	old_filename = gconf_client_get_string (gconf_client, BG_PREFERENCES_PICTURE_FILENAME, NULL);

	/* Create the selector */
   
	file_selector = gtk_file_selection_new ("Select an image for the background");

	if (old_filename != NULL && old_filename != "") {
		printf ("setting a filename\n");
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(file_selector), old_filename);
	}

	g_signal_connect (G_OBJECT (file_selector), "destroy",
			  (GCallback) gtk_widget_destroyed,
			  &file_selector);


	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (file_selector)->ok_button),
			  "clicked", (GCallback ) file_selector_cb, file_selector);

	g_signal_connect_swapped (G_OBJECT (GTK_FILE_SELECTION (file_selector)->cancel_button),
				  "clicked", (GCallback ) gtk_widget_destroy, file_selector);
  
	/* Display that dialog */
  
	gtk_widget_show (file_selector);
}


static void
background_type_toggled (GtkWidget *widget, gpointer data)
{
	GtkWidget **toggle_array = (GtkWidget **)data;
	static gboolean lock = FALSE;
	int i;

	if (lock == TRUE) {
		return;
	} else {
		lock = TRUE;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) == FALSE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
		lock = FALSE;
		return;
	}

	for (i = 0; i < NUMBER_BACKGROUND_TYPES; i++) {
		if (widget != toggle_array[i]) {
			g_object_set (G_OBJECT (toggle_array[i]), "active", FALSE, NULL);
		} else {
			gconf_client_set_string (gconf_client, BG_PREFERENCES_PICTURE_OPTIONS, 
						 background_type_to_string (i), NULL);
		}
	}


	bg_preferences_load (BG_PREFERENCES (bg_preferences));

	lock = FALSE;
}

static void
change_background_type_toggles (BackgroundType background_type, GtkWidget **toggle_array)
{
	if (background_type > NUMBER_BACKGROUND_TYPES) {
		g_warning ("An unknown background type, %d, was set. Ignoring...", background_type);
		return;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_array[background_type]), TRUE);

	if (background_type == BACKGROUND_TYPE_NO_PICTURE) {
		gtk_label_set_text (GTK_LABEL (border_shading_label), "Fill the background with a ");
	} else {
		gtk_label_set_text (GTK_LABEL (border_shading_label), "Border the picture with a ");
	}
}

static void
background_type_changed (GConfClient *client, guint cnxn_id,
			 GConfEntry *entry, gpointer user_data)
{
	int background_type;
	GtkWidget **toggle_array = (GtkWidget **) user_data;

	background_type = string_to_background_type (strdup (gconf_value_get_string (gconf_entry_get_value (entry))));

	change_background_type_toggles (background_type, toggle_array);
}


static void
set_background_picture (const char *filename)
{
	gboolean image_is_valid;

	image_is_valid = set_background_image_preview (filename);

	gconf_client_set_string (gconf_client, BG_PREFERENCES_PICTURE_FILENAME, filename, NULL);

	bg_preferences_load (BG_PREFERENCES (bg_preferences));

	update_preview_widgets (BG_PREFERENCES (bg_preferences), appliers, BG_APPLIER (bg_root_applier));
}


/* Construct the dialog */

static GladeXML *
create_dialog (BGApplier **bg_appliers) 
{
	GtkWidget *widget;

	GObject *peditor;

	int i, background_type;
	char *filename;
	GladeXML  *dialog;
	gboolean image_is_valid;

	/* FIXME: What the hell is domain? */
	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/background-properties.glade", "background_preferences_widget", NULL);
	
	widget = WID ("background_preferences_widget");

	toggle_array[BACKGROUND_TYPE_NO_PICTURE]  =  glade_xml_get_widget (dialog, "no_picture_toggle");
	toggle_array[BACKGROUND_TYPE_WALLPAPER]   =  glade_xml_get_widget (dialog, "wallpaper_toggle");
	toggle_array[BACKGROUND_TYPE_CENTERED]    =  glade_xml_get_widget (dialog, "centered_toggle");
	toggle_array[BACKGROUND_TYPE_SCALED]      =  glade_xml_get_widget (dialog, "scaled_toggle");
	toggle_array[BACKGROUND_TYPE_STRETCHED]   =  glade_xml_get_widget (dialog, "stretched_toggle");
	
	for (i = 0; i < NUMBER_BACKGROUND_TYPES; i++) {
		g_signal_connect (G_OBJECT (toggle_array[i]), 
				  "clicked", (GCallback) background_type_toggled,
				  toggle_array);
	}

	border_shading_label = glade_xml_get_widget (dialog, "border_shading_label");

	background_type = string_to_background_type (gconf_client_get_string (gconf_client, 
									      BG_PREFERENCES_PICTURE_OPTIONS, 
									      NULL));

	change_background_type_toggles (background_type, toggle_array);

	gconf_client_notify_add (gconf_client, BG_PREFERENCES_PICTURE_OPTIONS, 
				 &background_type_changed, toggle_array, NULL, NULL);


	widget = glade_xml_get_widget (dialog, "color1");
	peditor = gconf_peditor_new_color
		(NULL, BG_PREFERENCES_PRIMARY_COLOR, widget, NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, bg_preferences);

	widget = glade_xml_get_widget (dialog, "color2");
	peditor = gconf_peditor_new_color
		(NULL, BG_PREFERENCES_SECONDARY_COLOR, widget, NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, bg_preferences);

	widget = glade_xml_get_widget (dialog, "border_shading");
	peditor = gconf_peditor_new_select_menu
		(NULL, BG_PREFERENCES_COLOR_SHADING_TYPE, widget, 
		 "conv-to-widget-cb", peditor_string_to_orientation,
		 "conv-from-widget-cb", peditor_orientation_to_string, NULL);

	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, bg_preferences);



	widget = glade_xml_get_widget (dialog, "no_picture_frame");
	gtk_box_pack_end (GTK_BOX (widget), bg_applier_get_preview_widget (bg_appliers [BACKGROUND_TYPE_NO_PICTURE]), TRUE, TRUE, 0);

	widget = glade_xml_get_widget (dialog, "wallpaper_frame");
	gtk_box_pack_end (GTK_BOX (widget), bg_applier_get_preview_widget (bg_appliers [BACKGROUND_TYPE_WALLPAPER]), TRUE, TRUE, 0);

	widget = glade_xml_get_widget (dialog, "centered_frame");
	gtk_box_pack_end (GTK_BOX (widget), bg_applier_get_preview_widget (bg_appliers [BACKGROUND_TYPE_CENTERED]), TRUE, TRUE, 0);

	widget = glade_xml_get_widget (dialog, "scaled_frame");
	gtk_box_pack_end (GTK_BOX (widget), bg_applier_get_preview_widget (bg_appliers [BACKGROUND_TYPE_SCALED]), TRUE, TRUE, 0);

	widget = glade_xml_get_widget (dialog, "stretched_frame");
	gtk_box_pack_end (GTK_BOX (widget), bg_applier_get_preview_widget (bg_appliers [BACKGROUND_TYPE_STRETCHED]), TRUE, TRUE, 0);

	widget = glade_xml_get_widget (dialog, "background_image_button");
	g_signal_connect (G_OBJECT (widget), "clicked", (GCallback) image_filename_clicked, NULL);

	background_image_preview = glade_xml_get_widget (dialog, "background_image_preview");

	background_image_label = glade_xml_get_widget (dialog, "image_filename_label");
	filename = gconf_client_get_string (gconf_client, BG_PREFERENCES_PICTURE_FILENAME, NULL);

	image_is_valid = set_background_image_preview (filename);

	return dialog;
}

static gboolean 
idle_draw (gpointer data)
{
	update_preview_widgets (BG_PREFERENCES (bg_preferences), appliers, BG_APPLIER (bg_root_applier));
	return FALSE;
}

static void
dialog_button_clicked_cb (GtkDialog *dialog, gint response_id, GConfChangeSet *changeset) 
{
	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	}
}

int
main (int argc, char **argv) 
{
	GConfChangeSet *changeset;
	GtkWidget *dialog_win;
	GdkPixbuf *pixbuf;

	int orientation;
	int i;
	static gboolean get_legacy;

	static struct poptOption cap_options[] = {
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	setup_session_mgmt (argv[0]);

	gconf_client = gconf_client_get_default ();
	gconf_client_add_dir (gconf_client, "/desktop/gnome/background", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();

		for (i=0; i < NUMBER_BACKGROUND_TYPES; i++) {
			appliers[i] = BG_APPLIER (bg_applier_new (BG_APPLIER_PREVIEW));
		}

		/* setup a background preferences object */
		bg_preferences = bg_preferences_new ();
		bg_preferences_load (BG_PREFERENCES (bg_preferences));

		/* setup a background applier for the root window */
		bg_root_applier = bg_applier_new (BG_APPLIER_ROOT);

		dialog = create_dialog (appliers);

		dialog_win = gtk_dialog_new_with_buttons
			(_("Background Properties"), NULL, -1,
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			 NULL);
		gtk_window_set_modal (GTK_WINDOW(dialog_win), FALSE);
		pixbuf = gdk_pixbuf_new_from_file (GNOMECC_DATA_DIR "/icons/background-capplet.png", NULL);
		gtk_window_set_icon (GTK_WINDOW(dialog_win), pixbuf);
		
		g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback) dialog_button_clicked_cb, changeset);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), 
				    WID ("background_preferences_widget"), TRUE, 
				    TRUE, GNOME_PAD_SMALL);

		gtk_drag_dest_set (dialog_win, GTK_DEST_DEFAULT_ALL,
				   drop_types, n_drop_types,
				   GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
		g_signal_connect (G_OBJECT (dialog_win), "drag_motion",
				  G_CALLBACK (drag_motion_cb), NULL);
		g_signal_connect (G_OBJECT (dialog_win), "drag_leave",
				  G_CALLBACK (drag_leave_cb), NULL);
		g_signal_connect (G_OBJECT (dialog_win), "drag_data_received",
				  G_CALLBACK (drag_data_received_cb),
				  NULL);

		gtk_widget_show_all (dialog_win);

		orientation = string_to_orientation (gconf_client_get_string (gconf_client, BG_PREFERENCES_COLOR_SHADING_TYPE, NULL));
		setup_color_widgets (orientation);
		
		gtk_idle_add (idle_draw, NULL);

		/* Make sure the preferences object gets destroyed when the dialog is
		   closed */
		g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, bg_preferences);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
