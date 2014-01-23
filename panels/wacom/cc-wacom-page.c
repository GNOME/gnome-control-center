/*
 * Copyright © 2011 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#ifdef FAKE_AREA
#include <gdk/gdk.h>
#endif /* FAKE_AREA */

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "cc-wacom-button-row.h"
#include "cc-wacom-page.h"
#include "cc-wacom-nav-button.h"
#include "cc-wacom-mapping-panel.h"
#include "cc-wacom-stylus-page.h"
#include "gsd-enums.h"
#include "calibrator-gui.h"

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)
#define CWID(x) (GtkContainer *) gtk_builder_get_object (priv->builder, x)
#define MWID(x) (GtkWidget *) gtk_builder_get_object (priv->mapping_builder, x)

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

#define WACOM_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PAGE, CcWacomPagePrivate))

#define THRESHOLD_MISCLICK	15
#define THRESHOLD_DOUBLECLICK	7

enum {
	MAPPING_DESCRIPTION_COLUMN,
	MAPPING_TYPE_COLUMN,
	MAPPING_BUTTON_COLUMN,
	MAPPING_BUTTON_DIRECTION,
	MAPPING_N_COLUMNS
};

struct _CcWacomPagePrivate
{
	CcWacomPanel   *panel;
	GsdWacomDevice *stylus, *eraser, *pad;
	GtkBuilder     *builder;
	GtkWidget      *nav;
	GtkWidget      *notebook;
	CalibArea      *area;
	GSettings      *wacom_settings;

	/* Button mapping */
	GtkBuilder     *mapping_builder;
	GtkWidget      *button_map;
	GtkListStore   *action_store;

	/* Display mapping */
	GtkWidget      *mapping;
	GtkWidget      *dialog;

	GCancellable   *cancellable;
};

/* Button combo box storage columns */
enum {
	BUTTONNUMBER_COLUMN,
	BUTTONNAME_COLUMN,
	N_BUTTONCOLUMNS
};

/* Tablet mode combo box storage columns */
enum {
	MODENUMBER_COLUMN,
	MODELABEL_COLUMN,
	N_MODECOLUMNS
};

/* Tablet mode options - keep in sync with .ui */
enum {
	MODE_ABSOLUTE, /* stylus + eraser absolute */
	MODE_RELATIVE, /* stylus + eraser relative */
};

/* Different types of layout for the tablet config */
enum {
	LAYOUT_NORMAL,        /* tracking mode, button mapping */
	LAYOUT_REVERSIBLE,    /* tracking mode, button mapping, left-hand orientation */
	LAYOUT_SCREEN        /* button mapping, calibration, display resolution */
};

static void
update_tablet_ui (CcWacomPage *page,
		  int          layout);

static int
get_layout_type (GsdWacomDevice *device)
{
	int layout;

	if (gsd_wacom_device_is_screen_tablet (device))
		layout = LAYOUT_SCREEN;
	else if (gsd_wacom_device_reversible (device))
		layout = LAYOUT_REVERSIBLE;
	else
		layout = LAYOUT_NORMAL;

	return layout;
}

static void
set_calibration (GsdWacomDevice *device,
                 const gint      display_width,
                 const gint      display_height,
                 gint           *cal,
                 gsize           ncal,
                 GSettings      *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	GVariant    *last_resolution;
	GVariant   **tmp;
	gsize        nvalues;
	gint         i;

	current = g_settings_get_value (settings, "area");
	g_variant_get_fixed_array (current, &nvalues, sizeof (gint32));
	if ((ncal != 4) || (nvalues != 4)) {
		g_warning("Unable set set device calibration property. Got %"G_GSIZE_FORMAT" items to put in %"G_GSIZE_FORMAT" slots; expected %d items.\n", ncal, nvalues, 4);
		return;
	}

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_int32 (cal[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, tmp, nvalues);
	g_settings_set_value (settings, "area", array);

	g_free (tmp);

	g_debug ("Setting area top (%d, %d) bottom (%d, %d) (last used resolution: %d x %d)",
		 cal[0], cal[1], cal[2], cal[3],
		 display_width, display_height);

	/* set the last-calibration-resolution */
	last_resolution = g_variant_new ("(ii)", display_width, display_height);
	settings = gsd_wacom_device_get_settings (device);
	g_settings_set_value (settings,
			      "last-calibrated-resolution",
			      last_resolution);
}

static void
finish_calibration (CalibArea *area,
		    gpointer   user_data)
{
	CcWacomPage *page = (CcWacomPage *) user_data;
	CcWacomPagePrivate *priv = page->priv;
	XYinfo axis;
	gboolean swap_xy;
	gint cal[4], display_width, display_height;

	if (calib_area_finish (area, &axis, &swap_xy)) {
		cal[0] = axis.x_min;
		cal[1] = axis.y_min;
		cal[2] = axis.x_max;
		cal[3] = axis.y_max;

		calib_area_get_display_size (area, &display_width, &display_height);

		set_calibration (page->priv->stylus,
				 display_width,
				 display_height,
				 cal, 4, priv->wacom_settings);
	} else {
		/* Reset the old values */
		GVariant *old_calibration;

		old_calibration = g_object_get_data (G_OBJECT (page), "old-calibration");
		g_settings_set_value (page->priv->wacom_settings, "area", old_calibration);
		g_object_set_data (G_OBJECT (page), "old-calibration", NULL);
	}

	calib_area_free (area);
	priv->area = NULL;
	gtk_widget_set_sensitive (WID ("button-calibrate"), TRUE);
}

static gboolean
run_calibration (CcWacomPage *page,
		 GVariant    *old_calibration,
		 gint        *cal,
		 gint         monitor)
{
	XYinfo              old_axis;
	GdkDevice          *gdk_device;
	CcWacomPagePrivate *priv;
	int                 device_id;

	g_assert (page->priv->area == NULL);

	old_axis.x_min = cal[0];
	old_axis.y_min = cal[1];
	old_axis.x_max = cal[2];
	old_axis.y_max = cal[3];

	priv = page->priv;
	g_object_get (priv->stylus, "gdk-device", &gdk_device, NULL);

	if (gdk_device != NULL)
		g_object_get (gdk_device, "device-id", &device_id, NULL);
	else
		device_id = -1;

	priv->area = calib_area_new (NULL,
				     monitor,
				     device_id,
				     finish_calibration,
				     page,
				     &old_axis,
				     THRESHOLD_MISCLICK,
				     THRESHOLD_DOUBLECLICK);

	g_object_set_data_full (G_OBJECT (page),
				"old-calibration",
				old_calibration,
				(GDestroyNotify) g_variant_unref);

	return FALSE;
}

static void
calibrate (CcWacomPage *page)
{
	CcWacomPagePrivate *priv;
	int i, *calibration;
	GVariant *old_calibration, **tmp, *array;
	gsize ncal;
	gint monitor;
#ifdef FAKE_AREA
	GdkScreen *screen;
#endif

	priv = page->priv;

	monitor = gsd_wacom_device_get_display_monitor (page->priv->stylus);
	if (monitor < 0) {
		/* The display the tablet should be mapped to could not be located.
		 * This shouldn't happen if the EDID data is good...
		 */
		g_critical("Output associated with the tablet is not connected. Unable to calibrate.");
		return;
	}

	old_calibration = g_settings_get_value (page->priv->wacom_settings, "area");
	g_variant_get_fixed_array (old_calibration, &ncal, sizeof (gint32));

	if (ncal != 4) {
		g_warning("Device calibration property has wrong length. Got %"G_GSIZE_FORMAT" items; expected %d.\n", ncal, 4);
		return;
	}

#ifdef FAKE_AREA
	/* Prepare the monitor attachment */
	screen = gdk_screen_get_default ();

	calibration = g_new0 (int, 4);
	calibration[0] = 0;
	calibration[1] = gdk_screen_get_width (screen);
	calibration[2] = 0;
	calibration[3] = gdk_screen_get_height (screen);
#else
	calibration = gsd_wacom_device_get_default_area (priv->stylus);
#endif /* FAKE_AREA */

	/* Reset the current values, to avoid old calibrations
	 * from interfering with the calibration */
	tmp = g_malloc (ncal * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_int32 (calibration[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, tmp, 4);
	g_settings_set_value (page->priv->wacom_settings, "area", array);
	g_free (tmp);

	run_calibration (page, old_calibration, calibration, monitor);
	g_free (calibration);
	gtk_widget_set_sensitive (WID ("button-calibrate"), FALSE);
}

static void
calibrate_button_clicked_cb (GtkButton   *button,
			     CcWacomPage *page)
{
	calibrate (page);
}

/* This avoids us crashing when a newer version of
 * gnome-control-center has been used, and we load up an
 * old one, as the action type if unknown to the old g-c-c */
static gboolean
action_type_is_valid (GsdWacomActionType type)
{
	if (type >= G_N_ELEMENTS(action_table))
		return FALSE;
	return TRUE;
}

static void
create_row_from_button (GtkWidget            *list_box,
			GsdWacomTabletButton *button,
			GtkDirectionType      dir)
{
	GtkWidget *row;

	row = cc_wacom_button_row_new (button, dir);
	gtk_container_add (GTK_CONTAINER (list_box), row);
	gtk_widget_show (row);
}

static void
setup_button_mapping (CcWacomPage *page)
{
	CcWacomPagePrivate *priv;
	GList              *list, *l;
	GtkWidget          *list_box = NULL;

	priv = page->priv;
	list_box = MWID ("shortcuts_list");
	list = gsd_wacom_device_get_buttons (priv->pad);

	for (l = list; l != NULL; l = l->next) {
		GsdWacomTabletButton *button = l->data;
		GsdWacomActionType    action_type;

		if (button->type == WACOM_TABLET_BUTTON_TYPE_HARDCODED)
			continue;

		action_type = g_settings_get_enum (button->settings, "action-type");
		if (!action_type_is_valid (action_type))
			continue;

		if (button->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
		    button->type == WACOM_TABLET_BUTTON_TYPE_RING) {
			create_row_from_button (list_box, button, GTK_DIR_UP);
			create_row_from_button (list_box, button, GTK_DIR_DOWN);
			continue;
		}

		create_row_from_button (list_box, button, 0);
	}
	g_list_free (list);
}

static void
button_mapping_dialog_closed (GtkDialog   *dialog,
			      int          response_id,
			      CcWacomPage *page)
{
	CcWacomPagePrivate *priv;

	priv = page->priv;
	gtk_widget_destroy (MWID ("button-mapping-dialog"));
	g_object_unref (priv->mapping_builder);
	priv->mapping_builder = NULL;
}

static void
show_button_mapping_dialog (CcWacomPage *page)
{
	GtkWidget          *toplevel;
	GError             *error = NULL;
	GtkWidget          *dialog;
	CcWacomPagePrivate *priv;

	priv = page->priv;

	g_assert (priv->mapping_builder == NULL);
	priv->mapping_builder = gtk_builder_new ();
	gtk_builder_add_from_resource (priv->mapping_builder,
                                       "/org/gnome/control-center/wacom/button-mapping.ui",
                                       &error);

	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->mapping_builder);
		priv->mapping_builder = NULL;
		g_error_free (error);
		return;
	}

	setup_button_mapping (page);

	dialog = MWID ("button-mapping-dialog");
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (button_mapping_dialog_closed), page);

	gtk_widget_show (dialog);

	priv->button_map = dialog;
	g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &priv->button_map);
}

static void
set_osd_visibility_cb (GObject      *source_object,
		       GAsyncResult *res,
		       gpointer      data)
{
	GError      *error = NULL;
	GVariant    *result;
	CcWacomPage *page;

	page = CC_WACOM_PAGE (data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

	if (result == NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_printerr ("Error setting OSD's visibility: %s\n", error->message);
			g_error_free (error);
			show_button_mapping_dialog (page);
		} else {
			g_error_free (error);
			return;
		}
	}
}

static void
set_osd_visibility (CcWacomPage *page,
		    guint32      device_id)
{
	CcWacomPagePrivate *priv;
	GDBusProxy         *proxy;

	priv = page->priv;
	proxy = cc_wacom_panel_get_gsd_wacom_bus_proxy (priv->panel);

	if (proxy == NULL) {
		show_button_mapping_dialog (page);
		return;
	}

	g_dbus_proxy_call (proxy,
			   "SetOSDVisibility",
			   g_variant_new ("(ubb)", device_id, TRUE, TRUE),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   priv->cancellable,
			   set_osd_visibility_cb,
			   page);
}

static void
map_buttons_button_clicked_cb (GtkButton   *button,
			       CcWacomPage *page)
{
	CcWacomPagePrivate *priv;
	GdkDevice *gdk_device = NULL;
	guint32 device_id;
        const gchar *layout_path;

	priv = page->priv;

	g_object_get (priv->pad, "gdk-device", &gdk_device, NULL);

	g_return_if_fail (gdk_device != NULL);

	g_object_get (gdk_device, "device-id", &device_id, NULL);

	/* Check if the OSD should be shown instead of the button mapping dialod */
        layout_path = gsd_wacom_device_get_layout_path (page->priv->pad);
        if (layout_path && g_file_test (layout_path, G_FILE_TEST_EXISTS)) {
		set_osd_visibility (page, device_id);
		return;
	}

	g_message ("Couldn't find a layout for '%s'. Launching the button mapping dialog.", gsd_wacom_device_get_name (priv->pad));
	show_button_mapping_dialog (page);
}

static void
display_mapping_dialog_closed (GtkDialog   *dialog,
			       int          response_id,
			       CcWacomPage *page)
{
	CcWacomPagePrivate *priv;
	int layout;

	priv = page->priv;
	gtk_widget_destroy (priv->dialog);
	priv->dialog = NULL;
	priv->mapping = NULL;
	layout = get_layout_type (priv->stylus);
	update_tablet_ui (page, layout);
}

static void
display_mapping_button_clicked_cb (GtkButton   *button,
				   CcWacomPage *page)
{
	CcWacomPagePrivate *priv;

	priv = page->priv;

	g_assert (priv->mapping == NULL);

	priv->dialog = gtk_dialog_new_with_buttons (_("Display Mapping"),
						    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    _("_Close"),
						    GTK_RESPONSE_ACCEPT,
						    NULL);
	priv->mapping = cc_wacom_mapping_panel_new ();
	cc_wacom_mapping_panel_set_device (CC_WACOM_MAPPING_PANEL (priv->mapping),
					   priv->stylus);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (priv->dialog))),
			   priv->mapping);
	g_signal_connect (G_OBJECT (priv->dialog), "response",
			  G_CALLBACK (display_mapping_dialog_closed), page);
	gtk_widget_show_all (priv->dialog);

	g_object_add_weak_pointer (G_OBJECT (priv->mapping), (gpointer *) &priv->dialog);
}

static void
tabletmode_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPagePrivate	*priv	= CC_WACOM_PAGE(user_data)->priv;
	GtkListStore		*liststore;
	GtkTreeIter		iter;
	gint			mode;
	gboolean		is_absolute;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-tabletmode"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    MODENUMBER_COLUMN, &mode,
			    -1);

	is_absolute = (mode == MODE_ABSOLUTE);
	g_settings_set_boolean (priv->wacom_settings, "is-absolute", is_absolute);
}

static const gchar*
opposite_rotation (const gchar *rotation)
{
	/* Order matters here, if not found we return "none"  */
	static const gchar *rotations[] = { "half", "cw", "none", "ccw" };
	guint i, n;

	n = G_N_ELEMENTS (rotations);
	for (i = 0; i < n; i++) {
		if (strcmp (rotation, rotations[i]) == 0)
			break;
	}

	return rotations[(i + n / 2) % n];
}

static void
left_handed_toggled_cb (GtkSwitch *sw, GParamSpec *pspec, gpointer *user_data)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(user_data)->priv;
	GsdWacomDevice          *device = priv->stylus;
	GsdWacomRotation 	display_rotation;
	const gchar*		rotation;

	display_rotation = gsd_wacom_device_get_display_rotation (device);
	rotation = gsd_wacom_device_rotation_type_to_name (display_rotation);
	if (gtk_switch_get_active (sw))
		rotation = opposite_rotation (rotation);

	g_settings_set_string (priv->wacom_settings, "rotation", rotation);
}

static void
set_left_handed_from_gsettings (CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(page)->priv;
	GsdWacomDevice          *device = priv->stylus;
	GsdWacomRotation 	display_rotation;
	const gchar*		rotation;

	display_rotation = gsd_wacom_device_get_display_rotation (device);
	rotation = g_settings_get_string (priv->wacom_settings, "rotation");
	if (strcmp (rotation, gsd_wacom_device_rotation_type_to_name (display_rotation)) != 0)
		gtk_switch_set_active (GTK_SWITCH (WID ("switch-left-handed")), TRUE);
}

static void
set_mode_from_gsettings (GtkComboBox *combo, CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = page->priv;
	gboolean		is_absolute;

	is_absolute = g_settings_get_boolean (priv->wacom_settings, "is-absolute");

	/* this must be kept in sync with the .ui file */
	gtk_combo_box_set_active (combo, is_absolute ? MODE_ABSOLUTE : MODE_RELATIVE);
}

static void
combobox_text_cellrenderer (GtkComboBox *combo, int name_column)
{
	GtkCellRenderer	*renderer;

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", BUTTONNAME_COLUMN, NULL);
}

static gboolean
display_clicked_cb (GtkButton   *button,
		    CcWacomPage *page)
{
	cc_wacom_panel_switch_to_panel (page->priv->panel, "display");
	return TRUE;
}

static gboolean
mouse_clicked_cb (GtkButton   *button,
		  CcWacomPage *page)
{
	cc_wacom_panel_switch_to_panel (page->priv->panel, "mouse");
	return TRUE;
}

/* Boilerplate code goes below */

static void
cc_wacom_page_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_page_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_page_dispose (GObject *object)
{
	CcWacomPagePrivate *priv = CC_WACOM_PAGE (object)->priv;

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_clear_object (&priv->cancellable);
	}

	if (priv->area) {
		calib_area_free (priv->area);
		priv->area = NULL;
	}

	if (priv->button_map) {
		gtk_widget_destroy (priv->button_map);
		priv->button_map = NULL;
	}

	if (priv->dialog) {
		gtk_widget_destroy (priv->dialog);
		priv->dialog = NULL;
	}

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	priv->panel = NULL;

	G_OBJECT_CLASS (cc_wacom_page_parent_class)->dispose (object);
}

static void
cc_wacom_page_class_init (CcWacomPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomPagePrivate));

	object_class->get_property = cc_wacom_page_get_property;
	object_class->set_property = cc_wacom_page_set_property;
	object_class->dispose = cc_wacom_page_dispose;
}

static void
cc_wacom_page_init (CcWacomPage *self)
{
	CcWacomPagePrivate *priv;
	GError *error = NULL;
	GtkComboBox *combo;
	GtkWidget *box;
	GtkSwitch *sw;
	char *objects[] = {
		"main-grid",
		"liststore-tabletmode",
		"liststore-buttons",
		"adjustment-tip-feel",
		"adjustment-eraser-feel",
		NULL
	};

	priv = self->priv = WACOM_PAGE_PRIVATE (self);

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_resource (priv->builder,
                                               "/org/gnome/control-center/wacom/gnome-wacom-properties.ui",
                                               objects,
                                               &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	box = WID ("main-grid");
	gtk_container_add (GTK_CONTAINER (self), box);
	gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);

	self->priv->notebook = WID ("stylus-notebook");

	g_signal_connect (WID ("button-calibrate"), "clicked",
			  G_CALLBACK (calibrate_button_clicked_cb), self);
	g_signal_connect (WID ("map-buttons-button"), "clicked",
			  G_CALLBACK (map_buttons_button_clicked_cb), self);

	combo = GTK_COMBO_BOX (WID ("combo-tabletmode"));
	combobox_text_cellrenderer (combo, MODELABEL_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (tabletmode_changed_cb), self);

	sw = GTK_SWITCH (WID ("switch-left-handed"));
	g_signal_connect (G_OBJECT (sw), "notify::active",
			  G_CALLBACK (left_handed_toggled_cb), self);

	g_signal_connect (G_OBJECT (WID ("display-link")), "activate-link",
			  G_CALLBACK (display_clicked_cb), self);

	g_signal_connect (G_OBJECT (WID ("mouse-link")), "activate-link",
			  G_CALLBACK (mouse_clicked_cb), self);

	g_signal_connect (G_OBJECT (WID ("display-mapping-button")), "clicked",
			  G_CALLBACK (display_mapping_button_clicked_cb), self);

	priv->nav = cc_wacom_nav_button_new ();
        gtk_widget_set_halign (priv->nav, GTK_ALIGN_END);
        gtk_widget_set_margin_start (priv->nav, 10);
	gtk_grid_attach (GTK_GRID (box), priv->nav, 1, 0, 1, 1);

	priv->cancellable = g_cancellable_new ();
}

static void
set_icon_name (CcWacomPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	CcWacomPagePrivate *priv;
	char *resource;

	priv = page->priv;

	resource = g_strdup_printf ("/org/gnome/control-center/wacom/%s.svg", icon_name);
	gtk_image_set_from_resource (GTK_IMAGE (WID (widget_name)), resource);
	g_free (resource);
}

typedef struct {
	GsdWacomStylus *stylus;
	GsdWacomStylus *eraser;
} StylusPair;

static void
add_styli (CcWacomPage *page)
{
	GList *styli, *l;
	CcWacomPagePrivate *priv;

	priv = page->priv;

	styli = gsd_wacom_device_list_styli (priv->stylus);

	for (l = styli; l; l = l->next) {
		GsdWacomStylus *stylus, *eraser;
		GtkWidget *page;

		stylus = l->data;

		if (gsd_wacom_stylus_get_stylus_type (stylus) == WACOM_STYLUS_TYPE_PUCK)
			continue;

		if (gsd_wacom_stylus_get_has_eraser (stylus)) {
			GsdWacomDeviceType type;
			type = gsd_wacom_stylus_get_stylus_type (stylus);
			eraser = gsd_wacom_device_get_stylus_for_type (priv->eraser, type);
		} else {
			eraser = NULL;
		}

		page = cc_wacom_stylus_page_new (stylus, eraser);
		cc_wacom_stylus_page_set_navigation (CC_WACOM_STYLUS_PAGE (page), GTK_NOTEBOOK (priv->notebook));
		gtk_widget_show (page);
		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), page, NULL);
	}
	g_list_free (styli);
}

static void
stylus_changed (GsdWacomDevice *device,
		GParamSpec     *pspec,
		CcWacomPage    *page)
{
	GsdWacomStylus *stylus;
	CcWacomPagePrivate *priv;
	int num_pages;
	guint i;

	priv = page->priv;
	g_object_get (G_OBJECT (device), "last-stylus", &stylus, NULL);
	if (stylus == NULL)
		return;

	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	for (i = 0; i < num_pages; i++) {
		GsdWacomStylus *s;
		CcWacomStylusPage *spage;

		spage = CC_WACOM_STYLUS_PAGE (gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), i));
		s = cc_wacom_stylus_page_get_stylus (spage);
		if (s == stylus) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), i);
			return;
		}
	}

	g_warning ("Failed to find the page for stylus '%s'",
		   gsd_wacom_stylus_get_name (stylus));
}

static void
remove_left_handed (CcWacomPagePrivate *priv)
{
	gtk_widget_destroy (WID ("label-left-handed"));
	gtk_widget_destroy (WID ("switch-left-handed"));
}

static void
remove_display_link (CcWacomPagePrivate *priv)
{
	gtk_widget_destroy (WID ("display-link"));

        gtk_container_child_set (CWID ("main-grid"),
                                 WID ("tablet-buttons-box"),
                                 "top_attach", 2, NULL);
}

static void
remove_mouse_link (CcWacomPagePrivate *priv)
{
        gtk_widget_destroy (WID ("mouse-link"));

        gtk_container_child_set (CWID ("main-grid"),
                                 WID ("tablet-buttons-box"),
                                 "top_attach", 2, NULL);
}

static gboolean
has_monitor (CcWacomPage *page)
{
	return gsd_wacom_device_get_display_monitor (page->priv->stylus) >= 0;
}

static void
update_tablet_ui (CcWacomPage *page,
		  int          layout)
{
	CcWacomPagePrivate *priv;
	GsdWacomStylus *puck;

	priv = page->priv;
	puck = gsd_wacom_device_get_stylus_for_type (priv->stylus, WACOM_STYLUS_TYPE_PUCK);
	if (puck == NULL)
		remove_mouse_link (priv);

	/* Hide the pad buttons if no pad is present */
	gtk_widget_set_visible (WID ("map-buttons-button"), priv->pad != NULL);

	switch (layout) {
	case LAYOUT_NORMAL:
		remove_left_handed (priv);
		remove_display_link (priv);
		break;
	case LAYOUT_REVERSIBLE:
		remove_display_link (priv);
		break;
	case LAYOUT_SCREEN:
		remove_left_handed (priv);

		gtk_widget_destroy (WID ("combo-tabletmode"));
		gtk_widget_destroy (WID ("label-trackingmode"));
		gtk_widget_destroy (WID ("display-mapping-button"));

		gtk_widget_show (WID ("button-calibrate"));
		gtk_widget_set_sensitive (WID ("button-calibrate"),
					  has_monitor (page));

		gtk_container_child_set (CWID ("main-grid"),
					 WID ("tablet-buttons-box"),
					 "top_attach", 1, NULL);
		gtk_container_child_set (CWID ("main-grid"),
					 WID ("display-link"),
					 "top_attach", 2, NULL);
		break;
	default:
		g_assert_not_reached ();
	}
}

gboolean
cc_wacom_page_update_tools (CcWacomPage    *page,
			    GsdWacomDevice *stylus,
			    GsdWacomDevice *eraser,
			    GsdWacomDevice *pad)
{
	CcWacomPagePrivate *priv;
	int layout;
	gboolean changed;

	/* Type of layout */
	layout = get_layout_type (stylus);

	priv = page->priv;
	changed = (priv->stylus != stylus || priv->eraser != eraser || priv->pad != pad);
	if (!changed)
		return FALSE;

	priv->stylus = stylus;
	priv->eraser = eraser;
	priv->pad = pad;

	update_tablet_ui (CC_WACOM_PAGE (page), layout);

	return TRUE;
}

GtkWidget *
cc_wacom_page_new (CcWacomPanel   *panel,
		   GsdWacomDevice *stylus,
		   GsdWacomDevice *eraser,
		   GsdWacomDevice *pad)
{
	CcWacomPage *page;
	CcWacomPagePrivate *priv;

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (stylus), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (stylus) == WACOM_TYPE_STYLUS, NULL);

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (eraser), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (eraser) == WACOM_TYPE_ERASER, NULL);

	if (pad != NULL)
		g_return_val_if_fail (gsd_wacom_device_get_device_type (pad) == WACOM_TYPE_PAD, NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	priv = page->priv;
	priv->panel = panel;

	cc_wacom_page_update_tools (page, stylus, eraser, pad);

	/* FIXME move this to construct */
	priv->wacom_settings  = gsd_wacom_device_get_settings (stylus);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), page);

	/* Tablet name */
	gtk_label_set_text (GTK_LABEL (WID ("label-tabletmodel")), gsd_wacom_device_get_name (stylus));

	/* Left-handedness */
	if (gsd_wacom_device_reversible (stylus))
		set_left_handed_from_gsettings (page);

	/* Tablet icon */
	set_icon_name (page, "image-tablet", gsd_wacom_device_get_icon_name (stylus));

	/* Add styli */
	add_styli (page);

	/* Get the current stylus and switch to its page */
	stylus_changed (priv->stylus, NULL, page);
	g_signal_connect (G_OBJECT (priv->stylus), "notify::last-stylus",
			  G_CALLBACK (stylus_changed), page);

	return GTK_WIDGET (page);
}

void
cc_wacom_page_set_navigation (CcWacomPage *page,
			      GtkNotebook *notebook,
			      gboolean     ignore_first_page)
{
	CcWacomPagePrivate *priv;

	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	priv = page->priv;

	g_object_set (G_OBJECT (priv->nav),
		      "notebook", notebook,
		      "ignore-first", ignore_first_page,
		      NULL);
}

void
cc_wacom_page_calibrate (CcWacomPage *page)
{
	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	calibrate (page);
}

gboolean
cc_wacom_page_can_calibrate (CcWacomPage *page)
{
	g_return_val_if_fail (CC_IS_WACOM_PAGE (page),
			      FALSE);

	return has_monitor (page);
}
