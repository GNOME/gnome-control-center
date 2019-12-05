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
#include <gdesktop-enums.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "cc-wacom-device.h"
#include "cc-wacom-button-row.h"
#include "cc-wacom-page.h"
#include "cc-wacom-nav-button.h"
#include "cc-wacom-mapping-panel.h"
#include "cc-wacom-stylus-page.h"
#include "gsd-enums.h"
#include "calibrator-gui.h"
#include "gsd-input-helper.h"

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (page->builder, x)
#define CWID(x) (GtkContainer *) gtk_builder_get_object (page->builder, x)
#define MWID(x) (GtkWidget *) gtk_builder_get_object (page->mapping_builder, x)

#define THRESHOLD_MISCLICK	15
#define THRESHOLD_DOUBLECLICK	7

enum {
	MAPPING_DESCRIPTION_COLUMN,
	MAPPING_TYPE_COLUMN,
	MAPPING_BUTTON_COLUMN,
	MAPPING_BUTTON_DIRECTION,
	MAPPING_N_COLUMNS
};

struct _CcWacomPage
{
	GtkBox          parent_instance;

	CcWacomPanel   *panel;
	CcWacomDevice  *stylus;
	CcWacomDevice  *pad;
	GtkBuilder     *builder;
	GtkWidget      *nav;
	GtkWidget      *notebook;
	CalibArea      *area;
	GSettings      *wacom_settings;

	GtkSizeGroup   *header_group;

	/* Button mapping */
	GtkBuilder     *mapping_builder;
	GtkWidget      *button_map;
	GtkListStore   *action_store;

	/* Display mapping */
	GtkWidget      *mapping;
	GtkWidget      *dialog;

	GCancellable   *cancellable;
};

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

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
get_layout_type (CcWacomDevice *device)
{
	int layout;

	if (cc_wacom_device_get_integration_flags (device) &
	    (WACOM_DEVICE_INTEGRATED_DISPLAY | WACOM_DEVICE_INTEGRATED_SYSTEM))
		layout = LAYOUT_SCREEN;
	else if (cc_wacom_device_is_reversible (device))
		layout = LAYOUT_REVERSIBLE;
	else
		layout = LAYOUT_NORMAL;

	return layout;
}

static void
set_calibration (CcWacomDevice  *device,
                 const gint      display_width,
                 const gint      display_height,
                 gdouble        *cal,
                 gsize           ncal,
                 GSettings      *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	g_autofree GVariant   **tmp = NULL;
	gsize        nvalues;
	gint         i;

	current = g_settings_get_value (settings, "area");
	g_variant_get_fixed_array (current, &nvalues, sizeof (gdouble));
	if ((ncal != 4) || (nvalues != 4)) {
		g_warning("Unable set set device calibration property. Got %"G_GSIZE_FORMAT" items to put in %"G_GSIZE_FORMAT" slots; expected %d items.\n", ncal, nvalues, 4);
		return;
	}

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_double (cal[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_DOUBLE, tmp, nvalues);
	g_settings_set_value (settings, "area", array);

	g_debug ("Setting area to %f, %f, %f, %f (left/right/top/bottom) (last used resolution: %d x %d)",
		 cal[0], cal[1], cal[2], cal[3],
		 display_width, display_height);
}

static void
finish_calibration (CalibArea *area,
		    gpointer   user_data)
{
	CcWacomPage *page = (CcWacomPage *) user_data;
	XYinfo axis;
	gdouble cal[4];
	gint display_width, display_height;

	if (calib_area_finish (area)) {
		calib_area_get_padding (area, &axis);
		cal[0] = axis.x_min;
		cal[1] = axis.x_max;
		cal[2] = axis.y_min;
		cal[3] = axis.y_max;

		calib_area_get_display_size (area, &display_width, &display_height);

		set_calibration (page->stylus,
				 display_width,
				 display_height,
				 cal, 4, page->wacom_settings);
	} else {
		/* Reset the old values */
		GVariant *old_calibration;

		old_calibration = g_object_get_data (G_OBJECT (page), "old-calibration");
		g_settings_set_value (page->wacom_settings, "area", old_calibration);
		g_object_set_data (G_OBJECT (page), "old-calibration", NULL);
	}

	calib_area_free (area);
	page->area = NULL;
	gtk_widget_set_sensitive (WID ("button-calibrate"), TRUE);
}

static GdkDevice *
cc_wacom_page_get_gdk_device (CcWacomPage *page)
{
	GsdDevice *gsd_device;
	GdkDevice *gdk_device = NULL;
	GdkDisplay *display;
	GdkSeat *seat;
	g_autoptr(GList) slaves = NULL;
	GList *l;

	gsd_device = cc_wacom_device_get_device (page->stylus);
	g_return_val_if_fail (GSD_IS_DEVICE (gsd_device), NULL);

	display = gtk_widget_get_display (GTK_WIDGET (page));
	seat = gdk_display_get_default_seat (display);
	slaves = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_TABLET_STYLUS);

	for (l = slaves; l && !gdk_device; l = l->next) {
		g_autofree gchar *device_node = NULL;

		if (gdk_device_get_source (l->data) != GDK_SOURCE_PEN)
			continue;

#ifdef GDK_WINDOWING_X11
		if (GDK_IS_X11_DISPLAY (display))
			device_node = xdevice_get_device_node (gdk_x11_device_get_id (l->data));
#endif
#ifdef GDK_WINDOWING_WAYLAND
		if (GDK_IS_WAYLAND_DISPLAY (display))
			device_node = g_strdup (gdk_wayland_device_get_node_path (l->data));
#endif

		if (g_strcmp0 (device_node, gsd_device_get_device_file (gsd_device)) == 0)
			gdk_device = l->data;
	}

	return gdk_device;
}

static gboolean
run_calibration (CcWacomPage *page,
		 GVariant    *old_calibration,
		 gdouble     *cal,
		 GdkMonitor  *monitor)
{
	GdkDisplay *display = gdk_monitor_get_display (monitor);
	gint i, n_monitor = 0;

	g_assert (page->area == NULL);

	for (i = 0; i < gdk_display_get_n_monitors (display); i++) {
		if (monitor == gdk_display_get_monitor (display, i)) {
			n_monitor = i;
			break;
		}
	}

	page->area = calib_area_new (NULL,
				     n_monitor,
				     cc_wacom_page_get_gdk_device (page),
				     finish_calibration,
				     page,
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
	int i;
	GVariant *old_calibration, *array;
	g_autofree GVariant **tmp = NULL;
	g_autofree gdouble *calibration = NULL;
	gsize ncal;
	GdkMonitor *monitor;
	GdkScreen *screen;
	g_autoptr(GnomeRRScreen) rr_screen = NULL;
	GnomeRROutput *output;
	g_autoptr(GError) error = NULL;
	gint x, y;

	screen = gdk_screen_get_default ();
	rr_screen = gnome_rr_screen_new (screen, &error);
	if (error) {
		g_warning ("Could not connect to display manager: %s", error->message);
		return;
	}

	output = cc_wacom_device_get_output (page->stylus, rr_screen);
	gnome_rr_output_get_position (output, &x, &y);
	monitor = gdk_display_get_monitor_at_point (gdk_screen_get_display (screen), x, y);

	if (!monitor) {
		/* The display the tablet should be mapped to could not be located.
		 * This shouldn't happen if the EDID data is good...
		 */
		g_critical("Output associated with the tablet is not connected. Unable to calibrate.");
		return;
	}

	old_calibration = g_settings_get_value (page->wacom_settings, "area");
	g_variant_get_fixed_array (old_calibration, &ncal, sizeof (gdouble));

	if (ncal != 4) {
		g_warning("Device calibration property has wrong length. Got %"G_GSIZE_FORMAT" items; expected %d.\n", ncal, 4);
		return;
	}

	calibration = g_new0 (gdouble, ncal);

	/* Reset the current values, to avoid old calibrations
	 * from interfering with the calibration */
	tmp = g_malloc (ncal * sizeof (GVariant*));
	for (i = 0; i < ncal; i++) {
		calibration[i] = 0.0;
		tmp[i] = g_variant_new_double (calibration[i]);
	}

	array = g_variant_new_array (G_VARIANT_TYPE_DOUBLE, tmp, ncal);
	g_settings_set_value (page->wacom_settings, "area", array);

	run_calibration (page, old_calibration, calibration, monitor);
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
action_type_is_valid (GDesktopPadButtonAction action)
{
	if (action >= G_N_ELEMENTS (action_table))
		return FALSE;
	return TRUE;
}

static void
create_row_from_button (GtkWidget *list_box,
			guint      button,
			GSettings *settings)
{
	GtkWidget *row;

	row = cc_wacom_button_row_new (button, settings);
	gtk_container_add (GTK_CONTAINER (list_box), row);
	gtk_widget_show (row);
}

static void
setup_button_mapping (CcWacomPage *page)
{
	GDesktopPadButtonAction action;
	GtkWidget *list_box;
	guint i, n_buttons;
	GSettings *settings;

	list_box = MWID ("shortcuts_list");
	n_buttons = cc_wacom_device_get_num_buttons (page->pad);

	for (i = 0; i < n_buttons; i++) {
		settings = cc_wacom_device_get_button_settings (page->pad, i);
		if (!settings)
			continue;

		action = g_settings_get_enum (settings, "action");
		if (!action_type_is_valid (action))
			continue;

		create_row_from_button (list_box, i, settings);
	}
}

static void
button_mapping_dialog_closed (GtkDialog   *dialog,
			      int          response_id,
			      CcWacomPage *page)
{
	gtk_widget_destroy (MWID ("button-mapping-dialog"));
	g_clear_object (&page->mapping_builder);
}

static void
show_button_mapping_dialog (CcWacomPage *page)
{
	GtkWidget          *toplevel;
	g_autoptr(GError)   error = NULL;
	GtkWidget          *dialog;

	g_assert (page->mapping_builder == NULL);
	page->mapping_builder = gtk_builder_new ();
	gtk_builder_add_from_resource (page->mapping_builder,
                                       "/org/gnome/control-center/wacom/button-mapping.ui",
                                       &error);

	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_clear_object (&page->mapping_builder);
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

	page->button_map = dialog;
	g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &page->button_map);
}

static void
set_osd_visibility_cb (GObject      *source_object,
		       GAsyncResult *res,
		       gpointer      data)
{
	g_autoptr(GError) error = NULL;
	GVariant    *result;
	CcWacomPage *page;

	page = CC_WACOM_PAGE (data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

	if (result == NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_printerr ("Error setting OSD's visibility: %s\n", error->message);
			show_button_mapping_dialog (page);
		} else {
			return;
		}
	}
}

static void
set_osd_visibility (CcWacomPage *page)
{
	GDBusProxy         *proxy;
	GsdDevice          *gsd_device;
	const gchar        *device_path;

	proxy = cc_wacom_panel_get_gsd_wacom_bus_proxy (page->panel);
	gsd_device = cc_wacom_device_get_device (page->pad);

	device_path = gsd_device_get_device_file (gsd_device);

	if (proxy == NULL) {
		show_button_mapping_dialog (page);
		return;
	}

	g_dbus_proxy_call (proxy,
			   "Show",
			   g_variant_new ("(ob)", device_path, TRUE),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   page->cancellable,
			   set_osd_visibility_cb,
			   page);
}

static void
map_buttons_button_clicked_cb (GtkButton   *button,
			       CcWacomPage *page)
{
	set_osd_visibility (page);
}

static void
display_mapping_dialog_closed (GtkDialog   *dialog,
			       int          response_id,
			       CcWacomPage *page)
{
	int layout;

	gtk_widget_destroy (page->dialog);
	page->dialog = NULL;
	page->mapping = NULL;
	layout = get_layout_type (page->stylus);
	update_tablet_ui (page, layout);
}

static void
display_mapping_button_clicked_cb (GtkButton   *button,
				   CcWacomPage *page)
{
	g_assert (page->mapping == NULL);

	page->dialog = gtk_dialog_new_with_buttons (_("Display Mapping"),
						    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    _("_Close"),
						    GTK_RESPONSE_ACCEPT,
						    NULL);
	page->mapping = cc_wacom_mapping_panel_new ();
	cc_wacom_mapping_panel_set_device (CC_WACOM_MAPPING_PANEL (page->mapping),
					   page->stylus);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (page->dialog))),
			   page->mapping);
	g_signal_connect (G_OBJECT (page->dialog), "response",
			  G_CALLBACK (display_mapping_dialog_closed), page);
	gtk_widget_show_all (page->dialog);

	g_object_add_weak_pointer (G_OBJECT (page->mapping), (gpointer *) &page->dialog);
}

static void
tabletmode_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPage *page = CC_WACOM_PAGE (user_data);
	GtkListStore *liststore;
	GtkTreeIter iter;
	gint mode;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-tabletmode"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    MODENUMBER_COLUMN, &mode,
			    -1);

	g_settings_set_enum (page->wacom_settings, "mapping", mode);
}

static void
left_handed_toggled_cb (GtkSwitch *sw, GParamSpec *pspec, gpointer *user_data)
{
	CcWacomPage *page = CC_WACOM_PAGE (user_data);
	gboolean left_handed;

	left_handed = gtk_switch_get_active (sw);
	g_settings_set_boolean (page->wacom_settings, "left-handed", left_handed);
}

static void
set_left_handed_from_gsettings (CcWacomPage *page)
{
	gboolean left_handed;

	left_handed = g_settings_get_boolean (page->wacom_settings, "left-handed");
	gtk_switch_set_active (GTK_SWITCH (WID ("switch-left-handed")), left_handed);
}

static void
set_mode_from_gsettings (GtkComboBox *combo,
			 CcWacomPage *page)
{
	GDesktopTabletMapping mapping;

	mapping = g_settings_get_enum (page->wacom_settings, "mapping");

	/* this must be kept in sync with the .ui file */
	gtk_combo_box_set_active (combo, mapping);
}

static void
update_display_decoupled_sensitivity (CcWacomPage *page,
				      gboolean	   active)
{
	if (get_layout_type (page->stylus) != LAYOUT_SCREEN)
		return;

	gtk_widget_set_sensitive (WID ("label-trackingmode"), active);
	gtk_widget_set_sensitive (WID ("combo-tabletmode"), active);
	gtk_widget_set_sensitive (WID ("display-mapping-button-2"), active);

	gtk_widget_set_sensitive (WID ("button-calibrate"), !active);
}

static void
set_display_decoupled_from_gsettings (GtkSwitch *sw,
				      CcWacomPage *page)
{
	g_auto(GStrv) output = g_settings_get_strv (page->wacom_settings, "output");
	gboolean active = g_strcmp0 (output[0], "") != 0;

	gtk_switch_set_active (sw, active);
	update_display_decoupled_sensitivity (page, active);
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
	cc_wacom_panel_switch_to_panel (page->panel, "display");
	return TRUE;
}

static gboolean
mouse_clicked_cb (GtkButton   *button,
		  CcWacomPage *page)
{
	cc_wacom_panel_switch_to_panel (page->panel, "mouse");
	return TRUE;
}

static void
decouple_display_toggled_cb (GtkSwitch	 *sw,
			     GParamSpec	 *pspec,
			     CcWacomPage *page)
{
	gboolean active = gtk_switch_get_active (sw);

	update_display_decoupled_sensitivity (page, active);

	if (!active) {
		cc_wacom_device_set_output (page->stylus, NULL);
	} else {
		GdkScreen *screen;
		GnomeRRScreen *rr_screen;
		GnomeRROutput **outputs, *picked = NULL;
		g_autoptr(GError) error = NULL;
		int i;

		screen = gtk_widget_get_screen (GTK_WIDGET (sw));
		rr_screen = gnome_rr_screen_new (screen, &error);
		if (rr_screen == NULL) {
			g_warning ("Could not connect to display manager: %s", error->message);
			return;
		}

		outputs = gnome_rr_screen_list_outputs (rr_screen);

		/* Pick *some* output here. decoupled mode can only jump across
		 * monitors, not map to the full span of those. We prefer the
		 * builtin display, falling back to the first output found if
		 * there's none.
		 */
		for (i = 0; outputs[i] != NULL; i++) {
			if (gnome_rr_output_is_builtin_display (outputs[i]))
				picked = outputs[i];
		}

		if (!picked)
			picked = outputs[0];

		cc_wacom_device_set_output (page->stylus, picked);
	}
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
	CcWacomPage *self = CC_WACOM_PAGE (object);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);
	g_clear_pointer (&self->area, calib_area_free);
	g_clear_pointer (&self->button_map, gtk_widget_destroy);
	g_clear_pointer (&self->dialog, gtk_widget_destroy);
	g_clear_object (&self->builder);
	g_clear_object (&self->header_group);

	self->panel = NULL;

	G_OBJECT_CLASS (cc_wacom_page_parent_class)->dispose (object);
}

static void
cc_wacom_page_class_init (CcWacomPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cc_wacom_page_get_property;
	object_class->set_property = cc_wacom_page_set_property;
	object_class->dispose = cc_wacom_page_dispose;
}

static void
remove_link_padding (GtkWidget *widget)
{
	g_autoptr(GtkCssProvider) provider = NULL;

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
					 ".link { padding: 0px; }", -1, NULL);
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
					GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
cc_wacom_page_init (CcWacomPage *page)
{
	g_autoptr(GError) error = NULL;
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

	page->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_resource (page->builder,
                                               "/org/gnome/control-center/wacom/gnome-wacom-properties.ui",
                                               objects,
                                               &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		return;
	}

	box = WID ("main-grid");
	gtk_container_add (GTK_CONTAINER (page), box);
	gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);

	g_signal_connect (WID ("button-calibrate"), "clicked",
			  G_CALLBACK (calibrate_button_clicked_cb), page);
	g_signal_connect (WID ("map-buttons-button"), "clicked",
			  G_CALLBACK (map_buttons_button_clicked_cb), page);

	combo = GTK_COMBO_BOX (WID ("combo-tabletmode"));
	combobox_text_cellrenderer (combo, MODELABEL_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (tabletmode_changed_cb), page);

	sw = GTK_SWITCH (WID ("switch-left-handed"));
	g_signal_connect (G_OBJECT (sw), "notify::active",
			  G_CALLBACK (left_handed_toggled_cb), page);

	g_signal_connect (G_OBJECT (WID ("display-link")), "activate-link",
			  G_CALLBACK (display_clicked_cb), page);
	remove_link_padding (WID ("display-link"));

	g_signal_connect (G_OBJECT (WID ("mouse-link")), "activate-link",
			  G_CALLBACK (mouse_clicked_cb), page);
	remove_link_padding (WID ("mouse-link"));

	g_signal_connect (G_OBJECT (WID ("display-mapping-button")), "clicked",
			  G_CALLBACK (display_mapping_button_clicked_cb), page);
	g_signal_connect (G_OBJECT (WID ("display-mapping-button-2")), "clicked",
			  G_CALLBACK (display_mapping_button_clicked_cb), page);
	g_signal_connect (WID ("switch-decouple-display"), "notify::active",
			  G_CALLBACK (decouple_display_toggled_cb), page);

	page->nav = cc_wacom_nav_button_new ();
        gtk_widget_set_halign (page->nav, GTK_ALIGN_END);
        gtk_widget_set_margin_start (page->nav, 10);
	gtk_widget_show (page->nav);
	gtk_container_add (CWID ("navigation-placeholder"), page->nav);

	page->cancellable = g_cancellable_new ();
}

static void
set_icon_name (CcWacomPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	g_autofree gchar *resource = NULL;

	resource = g_strdup_printf ("/org/gnome/control-center/wacom/%s.svg", icon_name);
	gtk_image_set_from_resource (GTK_IMAGE (WID (widget_name)), resource);
}

static void
remove_left_handed (CcWacomPage *page)
{
	gtk_widget_destroy (WID ("label-left-handed"));
	gtk_widget_destroy (WID ("switch-left-handed"));
}

static void
remove_display_link (CcWacomPage *page)
{
	gtk_widget_destroy (WID ("display-link"));
}

static void
remove_mouse_link (CcWacomPage *page)
{
        gtk_widget_destroy (WID ("mouse-link"));
}

static void
remove_decouple_options (CcWacomPage *page)
{
	gtk_widget_destroy (WID ("label-decouple-display"));
	gtk_widget_destroy (WID ("switch-decouple-display"));
	gtk_widget_destroy (WID ("display-mapping-button-2"));
}

static gboolean
has_monitor (CcWacomPage *page)
{
	WacomIntegrationFlags integration_flags;

	integration_flags = cc_wacom_device_get_integration_flags (page->stylus);

	return ((integration_flags &
		 (WACOM_DEVICE_INTEGRATED_DISPLAY | WACOM_DEVICE_INTEGRATED_SYSTEM)) != 0);
}

static void
update_tablet_ui (CcWacomPage *page,
		  int          layout)
{
	WacomIntegrationFlags integration_flags;

	integration_flags = cc_wacom_device_get_integration_flags (page->stylus);

	if ((integration_flags &
	     (WACOM_DEVICE_INTEGRATED_DISPLAY | WACOM_DEVICE_INTEGRATED_SYSTEM)) != 0) {
		/* FIXME: Check we've got a puck, or a corresponding touchpad device */
		remove_mouse_link (page);
	}

	/* Hide the pad buttons if no pad is present */
	gtk_widget_set_visible (WID ("map-buttons-button"), page->pad != NULL);

	switch (layout) {
	case LAYOUT_NORMAL:
		remove_left_handed (page);
		remove_display_link (page);
		remove_decouple_options (page);
		break;
	case LAYOUT_REVERSIBLE:
		remove_display_link (page);
		remove_decouple_options (page);
		break;
	case LAYOUT_SCREEN:
		remove_left_handed (page);

		gtk_widget_destroy (WID ("display-mapping-button"));

		gtk_widget_show (WID ("button-calibrate"));
		gtk_widget_set_sensitive (WID ("button-calibrate"),
					  has_monitor (page));

		gtk_container_child_set (CWID ("main-controls-grid"),
					 WID ("label-trackingmode"),
					 "top_attach", 5, NULL);
		gtk_container_child_set (CWID ("main-controls-grid"),
					 WID ("combo-tabletmode"),
					 "top_attach", 5, NULL);
		break;
	default:
		g_assert_not_reached ();
	}
}

gboolean
cc_wacom_page_update_tools (CcWacomPage   *page,
			    CcWacomDevice *stylus,
			    CcWacomDevice *pad)
{
	int layout;
	gboolean changed;

	/* Type of layout */
	layout = get_layout_type (stylus);

	changed = (page->stylus != stylus || page->pad != pad);
	if (!changed)
		return FALSE;

	page->stylus = stylus;
	page->pad = pad;

	update_tablet_ui (CC_WACOM_PAGE (page), layout);

	return TRUE;
}

GtkWidget *
cc_wacom_page_new (CcWacomPanel  *panel,
		   CcWacomDevice *stylus,
		   CcWacomDevice *pad)
{
	CcWacomPage *page;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (stylus), NULL);
	g_return_val_if_fail (!pad || CC_IS_WACOM_DEVICE (pad), NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	page->panel = panel;

	cc_wacom_page_update_tools (page, stylus, pad);

	/* FIXME move this to construct */
	page->wacom_settings  = cc_wacom_device_get_settings (stylus);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), page);
	if (get_layout_type (page->stylus) == LAYOUT_SCREEN)
		set_display_decoupled_from_gsettings (GTK_SWITCH (WID ("switch-decouple-display")), page);

	/* Tablet name */
	gtk_label_set_text (GTK_LABEL (WID ("label-tabletmodel")), cc_wacom_device_get_name (stylus));

	/* Left-handedness */
	if (cc_wacom_device_is_reversible (stylus))
		set_left_handed_from_gsettings (page);

	/* Tablet icon */
	set_icon_name (page, "image-tablet", cc_wacom_device_get_icon_name (stylus));

	return GTK_WIDGET (page);
}

void
cc_wacom_page_set_navigation (CcWacomPage *page,
			      GtkNotebook *notebook,
			      gboolean     ignore_first_page)
{
	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	g_object_set (G_OBJECT (page->nav),
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
