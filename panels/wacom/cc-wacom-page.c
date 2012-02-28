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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "cc-wacom-page.h"
#include "cc-wacom-nav-button.h"
#include "cc-wacom-mapping-panel.h"
#include "cc-wacom-stylus-page.h"
#include "gsd-enums.h"
#include "gui_gtk.h"

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)
#define CWID(x) (GtkContainer *) gtk_builder_get_object (priv->builder, x)
#define MWID(x) (GtkWidget *) gtk_builder_get_object (priv->mapping_builder, x)

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

#define WACOM_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PAGE, CcWacomPagePrivate))

#define THRESHOLD_MISCLICK	15
#define THRESHOLD_DOUBLECLICK	7

#define ACTION_TYPE_KEY         "action-type"
#define CUSTOM_ACTION_KEY       "custom-action"
#define KEY_CUSTOM_ELEVATOR_ACTION "custom-elevator-action"

enum {
	MAPPING_DESCRIPTION_COLUMN,
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

	/* Display mapping */
	GtkWidget      *mapping;
	GtkWidget      *dialog;
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

static void
set_calibration (gint      *cal,
                 gsize      ncal,
                 GSettings *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	GVariant   **tmp;
	gsize        nvalues;
	int          i;

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
	g_variant_unref (array);
}

static void
finish_calibration (CalibArea *area,
		    gpointer   user_data)
{
	CcWacomPage *page = (CcWacomPage *) user_data;
	CcWacomPagePrivate *priv = page->priv;
	XYinfo axis;
	gboolean swap_xy;
	int cal[4];

	if (calib_area_finish (area, &axis, &swap_xy)) {
		cal[0] = axis.x_min;
		cal[1] = axis.y_min;
		cal[2] = axis.x_max;
		cal[3] = axis.y_max;

		set_calibration(cal, 4, page->priv->wacom_settings);
	}

	calib_area_free (area);
	page->priv->area = NULL;
	gtk_widget_set_sensitive (WID ("button-calibrate"), TRUE);
}

static gboolean
run_calibration (CcWacomPage *page,
		 gint        *cal,
		 gint         monitor)
{
	XYinfo old_axis;

	g_assert (page->priv->area == NULL);

	old_axis.x_min = cal[0];
	old_axis.y_min = cal[1];
	old_axis.x_max = cal[2];
	old_axis.y_max = cal[3];

	page->priv->area = calib_area_new (NULL,
					   monitor,
					   finish_calibration,
					   page,
					   &old_axis,
					   THRESHOLD_MISCLICK,
					   THRESHOLD_DOUBLECLICK);

	return FALSE;
}

static void
calibrate_button_clicked_cb (GtkButton   *button,
			     CcWacomPage *page)
{
	int i, calibration[4];
	GVariant *variant;
	int *current;
	gsize ncal;
	gint monitor;

	monitor = gsd_wacom_device_get_display_monitor (page->priv->stylus);
	if (monitor < 0) {
		/* The display the tablet should be mapped to could not be located.
		 * This shouldn't happen if the EDID data is good...
		 */
		g_critical("Output associated with the tablet is not connected. Unable to calibrate.");
		return;
	}

	variant = g_settings_get_value (page->priv->wacom_settings, "area");
	current = (int *) g_variant_get_fixed_array (variant, &ncal, sizeof (gint32));

	if (ncal != 4) {
		g_warning("Device calibration property has wrong length. Got %"G_GSIZE_FORMAT" items; expected %d.\n", ncal, 4);
		g_free (current);
		return;
	}

	for (i = 0; i < 4; i++)
		calibration[i] = current[i];

	if (calibration[0] == -1 &&
	    calibration[1] == -1 &&
	    calibration[2] == -1 &&
	    calibration[3] == -1) {
		gint *device_cal;
		device_cal = gsd_wacom_device_get_area (page->priv->stylus);
		for (i = 0; i < 4; i++)
			calibration[i] = device_cal[i];
		g_free (device_cal);
	}

	run_calibration (page, calibration, monitor);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
}

static char *
get_elevator_shortcut_string (GSettings        *settings,
			      GtkDirectionType  dir)
{
	char **strv, *str;

	strv = g_settings_get_strv (settings, KEY_CUSTOM_ELEVATOR_ACTION);
	if (strv == NULL)
		return NULL;

	if (g_strv_length (strv) >= 1 && dir == GTK_DIR_UP)
		str = g_strdup (strv[0]);
	else if (g_strv_length (strv) >= 2 && dir == GTK_DIR_DOWN)
		str = g_strdup (strv[1]);
	else
		str = NULL;

	g_strfreev (strv);

	return str;
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
		GtkCellRenderer   *cell,
		GtkTreeModel      *model,
		GtkTreeIter       *iter,
		gpointer           data)
{
	GsdWacomTabletButton *button;
	GsdWacomActionType type;
	GtkDirectionType dir;
	char *str;
	guint keyval;
	guint mask;

	gtk_tree_model_get (model, iter,
			    MAPPING_BUTTON_COLUMN, &button,
			    MAPPING_BUTTON_DIRECTION, &dir,
			    -1);

	if (button == NULL) {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
		return;
	}

	if (button->type == WACOM_TABLET_BUTTON_TYPE_HARDCODED) {
		/* FIXME this should list the name of the button,
		 * Switch Modes Touchring #1 for example */
		g_object_set (cell,
			      "visible", TRUE,
			      "editable", FALSE,
			      "accel-key", 0,
			      "accel-mods", 0,
			      "style", PANGO_STYLE_NORMAL,
			      "text", _("Switch Modes"),
			      NULL);
		return;
	}

	if (button->settings == NULL) {
		g_warning ("Button '%s' does not have an associated GSettings", button->id);
		return;
	}

	type = g_settings_get_enum (button->settings, ACTION_TYPE_KEY);
	if (type == GSD_WACOM_ACTION_TYPE_NONE) {
		g_object_set (cell,
			      "visible", TRUE,
			      "editable", TRUE,
			      "accel-key", 0,
			      "accel-mods", 0,
			      "style", PANGO_STYLE_NORMAL,
			      "text", _("None"),
			      NULL);
		return;
	}

	if (button->type == WACOM_TABLET_BUTTON_TYPE_ELEVATOR)
		str = get_elevator_shortcut_string (button->settings, dir);
	else
		str = g_settings_get_string (button->settings, CUSTOM_ACTION_KEY);

	if (str == NULL || *str == '\0') {
		g_object_set (cell,
			      "visible", TRUE,
			      "editable", TRUE,
			      "accel-key", 0,
			      "accel-mods", 0,
			      "style", PANGO_STYLE_NORMAL,
			      "text", _("None"),
			      NULL);
		g_free (str);
		return;
	}
	gtk_accelerator_parse (str, &keyval, &mask);
	g_free (str);

	g_object_set (cell,
		      "visible", TRUE,
		      "editable", TRUE,
		      "accel-key", keyval,
		      "accel-mods", mask,
		      "style", PANGO_STYLE_NORMAL,
		      NULL);
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
		  GdkEventButton *event,
		  gpointer        user_data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	if (event->window != gtk_tree_view_get_bin_window (tree_view))
		return FALSE;

	if (gtk_tree_view_get_path_at_pos (tree_view,
					   (gint) event->x,
					   (gint) event->y,
					   &path, &column,
					   NULL, NULL))
	{
		GtkTreeModel *model;
		GtkTreeIter iter;
		GsdWacomTabletButton *button;

		model = gtk_tree_view_get_model (tree_view);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    MAPPING_BUTTON_COLUMN, &button,
				    -1);

		gtk_widget_grab_focus (GTK_WIDGET (tree_view));
		gtk_tree_view_set_cursor (tree_view,
					  path,
					  gtk_tree_view_get_column (tree_view, 1),
					  TRUE);
		g_signal_stop_emission_by_name (tree_view, "button_press_event");
		gtk_tree_path_free (path);
	}
	return TRUE;
}

static void
start_editing_kb_cb (GtkTreeView *treeview,
                          GtkTreePath *path,
                          GtkTreeViewColumn *column,
                          gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GsdWacomTabletButton *button;

  model = gtk_tree_view_get_model (treeview);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      MAPPING_BUTTON_COLUMN, &button,
                      -1);

  gtk_widget_grab_focus (GTK_WIDGET (treeview));
  gtk_tree_view_set_cursor (treeview,
			    path,
			    gtk_tree_view_get_column (treeview, 1),
			    TRUE);
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       GdkModifierType        mask,
                       guint                  keycode,
                       CcWacomPage           *page)
{
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeView *view;
  GtkTreeIter iter;
  CcWacomPagePrivate *priv;
  GsdWacomTabletButton *button;
  GtkDirectionType dir;
  char *str;

  priv = page->priv;
  view = GTK_TREE_VIEW (MWID("shortcut_treeview"));
  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      MAPPING_BUTTON_COLUMN, &button,
		      MAPPING_BUTTON_DIRECTION, &dir,
                      -1);

  /* sanity check */
  if (button == NULL)
    return;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  mask &= ~GDK_LOCK_MASK;

  str = gtk_accelerator_name (keyval, mask);

  if (button->type == WACOM_TABLET_BUTTON_TYPE_ELEVATOR) {
    char *strs[3];
    char **strv;

    strs[2] = NULL;
    strs[0] = strs[1] = "";
    strv = g_settings_get_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION);
    if (strv != NULL) {
	    if (g_strv_length (strv) >= 1)
		    strs[0] = strv[0];
	    if (g_strv_length (strv) >= 2)
		    strs[1] = strv[1];
    }

    if (dir == GTK_DIR_UP)
	    strs[0] = str;
    else
	    strs[1] = str;

    g_settings_set_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION, (const gchar * const*) strs);
    if (strv != NULL)
	    g_strfreev (strv);
  } else {
    g_settings_set_string (button->settings, CUSTOM_ACTION_KEY, str);
  }
  g_settings_set_enum (button->settings, ACTION_TYPE_KEY, GSD_WACOM_ACTION_TYPE_CUSTOM);
  g_free (str);
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
                        const char          *path_string,
                        CcWacomPage         *page)
{
  GtkTreeView *view;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GsdWacomTabletButton *button;
  CcWacomPagePrivate *priv;
  GtkDirectionType dir;

  priv = page->priv;
  view = GTK_TREE_VIEW (MWID("shortcut_treeview"));
  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      MAPPING_BUTTON_COLUMN, &button,
		      MAPPING_BUTTON_DIRECTION, &dir,
                      -1);

  /* sanity check */
  if (button == NULL)
    return;

  /* Unset the key */
  if (button->type == WACOM_TABLET_BUTTON_TYPE_ELEVATOR) {
    char *strs[3];
    char **strv;

    strs[2] = NULL;
    strs[0] = strs[1] = "";
    strv = g_settings_get_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION);
    if (strv != NULL) {
	    if (g_strv_length (strv) >= 1)
		    strs[0] = strv[0];
	    if (g_strv_length (strv) >= 2)
		    strs[1] = strv[1];
    }

    if (dir == GTK_DIR_UP)
	    strs[0] = "";
    else
	    strs[1] = "";

    if (*strs[0] == '\0' && *strs[1] == '\0')
	    g_settings_set_enum (button->settings, ACTION_TYPE_KEY, GSD_WACOM_ACTION_TYPE_NONE);
    g_settings_set_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION, (const gchar * const*) strs);
    if (strv != NULL)
	    g_strfreev (strv);
  } else {
	  g_settings_set_enum (button->settings, ACTION_TYPE_KEY, GSD_WACOM_ACTION_TYPE_NONE);
	  g_settings_set_string (button->settings, CUSTOM_ACTION_KEY, "");
  }
}

static void
add_button_to_store (GtkListStore         *model,
		     GsdWacomTabletButton *button,
		     GtkDirectionType      dir)
{
	GtkTreeIter new_row;
	char *dir_name;

	if (dir == GTK_DIR_UP || dir == GTK_DIR_DOWN) {
		dir_name = g_strdup_printf ("%s (%s)",
					    button->name,
					    dir == GTK_DIR_UP ? _("Up") : _("Down"));
	} else {
		dir_name = NULL;
	}

	gtk_list_store_append (model, &new_row);
	gtk_list_store_set (model, &new_row,
			    MAPPING_DESCRIPTION_COLUMN, dir_name ? dir_name : button->name,
			    MAPPING_BUTTON_COLUMN, button,
			    MAPPING_BUTTON_DIRECTION, dir,
			    -1);
	g_free (dir_name);
}

static void
setup_mapping_treeview (CcWacomPage *page)
{
	CcWacomPagePrivate *priv;
	GtkTreeView *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *model;
	GList *list, *l;

	priv = page->priv;
	treeview = GTK_TREE_VIEW(MWID ("shortcut_treeview"));

	g_signal_connect (treeview, "button_press_event",
			  G_CALLBACK (start_editing_cb), page);
	g_signal_connect (treeview, "row-activated",
			  G_CALLBACK (start_editing_kb_cb), page);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Button"),
							   renderer,
							   "text", MAPPING_DESCRIPTION_COLUMN,
							   NULL);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_expand (column, TRUE);

	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_sort_column_id (column, MAPPING_DESCRIPTION_COLUMN);

	renderer = (GtkCellRenderer *) g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL,
						     "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
						     NULL);

	g_signal_connect (renderer, "accel_edited",
			  G_CALLBACK (accel_edited_callback),
			  page);
	g_signal_connect (renderer, "accel_cleared",
			  G_CALLBACK (accel_cleared_callback),
			  page);

	column = gtk_tree_view_column_new_with_attributes (_("Action"), renderer, NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_expand (column, FALSE);

	gtk_tree_view_append_column (treeview, column);

	model = gtk_list_store_new (MAPPING_N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
	gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (model));

	/* Fill it up! */
	list = gsd_wacom_device_get_buttons (page->priv->pad);
	for (l = list; l != NULL; l = l->next) {
		GsdWacomTabletButton *button = l->data;

		if (button->type == WACOM_TABLET_BUTTON_TYPE_ELEVATOR) {
			add_button_to_store (model, button, GTK_DIR_UP);
			add_button_to_store (model, button, GTK_DIR_DOWN);
		} else {
			add_button_to_store (model, button, 0);
		}
	}
	g_list_free (list);
	g_object_unref (model);
}

static void
button_mapping_dialog_closed (GtkDialog   *dialog,
			      int          response_id,
			      CcWacomPage *page)
{
	CcWacomPagePrivate *priv;

	priv = page->priv;
	gtk_widget_destroy (MWID ("button-mapping-dialog"));
	g_object_unref (page->priv->mapping_builder);
	page->priv->mapping_builder = NULL;
}

static void
map_buttons_button_clicked_cb (GtkButton   *button,
			       CcWacomPage *page)
{
	GError *error = NULL;
	GtkWidget *dialog;
	CcWacomPagePrivate *priv;
	GtkWidget *toplevel;

	priv = page->priv;

	g_assert (priv->mapping_builder == NULL);
	priv->mapping_builder = gtk_builder_new ();
	gtk_builder_add_from_file (priv->mapping_builder,
				   GNOMECC_UI_DIR "/button-mapping.ui",
				   &error);

	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->mapping_builder);
		priv->mapping_builder = NULL;
		g_error_free (error);
		return;
	}

	setup_mapping_treeview (page);

	dialog = MWID ("button-mapping-dialog");
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (button_mapping_dialog_closed), page);

	gtk_widget_show (dialog);
}

static void
display_mapping_dialog_closed (GtkDialog   *dialog,
			       int          response_id,
			       CcWacomPage *page)
{
	CcWacomPagePrivate *priv;

	priv = page->priv;
	gtk_widget_destroy (priv->dialog);
	priv->dialog = NULL;
	priv->mapping = NULL;
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
						    GTK_STOCK_CLOSE,
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

static void
left_handed_toggled_cb (GtkSwitch *sw, GParamSpec *pspec, gpointer *user_data)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(user_data)->priv;
	const gchar*		rotation;

	rotation = gtk_switch_get_active (sw) ? "half" : "none";

	g_settings_set_string (priv->wacom_settings, "rotation", rotation);
}

static void
set_left_handed_from_gsettings (CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(page)->priv;
	const gchar*		rotation;

	rotation = g_settings_get_string (priv->wacom_settings, "rotation");
	if (strcmp (rotation, "half") == 0)
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

	if (priv->area) {
		calib_area_free (priv->area);
		priv->area = NULL;
	}

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}


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

	gtk_builder_add_objects_from_file (priv->builder,
					   GNOMECC_UI_DIR "/gnome-wacom-properties.ui",
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

	g_signal_connect (G_OBJECT (WID ("display-mapping-button")), "clicked",
			  G_CALLBACK (display_mapping_button_clicked_cb), self);

	priv->nav = cc_wacom_nav_button_new ();
        gtk_widget_set_halign (priv->nav, GTK_ALIGN_END);
        gtk_widget_set_margin_left (priv->nav, 10);
	gtk_grid_attach (GTK_GRID (box), priv->nav, 1, 0, 1, 1);
}

static void
set_icon_name (CcWacomPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	CcWacomPagePrivate *priv;
	char *filename, *path;

	priv = page->priv;

	filename = g_strdup_printf ("%s.svg", icon_name);
	path = g_build_filename (GNOMECC_UI_DIR, filename, NULL);
	g_free (filename);

	gtk_image_set_from_file (GTK_IMAGE (WID (widget_name)), path);
	g_free (path);
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

/* Different types of layout for the tablet config */
enum {
	LAYOUT_NORMAL,        /* tracking mode, button mapping */
	LAYOUT_REVERSIBLE,    /* tracking mode, button mapping, left-hand orientation */
	LAYOUT_SCREEN        /* button mapping, calibration, display resolution */
};

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
update_tablet_ui (CcWacomPage *page,
		  int          layout)
{
	CcWacomPagePrivate *priv;

	priv = page->priv;

	/* FIXME Handle ->pad being NULL and hide the pad buttons */

	switch (layout) {
	case LAYOUT_NORMAL:
		remove_left_handed (page->priv);
		remove_display_link (page->priv);
		break;
	case LAYOUT_REVERSIBLE:
		remove_display_link (page->priv);
		break;
	case LAYOUT_SCREEN:
		remove_left_handed (page->priv);

		gtk_widget_destroy (WID ("combo-tabletmode"));
		gtk_widget_destroy (WID ("label-trackingmode"));
		gtk_widget_destroy (WID ("display-mapping-button"));

		gtk_widget_show (WID ("button-calibrate"));
		gtk_widget_show (WID ("display-link"));

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

GtkWidget *
cc_wacom_page_new (CcWacomPanel   *panel,
		   GsdWacomDevice *stylus,
		   GsdWacomDevice *eraser,
		   GsdWacomDevice *pad)
{
	CcWacomPage *page;
	CcWacomPagePrivate *priv;
	int layout;

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (stylus), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (stylus) == WACOM_TYPE_STYLUS, NULL);

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (eraser), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (eraser) == WACOM_TYPE_ERASER, NULL);

	if (pad != NULL)
		g_return_val_if_fail (gsd_wacom_device_get_device_type (pad) == WACOM_TYPE_PAD, NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	priv = page->priv;
	priv->panel = panel;
	priv->stylus = stylus;
	priv->eraser = eraser;
	priv->pad = pad;

	/* FIXME move this to construct */
	priv->wacom_settings  = gsd_wacom_device_get_settings (stylus);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), page);

	/* Tablet name */
	gtk_label_set_text (GTK_LABEL (WID ("label-tabletmodel")), gsd_wacom_device_get_name (stylus));

	/* Type of layout */
	if (gsd_wacom_device_is_screen_tablet (stylus))
		layout = LAYOUT_SCREEN;
	else if (gsd_wacom_device_reversible (stylus))
		layout = LAYOUT_REVERSIBLE;
	else
		layout = LAYOUT_NORMAL;

	update_tablet_ui (page, layout);

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
