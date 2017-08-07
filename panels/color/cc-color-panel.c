/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
 */

#include <config.h>

#include <glib/gi18n.h>
#include <colord.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libsoup/soup.h>

#include "shell/list-box-helper.h"
#include "cc-color-calibrate.h"
#include "cc-color-cell-renderer-text.h"
#include "cc-color-panel.h"
#include "cc-color-resources.h"
#include "cc-color-common.h"
#include "cc-color-device.h"
#include "cc-color-profile.h"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

CC_PANEL_REGISTER (CcColorPanel, cc_color_panel)

#define COLOR_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_COLOR_PANEL, CcColorPanelPrivate))

struct _CcColorPanelPrivate
{
  CdClient      *client;
  CdDevice      *current_device;
  GPtrArray     *devices;
  GPtrArray     *sensors;
  GCancellable  *cancellable;
  GDBusProxy    *proxy;
  GSettings     *settings;
  GSettings     *settings_colord;
  GtkBuilder    *builder;
  GtkWidget     *assistant_calib;
  GtkWidget     *dialog_assign;
  GtkWidget     *main_window;
  CcColorCalibrate *calibrate;
  GtkListBox    *list_box;
  gchar         *list_box_filter;
  guint          list_box_selected_id;
  guint          list_box_activated_id;
  GtkSizeGroup  *list_box_size;
  gboolean       is_live_cd;
  gboolean       model_is_changing;
};

enum {
  GCM_PREFS_COMBO_COLUMN_TEXT,
  GCM_PREFS_COMBO_COLUMN_PROFILE,
  GCM_PREFS_COMBO_COLUMN_TYPE,
  GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME,
  GCM_PREFS_COMBO_COLUMN_NUM_COLUMNS
};

/* for the GtkListStores */
enum {
  COLUMN_CALIB_KIND_DESCRIPTION,
  COLUMN_CALIB_KIND_CAP_VALUE,
  COLUMN_CALIB_KIND_VISIBLE,
  COLUMN_CALIB_KIND_LAST
};
enum {
  COLUMN_CALIB_QUALITY_DESCRIPTION,
  COLUMN_CALIB_QUALITY_APPROX_TIME,
  COLUMN_CALIB_QUALITY_VALUE,
  COLUMN_CALIB_QUALITY_LAST
};
enum {
  COLUMN_CALIB_SENSOR_OBJECT,
  COLUMN_CALIB_SENSOR_DESCRIPTION,
  COLUMN_CALIB_SENSOR_LAST
};
enum {
  COLUMN_CALIB_TEMP_DESCRIPTION,
  COLUMN_CALIB_TEMP_VALUE_K,
  COLUMN_CALIB_TEMP_LAST
};

#define COLORD_SETTINGS_SCHEMA                          "org.freedesktop.ColorHelper"
#define GCM_SETTINGS_SCHEMA                             "org.gnome.settings-daemon.plugins.color"
#define GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD      "recalibrate-printer-threshold"
#define GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD      "recalibrate-display-threshold"

/* max number of devices and profiles to cause auto-expand at startup */
#define GCM_PREFS_MAX_DEVICES_PROFILES_EXPANDED         5

static void gcm_prefs_refresh_toolbar_buttons (CcColorPanel *panel);

static void
gcm_prefs_combobox_add_profile (CcColorPanel *prefs,
                                CdProfile *profile,
                                GtkTreeIter *iter)
{
  const gchar *id;
  GtkTreeIter iter_tmp;
  GString *string;
  GtkListStore *list_store;
  gchar *escaped = NULL;
  guint kind = 0;
  const gchar *warning = NULL;
#if CD_CHECK_VERSION(0,1,25)
  gchar **warnings;
#endif

  /* iter is optional */
  if (iter == NULL)
    iter = &iter_tmp;

  /* use description */
  string = g_string_new (cd_profile_get_title (profile));

  /* any source prefix? */
  id = cd_profile_get_metadata_item (profile,
                                     CD_PROFILE_METADATA_DATA_SOURCE);
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile has been auto-generated for this hardware */
      g_string_prepend (string, _("Default: "));
      kind = 1;
    }
#if CD_CHECK_VERSION(0,1,14)
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile his a standard space like AdobeRGB */
      g_string_prepend (string, _("Colorspace: "));
      kind = 2;
    }
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile is a test profile */
      g_string_prepend (string, _("Test profile: "));
      kind = 3;
    }
#endif

  /* is the profile faulty */
#if CD_CHECK_VERSION(0,1,25)
  warnings = cd_profile_get_warnings (profile);
  if (warnings != NULL && warnings[0] != NULL)
    warning = "dialog-warning-symbolic";
#endif

  escaped = g_markup_escape_text (string->str, -1);
  list_store = GTK_LIST_STORE(gtk_builder_get_object (prefs->priv->builder,
                                                      "liststore_assign"));
  gtk_list_store_append (list_store, iter);
  gtk_list_store_set (list_store, iter,
                      GCM_PREFS_COMBO_COLUMN_TEXT, escaped,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
                      GCM_PREFS_COMBO_COLUMN_TYPE, kind,
                      GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME, warning,
                      -1);

  g_string_free (string, TRUE);
}

static void
gcm_prefs_default_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CdProfile *profile;
  gboolean ret;
  GError *error = NULL;
  CcColorPanelPrivate *priv = prefs->priv;

  /* TODO: check if the profile is already systemwide */
  profile = cd_device_get_default_profile (priv->current_device);
  if (profile == NULL)
    goto out;

  /* install somewhere out of $HOME */
  ret = cd_profile_install_system_wide_sync (profile,
                                             priv->cancellable,
                                             &error);
  if (!ret)
    {
      g_warning ("failed to set profile system-wide: %s",
           error->message);
      g_error_free (error);
      goto out;
    }
out:
  if (profile != NULL)
    g_object_unref (profile);
}

static GFile *
gcm_prefs_file_chooser_get_icc_profile (CcColorPanel *prefs)
{
  GtkWindow *window;
  GtkWidget *dialog;
  GFile *file = NULL;
  GtkFileFilter *filter;
  CcColorPanelPrivate *priv = prefs->priv;

  /* create new dialog */
  window = GTK_WINDOW (priv->dialog_assign);
  /* TRANSLATORS: an ICC profile is a file containing colorspace data */
  dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Import"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
  gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

  /* setup the filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

  /* TRANSLATORS: filter name on the file->open dialog */
  gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

  /* setup the all files filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*");
  /* TRANSLATORS: filter name on the file->open dialog */
  gtk_file_filter_set_name (filter, _("All files"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

  /* did user choose file */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dialog));

  /* we're done */
  gtk_widget_destroy (dialog);

  /* or NULL for missing */
  return file;
}

static void
gcm_prefs_calib_cancel_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  gtk_widget_hide (prefs->priv->assistant_calib);
}

static gboolean
gcm_prefs_calib_delayed_complete_cb (gpointer user_data)
{
  CcColorPanel *panel = CC_COLOR_PANEL (user_data);
  GtkAssistant *assistant;
  GtkWidget *widget;

  assistant = GTK_ASSISTANT (panel->priv->assistant_calib);
  widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                               "box_calib_brightness"));
  gtk_assistant_set_page_complete (assistant, widget, TRUE);
  return FALSE;
}

static void
gcm_prefs_calib_prepare_cb (GtkAssistant *assistant,
                            GtkWidget    *page,
                            CcColorPanel *panel)
{
  GtkWidget *widget;

  /* give the user the indication they should actually manually set the
   * desired brightness rather than clicking blindly by delaying the
   * "Next" button deliberately for a second or so */
  widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
                                               "box_calib_brightness"));
  if (widget == page)
  {
    g_timeout_add_seconds (1, gcm_prefs_calib_delayed_complete_cb, panel);
    return;
  }

  /* disable the brightness page as we don't want to show a 'Finished'
   * button if the user goes back at any point */
  gtk_assistant_set_page_complete (assistant, widget, FALSE);
}

static void
gcm_prefs_calib_apply_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  gboolean ret;
  GError *error = NULL;
  GtkWindow *window = NULL;

  /* setup the calibration object with items that can fail */
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "button_calib_upload"));
  gtk_widget_show (widget);
  ret = cc_color_calibrate_setup (prefs->priv->calibrate,
                                  &error);
  if (!ret)
    {
      g_warning ("failed to setup calibrate: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* actually start the calibration */
  window = GTK_WINDOW (prefs->priv->assistant_calib);
  ret = cc_color_calibrate_start (prefs->priv->calibrate,
                                  window,
                                  &error);
  if (!ret)
    {
      g_warning ("failed to start calibrate: %s", error->message);
      gtk_widget_hide (GTK_WIDGET (window));
      g_error_free (error);
      goto out;
    }

  /* if we are a LiveCD then don't close the window as there is another
   * summary pane with the export button */
  if (!prefs->priv->is_live_cd)
    gtk_widget_hide (GTK_WIDGET (window));
out:
  return;
}

static gboolean
gcm_prefs_calib_delete_event_cb (GtkWidget *widget,
                                 GdkEvent *event,
                                 CcColorPanel *prefs)
{
  /* do not destroy the window */
  gcm_prefs_calib_cancel_cb (widget, prefs);
  return TRUE;
}

static void
gcm_prefs_calib_temp_treeview_clicked_cb (GtkTreeSelection *selection,
                                          CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *widget;
  guint target_whitepoint;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (prefs->priv->assistant_calib);
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "box_calib_temp"));
  gtk_assistant_set_page_complete (assistant, widget, ret);
  if (!ret)
    return;

  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_TEMP_VALUE_K, &target_whitepoint,
                      -1);
  cc_color_calibrate_set_temperature (priv->calibrate, target_whitepoint);
}

static void
gcm_prefs_calib_kind_treeview_clicked_cb (GtkTreeSelection *selection,
                                          CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdSensorCap device_kind;
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *widget;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (prefs->priv->assistant_calib);
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "box_calib_kind"));
  gtk_assistant_set_page_complete (assistant, widget, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_KIND_CAP_VALUE, &device_kind,
                      -1);
  cc_color_calibrate_set_kind (priv->calibrate, device_kind);
}

static void
gcm_prefs_calib_quality_treeview_clicked_cb (GtkTreeSelection *selection,
                                             CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdProfileQuality quality;
  gboolean ret;
  GtkAssistant *assistant;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *widget;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (prefs->priv->assistant_calib);
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "box_calib_quality"));
  gtk_assistant_set_page_complete (assistant, widget, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_QUALITY_VALUE, &quality,
                      -1);
  cc_color_calibrate_set_quality (priv->calibrate, quality);
}

static gboolean
gcm_prefs_calib_set_sensor_cap_supported_cb (GtkTreeModel *model,
                                             GtkTreePath *path,
                                             GtkTreeIter *iter,
                                             gpointer data)
{
  CdSensorCap cap;
  CdSensor *sensor = CD_SENSOR (data);
  gboolean supported;

  gtk_tree_model_get (model, iter,
                      COLUMN_CALIB_KIND_CAP_VALUE, &cap,
                      -1);
  supported = cd_sensor_has_cap (sensor, cap);
  g_debug ("%s(%s) is %s",
           cd_sensor_get_model (sensor),
           cd_sensor_cap_to_string (cap),
           supported ? "supported" : "not-supported");
  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      COLUMN_CALIB_KIND_VISIBLE, supported,
                      -1);
  return FALSE;
}

static guint8
_cd_bitfield_popcount (guint64 bitfield)
{
  guint8 i;
  guint8 tmp = 0;
  for (i = 0; i < 64; i++)
    tmp += cd_bitfield_contain (bitfield, i);
  return tmp;
}

static void
gcm_prefs_calib_set_sensor (CcColorPanel *prefs,
                            CdSensor *sensor)
{
  CcColorPanelPrivate *priv = prefs->priv;
  GtkTreeModel *model;
  GtkWidget *page;
  guint64 caps;
  guint8 i;

  /* use this sensor for calibration */
  cc_color_calibrate_set_sensor (priv->calibrate, sensor);

  /* hide display types the sensor does not support */
  model = GTK_TREE_MODEL (gtk_builder_get_object (priv->builder,
                                                  "liststore_calib_kind"));
  gtk_tree_model_foreach (model,
                          gcm_prefs_calib_set_sensor_cap_supported_cb,
                          sensor);

  /* if the sensor only supports one kind then do not show the panel at all */
  page = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                             "box_calib_kind"));
  caps = cd_sensor_get_caps (sensor);
  if (_cd_bitfield_popcount (caps) == 1)
    {
      gtk_widget_set_visible (page, FALSE);
      for (i = 0; i < CD_SENSOR_CAP_LAST; i++)
        {
          if (cd_bitfield_contain (caps, i))
            cc_color_calibrate_set_kind (priv->calibrate, i);
        }
    }
  else
    {
      cc_color_calibrate_set_kind (priv->calibrate, CD_SENSOR_CAP_UNKNOWN);
      gtk_widget_set_visible (page, TRUE);
    }
}

static void
gcm_prefs_calib_sensor_treeview_clicked_cb (GtkTreeSelection *selection,
                                             CcColorPanel *prefs)
{
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *widget;
  CdSensor *sensor;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (prefs->priv->assistant_calib);
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "box_calib_sensor"));
  gtk_assistant_set_page_complete (assistant, widget, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_SENSOR_OBJECT, &sensor,
                      -1);
  gcm_prefs_calib_set_sensor (prefs, sensor);
  g_object_unref (sensor);
}

static void
gcm_prefs_calibrate_display (CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdSensor *sensor_tmp;
  const gchar *tmp;
  GtkListStore *liststore;
  GtkTreeIter iter;
  GtkWidget *page;
  GtkWidget *widget;
  guint i;

  /* set target device */
  cc_color_calibrate_set_device (priv->calibrate, priv->current_device);

  /* add sensors to list */
  liststore = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                      "liststore_calib_sensor"));
  gtk_list_store_clear (liststore);
  page = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                             "box_calib_sensor"));
  if (priv->sensors->len > 1)
    {
      for (i = 0; i < priv->sensors->len; i++)
        {
          sensor_tmp = g_ptr_array_index (priv->sensors, i);
          gtk_list_store_append (liststore, &iter);
          gtk_list_store_set (liststore, &iter,
                              COLUMN_CALIB_SENSOR_OBJECT, sensor_tmp,
                              COLUMN_CALIB_SENSOR_DESCRIPTION, cd_sensor_get_model (sensor_tmp),
                              -1);
        }
      gtk_widget_set_visible (page, TRUE);
    }
  else
    {
      sensor_tmp = g_ptr_array_index (priv->sensors, 0);
      gcm_prefs_calib_set_sensor (prefs, sensor_tmp);
      gtk_widget_set_visible (page, FALSE);
    }

  /* set default profile title */
  tmp = cd_device_get_model (priv->current_device);
  if (tmp == NULL)
    tmp = cd_device_get_vendor (priv->current_device);
  if (tmp == NULL)
    tmp = _("Screen");
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "entry_calib_title"));
  gtk_entry_set_text (GTK_ENTRY (widget), tmp);
  cc_color_calibrate_set_title (priv->calibrate, tmp);

  /* set the display whitepoint to D65 by default */
  //FIXME?

  /* show ui */
  widget = GTK_WIDGET (priv->assistant_calib);
  gtk_window_set_transient_for (GTK_WINDOW (widget),
                                GTK_WINDOW (priv->main_window));
  gtk_widget_show (widget);
}

static void
gcm_prefs_title_entry_changed_cb (GtkWidget *widget,
                                  GParamSpec *param_spec,
                                  CcColorPanel *prefs)
{
  GtkAssistant *assistant;
  GtkWidget *page;
  const gchar *value;

  assistant = GTK_ASSISTANT (prefs->priv->assistant_calib);
  page = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                             "box_calib_title"));
  value = gtk_entry_get_text (GTK_ENTRY (widget));
  cc_color_calibrate_set_title (prefs->priv->calibrate, value);
  gtk_assistant_set_page_complete (assistant, page, value[0] != '\0');
}

static void
gcm_prefs_calibrate_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  gboolean ret;
  GError *error = NULL;
  guint xid;
  GPtrArray *argv;
  CcColorPanelPrivate *priv = prefs->priv;

  /* use the new-style calibration helper */
  if (cd_device_get_kind (priv->current_device) == CD_DEVICE_KIND_DISPLAY)
    {
      gcm_prefs_calibrate_display (prefs);
      return;
    }

  /* get xid */
  xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (priv->main_window)));

  /* run with modal set */
  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_build_filename (BINDIR, "gcm-calibrate", NULL));
  g_ptr_array_add (argv, g_strdup ("--device"));
  g_ptr_array_add (argv, g_strdup (cd_device_get_id (priv->current_device)));
  g_ptr_array_add (argv, g_strdup ("--parent-window"));
  g_ptr_array_add (argv, g_strdup_printf ("%i", xid));
  g_ptr_array_add (argv, NULL);
  ret = g_spawn_async (NULL, (gchar**) argv->pdata, NULL, 0,
                       NULL, NULL, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to run calibrate: %s", error->message);
      g_error_free (error);
    }
  g_ptr_array_unref (argv);
}

static gboolean
gcm_prefs_is_profile_suitable_for_device (CdProfile *profile,
                                          CdDevice *device)
{
  const gchar *data_source;
  CdProfileKind profile_kind_tmp;
  CdProfileKind profile_kind;
  CdColorspace profile_colorspace;
  CdColorspace device_colorspace = 0;
  gboolean ret = FALSE;
  CdDeviceKind device_kind;
  CdStandardSpace standard_space;

  /* not the right colorspace */
  device_colorspace = cd_device_get_colorspace (device);
  profile_colorspace = cd_profile_get_colorspace (profile);
  if (device_colorspace != profile_colorspace)
    goto out;

  /* if this is a display matching with one of the standard spaces that displays
   * could emulate, also mark it as suitable */
  if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY &&
      cd_profile_get_kind (profile) == CD_PROFILE_KIND_DISPLAY_DEVICE)
      {
        data_source = cd_profile_get_metadata_item (profile,
                                                    CD_PROFILE_METADATA_STANDARD_SPACE);
        standard_space = cd_standard_space_from_string (data_source);
        if (standard_space == CD_STANDARD_SPACE_SRGB ||
            standard_space == CD_STANDARD_SPACE_ADOBE_RGB)
          {
            ret = TRUE;
            goto out;
          }
      }

  /* not the correct kind */
  device_kind = cd_device_get_kind (device);
  profile_kind_tmp = cd_profile_get_kind (profile);
  profile_kind = cd_device_kind_to_profile_kind (device_kind);
  if (profile_kind_tmp != profile_kind)
    goto out;

  /* ignore the colorspace profiles */
  data_source = cd_profile_get_metadata_item (profile,
                                              CD_PROFILE_METADATA_DATA_SOURCE);
  if (g_strcmp0 (data_source, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    goto out;

  /* success */
  ret = TRUE;
out:
  return ret;
}

static gint
gcm_prefs_combo_sort_func_cb (GtkTreeModel *model,
                              GtkTreeIter *a,
                              GtkTreeIter *b,
                              gpointer user_data)
{
  gint type_a, type_b;
  gchar *text_a;
  gchar *text_b;
  gint retval;

  /* get data from model */
  gtk_tree_model_get (model, a,
                      GCM_PREFS_COMBO_COLUMN_TYPE, &type_a,
                      GCM_PREFS_COMBO_COLUMN_TEXT, &text_a,
                      -1);
  gtk_tree_model_get (model, b,
                      GCM_PREFS_COMBO_COLUMN_TYPE, &type_b,
                      GCM_PREFS_COMBO_COLUMN_TEXT, &text_b,
                      -1);

  /* prefer normal type profiles over the 'Other Profile...' entry */
  if (type_a < type_b)
    retval = -1;
  else if (type_a > type_b)
    retval = 1;
  else
    retval = g_strcmp0 (text_a, text_b);

  g_free (text_a);
  g_free (text_b);
  return retval;
}

static gboolean
gcm_prefs_profile_exists_in_array (GPtrArray *array, CdProfile *profile)
{
  CdProfile *profile_tmp;
  guint i;

  for (i = 0; i < array->len; i++)
    {
      profile_tmp = g_ptr_array_index (array, i);
      if (cd_profile_equal (profile, profile_tmp))
         return TRUE;
    }
  return FALSE;
}

static void
gcm_prefs_add_profiles_suitable_for_devices (CcColorPanel *prefs,
                                             GPtrArray *profiles)
{
  CdProfile *profile_tmp;
  gboolean ret;
  GError *error = NULL;
  GPtrArray *profile_array = NULL;
  GtkTreeIter iter;
  GtkListStore *list_store;
  GtkWidget *widget;
  guint i;
  CcColorPanelPrivate *priv = prefs->priv;

  list_store = GTK_LIST_STORE(gtk_builder_get_object (prefs->priv->builder,
                                                      "liststore_assign"));
  gtk_list_store_clear (list_store);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store),
                                        GCM_PREFS_COMBO_COLUMN_TEXT,
                                        GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (list_store),
                                   GCM_PREFS_COMBO_COLUMN_TEXT,
                                   gcm_prefs_combo_sort_func_cb,
                                   list_store, NULL);

  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "label_assign_warning"));
  gtk_widget_hide (widget);

  /* get profiles */
  profile_array = cd_client_get_profiles_sync (priv->client,
                                               priv->cancellable,
                                               &error);
  if (profile_array == NULL)
    {
      g_warning ("failed to get profiles: %s",
           error->message);
      g_error_free (error);
      goto out;
    }

  /* add profiles of the right kind */
  for (i = 0; i < profile_array->len; i++)
    {
      profile_tmp = g_ptr_array_index (profile_array, i);

      /* get properties */
      ret = cd_profile_connect_sync (profile_tmp,
                                     priv->cancellable,
                                     &error);
      if (!ret)
        {
          g_warning ("failed to get profile: %s", error->message);
          g_error_free (error);
          goto out;
        }

      /* don't add any of the already added profiles */
      if (profiles != NULL)
        {
          if (gcm_prefs_profile_exists_in_array (profiles, profile_tmp))
            continue;
        }

      /* only add correct types */
      ret = gcm_prefs_is_profile_suitable_for_device (profile_tmp,
                                                      priv->current_device);
      if (!ret)
        continue;

#if CD_CHECK_VERSION(0,1,13)
      /* ignore profiles from other user accounts */
      if (!cd_profile_has_access (profile_tmp))
        continue;
#endif

      /* add */
      gcm_prefs_combobox_add_profile (prefs,
                                      profile_tmp,
                                      &iter);
    }
out:
  if (profile_array != NULL)
    g_ptr_array_unref (profile_array);
}

static void
gcm_prefs_calib_upload_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdProfile *profile;
  const gchar *uri;
  gboolean ret;
  gchar *upload_uri = NULL;
  gchar *msg_result = NULL;
  gchar *data = NULL;
  GError *error = NULL;
  gsize length;
  guint status_code;
  SoupBuffer *buffer = NULL;
  SoupMessage *msg = NULL;
  SoupMultipart *multipart = NULL;
  SoupSession *session = NULL;

  profile = cc_color_calibrate_get_profile (prefs->priv->calibrate);
  ret = cd_profile_connect_sync (profile, NULL, &error);
  if (!ret)
    {
      g_warning ("Failed to get imported profile: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* read file */
  ret = g_file_get_contents (cd_profile_get_filename (profile),
                             &data,
                             &length,
                             &error);
  if (!ret)
    {
      g_warning ("Failed to read file: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* setup the session */
  session = soup_session_sync_new_with_options (SOUP_SESSION_USER_AGENT, "gnome-control-center",
                                                SOUP_SESSION_TIMEOUT, 5000,
                                                NULL);
  if (session == NULL)
  {
    g_warning ("Failed to setup networking");
    goto out;
  }
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_DEFAULT);

  /* create multipart form and upload file */
  multipart = soup_multipart_new (SOUP_FORM_MIME_TYPE_MULTIPART);
  buffer = soup_buffer_new (SOUP_MEMORY_STATIC, data, length);
  soup_multipart_append_form_file (multipart,
                                   "upload",
                                   cd_profile_get_filename (profile),
                                   NULL,
                                   buffer);
  upload_uri = g_settings_get_string (priv->settings_colord, "profile-upload-uri");
  msg = soup_form_request_new_from_multipart (upload_uri, multipart);
  status_code = soup_session_send_message (session, msg);
  if (status_code != 201)
    {
      widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                   "label_calib_upload_location"));
      /* TRANSLATORS: this is when the upload of the profile failed */
      msg_result = g_strdup_printf (_("Failed to upload file: %s"), msg->reason_phrase),
      gtk_label_set_label (GTK_LABEL (widget), msg_result);
      gtk_widget_show (widget);
      goto out;
    }

  /* show instructions to the user */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_calib_upload_location"));
  uri = soup_message_headers_get_one (msg->response_headers, "Location");
  msg_result = g_strdup_printf ("%s %s\n\n• %s\n• %s\n• %s",
                                /* TRANSLATORS: these are instructions on how to recover
                                 * the ICC profile on the native operating system and are
                                 * only shown when the user uses a LiveCD to calibrate */
                                _("The profile has been uploaded to:"),
                                uri,
                                _("Write down this URL."),
                                _("Restart this computer and boot your normal operating system."),
                                _("Type the URL into your browser to download and install the profile.")),
  gtk_label_set_label (GTK_LABEL (widget), msg_result);
  gtk_widget_show (widget);

  /* hide the upload button as duplicate uploads will fail */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_calib_upload"));
  gtk_widget_hide (widget);
out:
  if (session != NULL)
    g_object_unref (session);
  if (buffer != NULL)
    soup_buffer_free (buffer);
  if (msg != NULL)
    g_object_unref (msg);
  if (multipart != NULL)
    soup_multipart_free (multipart);
  g_free (data);
  g_free (upload_uri);
  g_free (msg_result);
}

static void
gcm_prefs_calib_export_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CdProfile *profile;
  gboolean ret;
  gchar *default_name = NULL;
  GError *error = NULL;
  GFile *destination = NULL;
  GFile *source = NULL;
  GtkWidget *dialog;

  /* TRANSLATORS: this is the dialog to save the ICC profile */
  dialog = gtk_file_chooser_dialog_new (_("Save Profile"),
                                        GTK_WINDOW (prefs->priv->main_window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  profile = cc_color_calibrate_get_profile (prefs->priv->calibrate);
  ret = cd_profile_connect_sync (profile, NULL, &error);
  if (!ret)
    {
      g_warning ("Failed to get imported profile: %s", error->message);
      g_error_free (error);
      goto out;
    }
  default_name = g_strdup_printf ("%s.icc", cd_profile_get_title (profile));
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), default_name);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      source = g_file_new_for_path (cd_profile_get_filename (profile));
      destination = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      ret = g_file_copy (source,
                         destination,
                         G_FILE_COPY_OVERWRITE,
                         NULL,
                         NULL,
                         NULL,
                         &error);
      if (!ret)
        {
          g_warning ("Failed to copy profile: %s", error->message);
          g_error_free (error);
          goto out;
        }
    }
out:
  gtk_widget_destroy (dialog);
  g_free (default_name);
  if (source != NULL)
    g_object_unref (source);
  if (destination != NULL)
    g_object_unref (destination);
}

static void
gcm_prefs_calib_export_link_cb (GtkLabel *widget,
                                const gchar *url,
                                CcColorPanel *prefs)
{
  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (prefs->priv->main_window)),
                "help:gnome-help/color-howtoimport",
                GDK_CURRENT_TIME, NULL);
}

static void
gcm_prefs_profile_add_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  GPtrArray *profiles;
  CcColorPanelPrivate *priv = prefs->priv;

  /* add profiles of the right kind */
  profiles = cd_device_get_profiles (priv->current_device);
  gcm_prefs_add_profiles_suitable_for_devices (prefs, profiles);

  /* make insensitve until we have a selection */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_assign_ok"));
  gtk_widget_set_sensitive (widget, FALSE);

  /* show the dialog */
  widget = GTK_WIDGET (priv->dialog_assign);
  gtk_widget_show (widget);
  gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (priv->main_window));
  if (profiles != NULL)
    g_ptr_array_unref (profiles);
}

static void
gcm_prefs_profile_remove_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdProfile *profile;
  gboolean ret = FALSE;
  GError *error = NULL;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (priv->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* just remove it, the list store will get ::changed */
  ret = cd_device_remove_profile_sync (priv->current_device,
                                       profile,
                                       priv->cancellable,
                                       &error);
  if (!ret)
    {
      g_warning ("failed to remove profile: %s", error->message);
      g_error_free (error);
    }
}

static void
gcm_prefs_make_profile_default_cb (GObject *object,
                                   GAsyncResult *res,
                                   CcColorPanel *prefs)
{
  CdDevice *device = CD_DEVICE (object);
  gboolean ret = FALSE;
  GError *error = NULL;

  ret = cd_device_make_profile_default_finish (device,
                                               res,
                                               &error);
  if (!ret)
    {
      g_warning ("failed to set default profile on %s: %s",
                 cd_device_get_id (device),
                 error->message);
      g_error_free (error);
    }
  else
    {
      gcm_prefs_refresh_toolbar_buttons (prefs);
    }
}

static void
gcm_prefs_device_profile_enable_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (priv->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* just set it default */
  g_debug ("setting %s default on %s",
           cd_profile_get_id (profile),
           cd_device_get_id (priv->current_device));
  cd_device_make_profile_default (priv->current_device,
                                  profile,
                                  priv->cancellable,
                                  (GAsyncReadyCallback) gcm_prefs_make_profile_default_cb,
                                  prefs);
}

static void
gcm_prefs_profile_view (CcColorPanel *prefs, CdProfile *profile)
{
  GPtrArray *argv = NULL;
  guint xid;
  gboolean ret;
  GError *error = NULL;
  CcColorPanelPrivate *priv = prefs->priv;

  /* get xid */
  xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (priv->main_window)));

  /* open up gcm-viewer as a info pane */
  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_build_filename (BINDIR, "gcm-viewer", NULL));
  g_ptr_array_add (argv, g_strdup ("--profile"));
  g_ptr_array_add (argv, g_strdup (cd_profile_get_id (profile)));
  g_ptr_array_add (argv, g_strdup ("--parent-window"));
  g_ptr_array_add (argv, g_strdup_printf ("%i", xid));
  g_ptr_array_add (argv, NULL);
  ret = g_spawn_async (NULL, (gchar**) argv->pdata, NULL, 0,
                       NULL, NULL, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to run calibrate: %s", error->message);
      g_error_free (error);
    }

  g_ptr_array_unref (argv);
}

static void
gcm_prefs_profile_assign_link_activate_cb (GtkLabel *label,
                                           const gchar *uri,
                                           CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (priv->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* show it in the viewer */
  gcm_prefs_profile_view (prefs, profile);
}

static void
gcm_prefs_profile_view_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (prefs->priv->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* open up gcm-viewer as a info pane */
  gcm_prefs_profile_view (prefs, profile);
}

static void
gcm_prefs_button_assign_cancel_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  gtk_widget_hide (priv->dialog_assign);
}

static void
gcm_prefs_button_assign_ok_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  CdProfile *profile = NULL;
  gboolean ret = FALSE;
  GError *error = NULL;
  GtkTreeSelection *selection;
  CcColorPanelPrivate *priv = prefs->priv;

  /* hide window */
  widget = GTK_WIDGET (priv->dialog_assign);
  gtk_widget_hide (widget);

  /* get the selected profile */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_assign"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    goto out;
  gtk_tree_model_get (model, &iter,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
                      -1);
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        goto out;
    }

  /* if the device is disabled, enable the device so that we can
   * add color profiles to it */
  if (!cd_device_get_enabled (priv->current_device))
    {
      ret = cd_device_set_enabled_sync (priv->current_device,
                                        TRUE,
                                        priv->cancellable,
                                        &error);
      if (!ret)
        {
          g_warning ("failed to enabled device: %s", error->message);
          g_error_free (error);
          goto out;
        }
    }

  /* just add it, the list store will get ::changed */
  ret = cd_device_add_profile_sync (priv->current_device,
                                    CD_DEVICE_RELATION_HARD,
                                    profile,
                                    priv->cancellable,
                                    &error);
  if (!ret)
    {
      g_warning ("failed to add: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* make it default */
  cd_device_make_profile_default (priv->current_device,
                                  profile,
                                  priv->cancellable,
                                  (GAsyncReadyCallback) gcm_prefs_make_profile_default_cb,
                                  prefs);
out:
  if (profile != NULL)
    g_object_unref (profile);
}

static gboolean
gcm_prefs_profile_delete_event_cb (GtkWidget *widget,
                                   GdkEvent *event,
                                   CcColorPanel *prefs)
{
  gcm_prefs_button_assign_cancel_cb (widget, prefs);
  return TRUE;
}

static void
gcm_prefs_add_profiles_columns (CcColorPanel *prefs,
                                GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* text */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", GCM_PREFS_COMBO_COLUMN_TEXT);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (treeview, column);

  /* image */
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "icon-name", GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME);
  gtk_tree_view_append_column (treeview, column);
}

static void
gcm_prefs_set_calibrate_button_sensitivity (CcColorPanel *prefs)
{
  gboolean ret = FALSE;
  GtkWidget *widget;
  const gchar *tooltip;
  CdDeviceKind kind;
  CdSensor *sensor_tmp;
  CcColorPanelPrivate *priv = prefs->priv;

  /* TRANSLATORS: this is when the button is sensitive */
  tooltip = _("Create a color profile for the selected device");

  /* no device selected */
  if (priv->current_device == NULL)
    goto out;

  /* are we a display */
  kind = cd_device_get_kind (priv->current_device);
  if (kind == CD_DEVICE_KIND_DISPLAY)
    {

      /* find whether we have hardware installed */
      if (priv->sensors == NULL || priv->sensors->len == 0)
        {
          /* TRANSLATORS: this is when the button is insensitive */
          tooltip = _("The measuring instrument is not detected. Please check it is turned on and correctly connected.");
          goto out;
        }

      /* success */
      ret = TRUE;

    }
  else if (kind == CD_DEVICE_KIND_SCANNER ||
           kind == CD_DEVICE_KIND_CAMERA ||
           kind == CD_DEVICE_KIND_WEBCAM)
    {

      /* TODO: find out if we can scan using gnome-scan */
      ret = TRUE;

    }
  else if (kind == CD_DEVICE_KIND_PRINTER)
    {

    /* find whether we have hardware installed */
    if (priv->sensors == NULL || priv->sensors->len == 0)
      {
        /* TRANSLATORS: this is when the button is insensitive */
        tooltip = _("The measuring instrument is not detected. Please check it is turned on and correctly connected.");
        goto out;
      }

    /* find whether we have hardware installed */
    sensor_tmp = g_ptr_array_index (priv->sensors, 0);
    ret = cd_sensor_has_cap (sensor_tmp, CD_SENSOR_CAP_PRINTER);
    if (!ret)
      {
        /* TRANSLATORS: this is when the button is insensitive */
        tooltip = _("The measuring instrument does not support printer profiling.");
        goto out;
      }

    /* success */
    ret = TRUE;

    }
  else
    {
      /* TRANSLATORS: this is when the button is insensitive */
      tooltip = _("The device type is not currently supported.");
    }
out:
  /* control the tooltip and sensitivity of the button */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_calibrate"));
  gtk_widget_set_tooltip_text (widget, tooltip);
  gtk_widget_set_sensitive (widget, ret);
}

static void
gcm_prefs_device_clicked (CcColorPanel *prefs, CdDevice *device)
{
  /* we have a new device */
  g_debug ("selected device is: %s",
           cd_device_get_id (device));

  /* can this device calibrate */
  gcm_prefs_set_calibrate_button_sensitivity (prefs);
}

static void
gcm_prefs_profile_clicked (CcColorPanel *prefs, CdProfile *profile, CdDevice *device)
{
  GtkWidget *widget;
  gchar *s;
  CcColorPanelPrivate *priv = prefs->priv;

  /* get profile */
  g_debug ("selected profile = %s",
     cd_profile_get_filename (profile));

  /* allow getting profile info */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
               "toolbutton_profile_view"));
  if (cd_profile_get_filename (profile) != NULL &&
      (s = g_find_program_in_path ("gcm-viewer")) != NULL)
    {
      gtk_widget_set_sensitive (widget, TRUE);
      g_free (s);
    }
  else
      gtk_widget_set_sensitive (widget, FALSE);
}

static void
gcm_prefs_profiles_treeview_clicked_cb (GtkTreeSelection *selection,
                                        CcColorPanel *prefs)
{
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeIter iter;
  CdProfile *profile = NULL;
#if CD_CHECK_VERSION(0,1,25)
  gchar **warnings;
#endif

  /* get selection */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;
  gtk_tree_model_get (model, &iter,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
                      -1);


  /* as soon as anything is selected, make the Add button sensitive */
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "button_assign_ok"));
  gtk_widget_set_sensitive (widget, TRUE);

  /* is the profile faulty */
  widget = GTK_WIDGET (gtk_builder_get_object (prefs->priv->builder,
                                               "label_assign_warning"));
#if CD_CHECK_VERSION(0,1,25)
  warnings = cd_profile_get_warnings (profile);
  gtk_widget_set_visible (widget, warnings != NULL && warnings[0] != NULL);
#else
  gtk_widget_set_visible (widget, FALSE);
#endif
  g_object_unref (profile);
}

static void
gcm_prefs_profiles_row_activated_cb (GtkTreeView *tree_view,
                                     GtkTreePath *path,
                                     GtkTreeViewColumn *column,
                                     CcColorPanel *prefs)
{
  GtkTreeIter iter;
  gboolean ret;

  ret = gtk_tree_model_get_iter (gtk_tree_view_get_model (tree_view), &iter, path);
  if (!ret)
    return;
  gcm_prefs_button_assign_ok_cb (NULL, prefs);
}


static void
gcm_prefs_button_assign_import_cb (GtkWidget *widget,
                                   CcColorPanel *prefs)
{
  GFile *file = NULL;
  GError *error = NULL;
  CdProfile *profile = NULL;
  CcColorPanelPrivate *priv = prefs->priv;

  file = gcm_prefs_file_chooser_get_icc_profile (prefs);
  if (file == NULL)
    {
      g_warning ("failed to get ICC file");
      widget = GTK_WIDGET (priv->dialog_assign);
      gtk_widget_hide (widget);
      goto out;
    }

#if CD_CHECK_VERSION(0,1,12)
  profile = cd_client_import_profile_sync (priv->client,
                                           file,
                                           priv->cancellable,
                                           &error);
  if (profile == NULL)
    {
      g_warning ("failed to get imported profile: %s", error->message);
      g_error_free (error);
      goto out;
    }
#endif

  /* add to list view */
  gcm_prefs_profile_add_cb (NULL, prefs);
out:
  if (file != NULL)
    g_object_unref (file);
  if (profile != NULL)
    g_object_unref (profile);
}

static void
gcm_prefs_sensor_coldplug (CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdSensor *sensor_tmp;
  gboolean ret;
  GError *error = NULL;
  GPtrArray *sensors;
  guint i;

  /* unref old */
  g_clear_pointer (&priv->sensors, g_ptr_array_unref);

  /* no present */
  sensors = cd_client_get_sensors_sync (priv->client, NULL, &error);
  if (sensors == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }
  if (sensors->len == 0)
    goto out;

  /* save a copy of the sensor list */
  priv->sensors = g_ptr_array_ref (sensors);

  /* connect to each sensor */
  for (i = 0; i < sensors->len; i++)
    {
      sensor_tmp = g_ptr_array_index (sensors, i);
      ret = cd_sensor_connect_sync (sensor_tmp, NULL, &error);
      if (!ret)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          goto out;
        }
    }
out:
  if (sensors != NULL)
    g_ptr_array_unref (sensors);
}

static void
gcm_prefs_client_sensor_changed_cb (CdClient *client,
                                    CdSensor *sensor,
                                    CcColorPanel *prefs)
{
  gcm_prefs_sensor_coldplug (prefs);
  gcm_prefs_set_calibrate_button_sensitivity (prefs);
}

static void
gcm_prefs_add_device_profile (CcColorPanel *prefs,
                              CdDevice *device,
                              CdProfile *profile,
                              gboolean is_default)
{
  CcColorPanelPrivate *priv = prefs->priv;
  gboolean ret;
  GError *error = NULL;
  GtkWidget *widget;

  /* get properties */
  ret = cd_profile_connect_sync (profile,
                                 priv->cancellable,
                                 &error);
  if (!ret)
    {
      g_warning ("failed to get profile: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* ignore profiles from other user accounts */
  if (!cd_profile_has_access (profile))
    {
      /* only print the filename if it exists */
      if (cd_profile_get_filename (profile) != NULL)
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_filename (profile));
        }
      else
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_id (profile));
        }
      goto out;
    }

  /* add to listbox */
  widget = cc_color_profile_new (device, profile, is_default);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (priv->list_box), widget);
  gtk_size_group_add_widget (priv->list_box_size, widget);
out:
  return;
}

static void
gcm_prefs_add_device_profiles (CcColorPanel *prefs, CdDevice *device)
{
  CdProfile *profile_tmp;
  GPtrArray *profiles = NULL;
  guint i;

  /* add profiles */
  profiles = cd_device_get_profiles (device);
  if (profiles == NULL)
    goto out;
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      gcm_prefs_add_device_profile (prefs, device, profile_tmp, i == 0);
    }
out:
  if (profiles != NULL)
    g_ptr_array_unref (profiles);
}

/* find the profile in the array -- for flicker-free changes */
static gboolean
gcm_prefs_find_profile_by_object_path (GPtrArray *profiles,
                                       const gchar *object_path)
{
  CdProfile *profile_tmp;
  guint i;

  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      if (g_strcmp0 (cd_profile_get_object_path (profile_tmp), object_path) == 0)
        return TRUE;
    }
  return FALSE;
}

/* find the profile in the list view -- for flicker-free changes */
static gboolean
gcm_prefs_find_widget_by_object_path (GList *list,
                                      const gchar *object_path_device,
                                      const gchar *object_path_profile)
{
  GList *l;
  CdDevice *device_tmp;
  CdProfile *profile_tmp;

  for (l = list; l != NULL; l = l->next)
    {
      if (!CC_IS_COLOR_PROFILE (l->data))
        continue;

      /* correct device ? */
      device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_device,
                     cd_device_get_object_path (device_tmp)) != 0)
        {
          continue;
        }

      /* this profile */
      profile_tmp = cc_color_profile_get_profile (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_profile,
                     cd_profile_get_object_path (profile_tmp)) == 0)
        {
          return TRUE;
        }
    }
  return FALSE;
}

static void
gcm_prefs_device_changed_cb (CdDevice *device, CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdDevice *device_tmp;
  CdProfile *profile_tmp;
  gboolean ret;
  GList *l;
  GList *list;
  GPtrArray *profiles;
  guint i;

  /* remove anything in the list view that's not in Device.Profiles */
  profiles = cd_device_get_profiles (device);
  list = gtk_container_get_children (GTK_CONTAINER (priv->list_box));
  for (l = list; l != NULL; l = l->next)
    {
      if (!CC_IS_COLOR_PROFILE (l->data))
        continue;

      /* correct device ? */
      device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (cd_device_get_id (device),
                     cd_device_get_id (device_tmp)) != 0)
        continue;

      /* if profile is not in Device.Profiles then remove */
      profile_tmp = cc_color_profile_get_profile (CC_COLOR_PROFILE (l->data));
      ret = gcm_prefs_find_profile_by_object_path (profiles,
                                                   cd_profile_get_object_path (profile_tmp));
      if (!ret) {
        gtk_widget_destroy (GTK_WIDGET (l->data));
        /* Don't look at the destroyed widget below */
        l->data = NULL;
      }
    }

  /* add anything in Device.Profiles that's not in the list view */
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      ret = gcm_prefs_find_widget_by_object_path (list,
                                                  cd_device_get_object_path (device),
                                                  cd_profile_get_object_path (profile_tmp));
      if (!ret)
        gcm_prefs_add_device_profile (prefs, device, profile_tmp, i == 0);
    }
  g_list_free (list);

  /* resort */
  gtk_list_box_invalidate_sort (priv->list_box);
}

static void
gcm_prefs_device_expanded_changed_cb (CcColorDevice *widget,
                                      gboolean is_expanded,
                                      CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  GList *l;
  GList *list;

  /* ignore internal changes */
  if (prefs->priv->model_is_changing)
    return;

  g_free (prefs->priv->list_box_filter);
  if (is_expanded)
    {
      priv->list_box_filter = g_strdup (cd_device_get_id (cc_color_device_get_device (widget)));

      /* unexpand other device widgets */
      list = gtk_container_get_children (GTK_CONTAINER (priv->list_box));
      prefs->priv->model_is_changing = TRUE;
      for (l = list; l != NULL; l = l->next)
        {
          if (!CC_IS_COLOR_DEVICE (l->data))
            continue;
          if (l->data != widget)
            cc_color_device_set_expanded (CC_COLOR_DEVICE (l->data), FALSE);
        }
      prefs->priv->model_is_changing = FALSE;
      g_list_free (list);
    }
  else
    {
      priv->list_box_filter = NULL;
    }
  gtk_list_box_invalidate_filter (priv->list_box);
}

static void
gcm_prefs_add_device (CcColorPanel *prefs, CdDevice *device)
{
  CcColorPanelPrivate *priv = prefs->priv;
  gboolean ret;
  GError *error = NULL;
  GtkWidget *widget;

  /* get device properties */
  ret = cd_device_connect_sync (device, priv->cancellable, &error);
  if (!ret)
    {
      g_warning ("failed to connect to the device: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* add device */
  widget = cc_color_device_new (device);
  g_signal_connect (widget, "expanded-changed",
                    G_CALLBACK (gcm_prefs_device_expanded_changed_cb), prefs);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (priv->list_box), widget);
  gtk_size_group_add_widget (priv->list_box_size, widget);

  /* add profiles */
  gcm_prefs_add_device_profiles (prefs, device);

  /* watch for changes */
  g_ptr_array_add (priv->devices, g_object_ref (device));
  g_signal_connect (device, "changed",
                    G_CALLBACK (gcm_prefs_device_changed_cb), prefs);
  gtk_list_box_invalidate_sort (priv->list_box);
out:
  return;
}

static void
gcm_prefs_remove_device (CcColorPanel *prefs, CdDevice *device)
{
  CcColorPanelPrivate *priv = prefs->priv;
  CdDevice *device_tmp;
  GList *l;
  GList *list;

  list = gtk_container_get_children (GTK_CONTAINER (priv->list_box));
  for (l = list; l != NULL; l = l->next)
    {
      if (CC_IS_COLOR_DEVICE (l->data))
        device_tmp = cc_color_device_get_device (CC_COLOR_DEVICE (l->data));
      else
        device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (cd_device_get_object_path (device),
                     cd_device_get_object_path (device_tmp)) == 0)
        {
          gtk_widget_destroy (GTK_WIDGET (l->data));
        }
    }
  g_list_free (list);
  g_signal_handlers_disconnect_by_func (device,
                                        G_CALLBACK (gcm_prefs_device_changed_cb),
                                        prefs);
  g_ptr_array_remove (priv->devices, device);
}

static void
gcm_prefs_update_device_list_extra_entry (CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv = prefs->priv;
  GList *device_widgets;
  GtkWidget *widget;
  guint number_of_devices;

  /* any devices to show? */
  device_widgets = gtk_container_get_children (GTK_CONTAINER (priv->list_box));
  number_of_devices = g_list_length (device_widgets);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_no_devices"));
  gtk_widget_set_visible (widget, number_of_devices == 0);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "vbox3"));
  gtk_widget_set_visible (widget, number_of_devices > 0);

  /* if we have only one device expand it by default */
  if (number_of_devices == 1)
    cc_color_device_set_expanded (CC_COLOR_DEVICE (device_widgets->data), TRUE);
  g_list_free (device_widgets);
}

static void
gcm_prefs_device_added_cb (CdClient *client,
                           CdDevice *device,
                           CcColorPanel *prefs)
{
  /* add the device */
  gcm_prefs_add_device (prefs, device);

  /* ensure we're not showing the 'No devices detected' entry */
  gcm_prefs_update_device_list_extra_entry (prefs);
}

static void
gcm_prefs_device_removed_cb (CdClient *client,
                             CdDevice *device,
                             CcColorPanel *prefs)
{
  /* remove from the UI */
  gcm_prefs_remove_device (prefs, device);

  /* ensure we showing the 'No devices detected' entry if required */
  gcm_prefs_update_device_list_extra_entry (prefs);
}

static void
gcm_prefs_get_devices_cb (GObject *object,
                          GAsyncResult *res,
                          gpointer user_data)
{
  CcColorPanel *prefs = (CcColorPanel *) user_data;
  CdClient *client = CD_CLIENT (object);
  CdDevice *device;
  GError *error = NULL;
  GPtrArray *devices;
  guint i;

  /* get devices and add them */
  devices = cd_client_get_devices_finish (client, res, &error);
  if (devices == NULL)
    {
      g_warning ("failed to add connected devices: %s",
                 error->message);
      g_error_free (error);
      goto out;
    }
  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      gcm_prefs_add_device (prefs, device);
    }

  /* ensure we show the 'No devices detected' entry if empty */
  gcm_prefs_update_device_list_extra_entry (prefs);
out:
  if (devices != NULL)
    g_ptr_array_unref (devices);
}

static void
gcm_prefs_list_box_row_selected_cb (GtkListBox *list_box,
                                    GtkListBoxRow *row,
                                    CcColorPanel *panel)
{
  gcm_prefs_refresh_toolbar_buttons (panel);
}

static void
gcm_prefs_refresh_toolbar_buttons (CcColorPanel *panel)
{
  CdProfile *profile = NULL;
  GtkWidget *widget;
  CcColorPanelPrivate *priv = panel->priv;
  GtkListBoxRow *row;
  gboolean is_device;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (priv->list_box);

  is_device = CC_IS_COLOR_DEVICE (row);

  /* nothing selected */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbar_devices"));
  gtk_widget_set_visible (widget, row != NULL);
  if (row == NULL)
    return;

  /* save current device */
  if (priv->current_device != NULL)
    g_object_unref (priv->current_device);
  g_object_get (row, "device", &priv->current_device, NULL);

  /* device actions */
  g_debug ("%s selected", is_device ? "device" : "profile");
  if (CC_IS_COLOR_DEVICE (row))
    {
      gcm_prefs_device_clicked (panel, priv->current_device);
      cc_color_device_set_expanded (CC_COLOR_DEVICE (row), TRUE);
    }
  else if (CC_IS_COLOR_PROFILE (row))
    {
      profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
      gcm_prefs_profile_clicked (panel, profile, priv->current_device);
    }
  else
    g_assert_not_reached ();

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_default"));
  gtk_widget_set_visible (widget, !is_device && cc_color_profile_get_is_default (CC_COLOR_PROFILE (row)));
  if (profile)
    gtk_widget_set_sensitive (widget, !cd_profile_get_is_system_wide (profile));

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_enable"));
  gtk_widget_set_visible (widget, !is_device && !cc_color_profile_get_is_default (CC_COLOR_PROFILE (row)));

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_calibrate"));
  gtk_widget_set_visible (widget, is_device);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_add"));
  gtk_widget_set_visible (widget, is_device);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_view"));
  gtk_widget_set_visible (widget, !is_device);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_remove"));
  gtk_widget_set_visible (widget, !is_device);
}

static void
gcm_prefs_list_box_row_activated_cb (GtkListBox *list_box,
                                     GtkListBoxRow *row,
                                     CcColorPanel *prefs)
{
  if (CC_IS_COLOR_PROFILE (row))
    {
      gcm_prefs_device_profile_enable_cb (NULL, prefs);
    }
}

static void
gcm_prefs_connect_cb (GObject *object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  gboolean ret;
  GError *error = NULL;
  CcColorPanel *prefs = CC_COLOR_PANEL (user_data);
  CcColorPanelPrivate *priv = prefs->priv;

  ret = cd_client_connect_finish (priv->client,
                                  res,
                                  &error);
  if (!ret)
    {
      g_warning ("failed to connect to colord: %s", error->message);
      g_error_free (error);
      return;
    }

  /* set calibrate button sensitivity */
  gcm_prefs_sensor_coldplug (prefs);


  /* get devices */
  cd_client_get_devices (priv->client,
                         priv->cancellable,
                         gcm_prefs_get_devices_cb,
                         prefs);
}

static gboolean
gcm_prefs_is_livecd (void)
{
#ifdef __linux__
  gboolean ret = TRUE;
  gchar *data = NULL;
  GError *error = NULL;

  /* allow testing */
  if (g_getenv ("CC_COLOR_PANEL_IS_LIVECD") != NULL)
    goto out;

  /* get the kernel commandline */
  ret = g_file_get_contents ("/proc/cmdline", &data, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to get kernel command line: %s",
      error->message);
      g_error_free (error);
      goto out;
    }
  ret = (g_strstr_len (data, -1, "liveimg") != NULL ||
         g_strstr_len (data, -1, "casper") != NULL);
out:
  g_free (data);
  return ret;
#else
  return FALSE;
#endif
}

static void
gcm_prefs_window_realize_cb (GtkWidget *widget, CcColorPanel *prefs)
{
  prefs->priv->main_window = gtk_widget_get_toplevel (widget);
}

static const char *
cc_color_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/color";
}

static void
cc_color_panel_get_property (GObject    *object,
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
cc_color_panel_set_property (GObject      *object,
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
cc_color_panel_dispose (GObject *object)
{
  CcColorPanelPrivate *priv = CC_COLOR_PANEL (object)->priv;
  CdDevice *device;
  guint i;

  /* stop the EggListView from firing when it gets disposed */
  if (priv->list_box_selected_id != 0)
    {
      g_signal_handler_disconnect (priv->list_box,
                                   priv->list_box_selected_id);
      priv->list_box_selected_id = 0;

      /* row-activated event should be connected at this point */
      g_signal_handler_disconnect (priv->list_box,
                                   priv->list_box_activated_id);
      priv->list_box_activated_id = 0;
    }

  /* stop the devices from emitting after the ListBox has been disposed */
  if (priv->devices != NULL)
    {
      for (i = 0; i < priv->devices->len; i++)
        {
          device = g_ptr_array_index (priv->devices, i);
          g_signal_handlers_disconnect_by_func (device,
                                                G_CALLBACK (gcm_prefs_device_changed_cb),
                                                CC_COLOR_PANEL (object));
        }
      g_ptr_array_unref (priv->devices);
      priv->devices = NULL;
    }

  if (priv->cancellable != NULL)
    g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->settings);
  g_clear_object (&priv->settings_colord);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->builder);
  g_clear_object (&priv->client);
  g_clear_object (&priv->current_device);
  g_clear_object (&priv->calibrate);
  g_clear_object (&priv->list_box_size);
  g_clear_pointer (&priv->sensors, g_ptr_array_unref);
  g_clear_pointer (&priv->list_box_filter, g_free);
  g_clear_pointer (&priv->dialog_assign, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_color_panel_parent_class)->dispose (object);
}

static void
cc_color_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_color_panel_parent_class)->finalize (object);
}

static void
cc_color_panel_class_init (CcColorPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcColorPanelPrivate));

  panel_class->get_help_uri = cc_color_panel_get_help_uri;

  object_class->get_property = cc_color_panel_get_property;
  object_class->set_property = cc_color_panel_set_property;
  object_class->dispose = cc_color_panel_dispose;
  object_class->finalize = cc_color_panel_finalize;
}

static gint
cc_color_panel_sort_func (GtkListBoxRow *a,
                          GtkListBoxRow *b,
                          gpointer user_data)
{
  const gchar *sort_a = NULL;
  const gchar *sort_b = NULL;
  if (CC_IS_COLOR_DEVICE (a))
    sort_a = cc_color_device_get_sortable (CC_COLOR_DEVICE (a));
  else if (CC_IS_COLOR_PROFILE (a))
    sort_a = cc_color_profile_get_sortable (CC_COLOR_PROFILE (a));
  else
    g_assert_not_reached ();
  if (CC_IS_COLOR_DEVICE (b))
    sort_b = cc_color_device_get_sortable (CC_COLOR_DEVICE (b));
  else if (CC_IS_COLOR_PROFILE (b))
    sort_b = cc_color_profile_get_sortable (CC_COLOR_PROFILE (b));
  else
    g_assert_not_reached ();
  return g_strcmp0 (sort_b, sort_a);
}

static gboolean
cc_color_panel_filter_func (GtkListBoxRow *row, void *user_data)
{
  CcColorPanel *prefs = CC_COLOR_PANEL (user_data);
  CdDevice *device;
  gboolean ret;

  /* always show all devices */
  if (CC_IS_COLOR_DEVICE (row))
    return TRUE;

  g_object_get (row, "device", &device, NULL);
  ret = g_strcmp0 (cd_device_get_id (device), prefs->priv->list_box_filter) == 0;
  g_object_unref (device);
  return ret;
}

static gboolean
cc_color_panel_treeview_quality_default_cb (GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer data)
{
  CdProfileQuality quality;
  GtkTreeSelection *selection = GTK_TREE_SELECTION (data);

  gtk_tree_model_get (model, iter,
                      COLUMN_CALIB_QUALITY_VALUE, &quality,
                      -1);
  if (quality == CD_PROFILE_QUALITY_MEDIUM)
    gtk_tree_selection_select_iter (selection, iter);
  return FALSE;
}

static void
cc_color_panel_init (CcColorPanel *prefs)
{
  CcColorPanelPrivate *priv;
  GError *error = NULL;
  GtkCellRenderer *renderer;
  GtkStyleContext *context;
  GtkTreeModel *model;
  GtkTreeModel *model_filter;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkWidget *widget;

  priv = prefs->priv = COLOR_PANEL_PRIVATE (prefs);
  g_resources_register (cc_color_get_resource ());

  priv->builder = gtk_builder_new ();
  gtk_builder_add_from_resource (priv->builder,
                                 "/org/gnome/control-center/color/color.ui",
                                 &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  priv->cancellable = g_cancellable_new ();
  priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  /* can do native display calibration using colord-session */
  priv->calibrate = cc_color_calibrate_new ();
  cc_color_calibrate_set_quality (priv->calibrate, CD_PROFILE_QUALITY_MEDIUM);

  /* setup defaults */
  priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
  priv->settings_colord = g_settings_new (COLORD_SETTINGS_SCHEMA);

  /* assign buttons */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_add"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_profile_add_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_remove"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_profile_remove_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_profile_view"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_profile_view_cb), prefs);

  /* href */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_assign_warning"));
  g_signal_connect (widget, "activate-link",
                    G_CALLBACK (gcm_prefs_profile_assign_link_activate_cb), prefs);

  /* add columns to profile tree view */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_assign"));
  gcm_prefs_add_profiles_columns (prefs, GTK_TREE_VIEW (widget));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (gcm_prefs_profiles_treeview_clicked_cb),
                    prefs);
  g_signal_connect (GTK_TREE_VIEW (widget), "row-activated",
                    G_CALLBACK (gcm_prefs_profiles_row_activated_cb),
                    prefs);

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_default"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_default_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_enable"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_device_profile_enable_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbutton_device_calibrate"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_calibrate_cb), prefs);

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "toolbar_devices"));
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  /* set up assign dialog */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "dialog_assign"));
  g_signal_connect (widget, "delete-event",
                    G_CALLBACK (gcm_prefs_profile_delete_event_cb), prefs);
  priv->dialog_assign = widget;

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_assign_cancel"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_button_assign_cancel_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_assign_ok"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_button_assign_ok_cb), prefs);

  /* setup icc profiles list */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_assign_import"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_button_assign_import_cb), prefs);

  /* setup the calibration helper */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "assistant_calib"));
  g_signal_connect (widget, "delete-event",
                    G_CALLBACK (gcm_prefs_calib_delete_event_cb),
                    prefs);
  g_signal_connect (widget, "apply",
                    G_CALLBACK (gcm_prefs_calib_apply_cb),
                    prefs);
  g_signal_connect (widget, "cancel",
                    G_CALLBACK (gcm_prefs_calib_cancel_cb),
                    prefs);
  g_signal_connect (widget, "close",
                    G_CALLBACK (gcm_prefs_calib_cancel_cb),
                    prefs);
  g_signal_connect (widget, "prepare",
                    G_CALLBACK (gcm_prefs_calib_prepare_cb),
                    prefs);
  priv->assistant_calib = widget;

  /* setup the calibration helper ::TreeView */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_calib_quality"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  gtk_tree_model_foreach (model,
                          cc_color_panel_treeview_quality_default_cb,
                          selection);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (gcm_prefs_calib_quality_treeview_clicked_cb),
                    prefs);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_QUALITY_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (widget),
                               GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new ();
  renderer = cc_color_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                "is-dim-label", TRUE,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_QUALITY_APPROX_TIME);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (widget),
                               GTK_TREE_VIEW_COLUMN (column));

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_calib_sensor"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (gcm_prefs_calib_sensor_treeview_clicked_cb),
                    prefs);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_SENSOR_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (widget),
                               GTK_TREE_VIEW_COLUMN (column));

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_calib_kind"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (gcm_prefs_calib_kind_treeview_clicked_cb),
                    prefs);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_KIND_DESCRIPTION);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  model_filter = gtk_tree_model_filter_new (model, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (widget), model_filter);
  gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (model_filter),
                                            COLUMN_CALIB_KIND_VISIBLE);

  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (widget),
                               GTK_TREE_VIEW_COLUMN (column));

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "treeview_calib_temp"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (gcm_prefs_calib_temp_treeview_clicked_cb),
                    prefs);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_TEMP_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (widget),
                               GTK_TREE_VIEW_COLUMN (column));
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "entry_calib_title"));
  g_signal_connect (widget, "notify::text",
        G_CALLBACK (gcm_prefs_title_entry_changed_cb), prefs);

  /* use a device client array */
  priv->client = cd_client_new ();
  g_signal_connect_object (priv->client, "device-added",
                           G_CALLBACK (gcm_prefs_device_added_cb), prefs, 0);
  g_signal_connect_object (priv->client, "device-removed",
                           G_CALLBACK (gcm_prefs_device_removed_cb), prefs, 0);

  /* use a listbox for the main UI */
  priv->list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_filter_func (priv->list_box,
                                cc_color_panel_filter_func,
                                prefs,
                                NULL);
  gtk_list_box_set_sort_func (priv->list_box,
                              cc_color_panel_sort_func,
                              prefs,
                              NULL);
  gtk_list_box_set_header_func (priv->list_box,
                                cc_list_box_update_header_func,
                                prefs, NULL);
  gtk_list_box_set_selection_mode (priv->list_box,
                                   GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (priv->list_box, FALSE);
  priv->list_box_selected_id =
    g_signal_connect (priv->list_box, "row-selected",
                      G_CALLBACK (gcm_prefs_list_box_row_selected_cb),
                      prefs);
  priv->list_box_activated_id =
    g_signal_connect (priv->list_box, "row-activated",
                    G_CALLBACK (gcm_prefs_list_box_row_activated_cb),
                    prefs);
  priv->list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "frame_devices"));
  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->list_box));
  gtk_widget_show (GTK_WIDGET (priv->list_box));

  /* connect to colord */
  cd_client_connect (priv->client,
                     priv->cancellable,
                     gcm_prefs_connect_cb,
                     prefs);

  /* use the color sensor */
  g_signal_connect_object (priv->client, "sensor-added",
                           G_CALLBACK (gcm_prefs_client_sensor_changed_cb),
                           prefs, 0);
  g_signal_connect_object (priv->client, "sensor-removed",
                           G_CALLBACK (gcm_prefs_client_sensor_changed_cb),
                           prefs, 0);

  /* set calibrate button sensitivity */
  gcm_prefs_set_calibrate_button_sensitivity (prefs);

  /* show the confirmation export page if we are running from a LiveCD */
  priv->is_live_cd = gcm_prefs_is_livecd ();
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_calib_summary"));
  gtk_widget_set_visible (widget, priv->is_live_cd);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_calib_export"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_calib_export_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "button_calib_upload"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (gcm_prefs_calib_upload_cb), prefs);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_calib_summary_message"));
  g_signal_connect (widget, "activate-link",
                    G_CALLBACK (gcm_prefs_calib_export_link_cb), prefs);


  widget = WID (priv->builder, "dialog-vbox1");
  gtk_container_add (GTK_CONTAINER (prefs), widget);
  g_signal_connect (widget, "realize",
                    G_CALLBACK (gcm_prefs_window_realize_cb),
                    prefs);
}
