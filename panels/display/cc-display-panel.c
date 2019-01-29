/*
 * Copyright (C) 2007, 2008  Red Hat, Inc.
 * Copyright (C) 2013 Intel, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "cc-display-panel.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <gdesktop-enums.h>
#include <math.h>

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
#include <libupower-glib/upower.h>

#include "cc-display-config-manager-dbus.h"
#include "cc-display-config.h"
#include "cc-display-arrangement.h"
#include "cc-night-light-dialog.h"
#include "cc-display-resources.h"
#include "cc-display-settings.h"

/* The minimum supported size for the panel */
#define MINIMUM_WIDTH 740
#define MINIMUM_HEIGHT 530

#define PANEL_PADDING   32
#define SECTION_PADDING 32
#define HEADING_PADDING 12

enum
{
  DISPLAY_MODE_PRIMARY,
  DISPLAY_MODE_SECONDARY,
  /* DISPLAY_MODE_PRESENTATION, */
  DISPLAY_MODE_MIRROR,
  DISPLAY_MODE_OFF
};

struct _CcDisplayPanel
{
  CcPanel parent_instance;

  CcDisplayConfigManager *manager;
  CcDisplayConfig *current_config;
  CcDisplayMonitor *current_output;

  CcDisplayArrangement *arrangement;
  CcDisplaySettings    *settings;

  guint           focus_id;

  GtkSizeGroup *main_size_group;
  GtkSizeGroup *rows_size_group;
  GtkWidget *stack;

  CcNightLightDialog *night_light_dialog;
  GSettings *settings_color;

  UpClient *up_client;
  gboolean lid_is_closed;

  GDBusProxy *shell_proxy;
  GCancellable *shell_cancellable;

  guint       sensor_watch_id;
  GDBusProxy *iio_sensor_proxy;
  gboolean    has_accelerometer;

  gchar     *main_title;
  GtkWidget *main_titlebar;
  GtkWidget *apply_titlebar;
  GtkWidget *apply_titlebar_apply;
  GtkWidget *apply_titlebar_warning;
};

CC_PANEL_REGISTER (CcDisplayPanel, cc_display_panel)

typedef struct
{
  int grab_x;
  int grab_y;
  int output_x;
  int output_y;
} GrabInfo;

enum
{
  CURRENT_OUTPUT,
  LAST_PANEL_SIGNAL
};
static guint panel_signals[LAST_PANEL_SIGNAL] = { 0 };

static GtkWidget*
make_night_light_widget (CcDisplayPanel *panel);
static void
update_apply_button (CcDisplayPanel *panel);
static void
apply_current_configuration (CcDisplayPanel *self);
static void
reset_current_config (CcDisplayPanel *panel);

static void
monitor_labeler_hide (CcDisplayPanel *self)
{
  if (!self->shell_proxy)
    return;

  g_dbus_proxy_call (self->shell_proxy,
                     "HideMonitorLabels",
                     NULL, G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
monitor_labeler_show (CcDisplayPanel *self)
{
  GList *outputs, *l;
  GVariantBuilder builder;
  gint number = 0;

  if (!self->shell_proxy || !self->current_config)
    return;

  outputs = cc_display_config_get_ui_sorted_monitors (self->current_config);
  if (!outputs)
    return;

  if (cc_display_config_is_cloning (self->current_config))
    return monitor_labeler_hide (self);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY);

  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      number = cc_display_monitor_get_ui_number (output);
      if (number == 0)
        continue;

      g_variant_builder_add (&builder, "{sv}",
                             cc_display_monitor_get_connector_name (output),
                             g_variant_new_int32 (number));
    }

  g_variant_builder_close (&builder);

  if (number < 2)
    return monitor_labeler_hide (self);

  g_dbus_proxy_call (self->shell_proxy,
                     "ShowMonitorLabels2",
                     g_variant_builder_end (&builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
ensure_monitor_labels (CcDisplayPanel *self)
{
  g_autoptr(GList) windows;
  GList *w;

  windows = gtk_window_list_toplevels ();

  for (w = windows; w; w = w->next)
    {
      if (gtk_window_has_toplevel_focus (GTK_WINDOW (w->data)))
        {
          monitor_labeler_show (self);
          break;
        }
    }

  if (!w)
    monitor_labeler_hide (self);
}

static void
dialog_toplevel_focus_changed (CcDisplayPanel *self)
{
  ensure_monitor_labels (self);
}

static void
reset_titlebar (CcDisplayPanel *self)
{
  GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  if (self->main_titlebar)
    {
      gtk_window_set_titlebar (GTK_WINDOW (toplevel), self->main_titlebar);
      g_clear_object (&self->main_titlebar);

      /* The split header bar will not reset the window title, so do that here. */
      gtk_window_set_title (GTK_WINDOW (toplevel), self->main_title);
      g_clear_pointer (&self->main_title, g_free);
    }

  g_clear_object (&self->apply_titlebar);
  g_clear_object (&self->apply_titlebar_apply);
  g_clear_object (&self->apply_titlebar_warning);
}

static void
active_panel_changed (CcShell    *shell,
                      GParamSpec *pspec,
                      CcPanel    *self)
{
  g_autoptr(CcPanel) panel = NULL;

  g_object_get (shell, "active-panel", &panel, NULL);
  if (panel != self)
    reset_titlebar (CC_DISPLAY_PANEL (self));
}

static void
cc_display_panel_dispose (GObject *object)
{
  CcDisplayPanel *self = CC_DISPLAY_PANEL (object);
  CcShell *shell;
  GtkWidget *toplevel;

  reset_titlebar (CC_DISPLAY_PANEL (object));

  if (self->sensor_watch_id > 0)
    {
      g_bus_unwatch_name (self->sensor_watch_id);
      self->sensor_watch_id = 0;
    }

  g_clear_object (&self->iio_sensor_proxy);

  if (self->focus_id)
    {
      shell = cc_panel_get_shell (CC_PANEL (object));
      toplevel = cc_shell_get_toplevel (shell);
      if (toplevel != NULL)
        g_signal_handler_disconnect (G_OBJECT (toplevel),
                                     self->focus_id);
      self->focus_id = 0;
      monitor_labeler_hide (CC_DISPLAY_PANEL (object));
    }

  g_clear_object (&self->manager);
  g_clear_object (&self->current_config);
  g_clear_object (&self->up_client);
  g_clear_object (&self->settings_color);
  g_clear_object (&self->main_size_group);

  g_cancellable_cancel (self->shell_cancellable);
  g_clear_object (&self->shell_cancellable);
  g_clear_object (&self->shell_proxy);

  g_clear_pointer ((GtkWidget **) &self->night_light_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
on_monitor_settings_updated_cb (CcDisplayPanel    *panel,
                                CcDisplayMonitor  *monitor,
                                CcDisplaySettings *settings)
{
  if (monitor)
    cc_display_config_snap_output (panel->current_config, monitor);
  update_apply_button (panel);
}

static void
cc_display_panel_constructed (GObject *object)
{
  g_signal_connect_object (cc_panel_get_shell (CC_PANEL (object)), "notify::active-panel",
                           G_CALLBACK (active_panel_changed), object, 0);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->constructed (object);
}

static const char *
cc_display_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-display";
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_display_panel_get_help_uri;

  object_class->constructed = cc_display_panel_constructed;
  object_class->dispose = cc_display_panel_dispose;

  panel_signals[CURRENT_OUTPUT] =
    g_signal_new ("current-output",
                  CC_TYPE_DISPLAY_PANEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
set_current_output (CcDisplayPanel   *panel,
                    CcDisplayMonitor *output)
{
  panel->current_output = output;
  g_signal_emit (panel, panel_signals[CURRENT_OUTPUT], 0);
}

static GtkWidget *
make_bin (void)
{
  return g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL);
}

static GtkWidget *
wrap_in_boxes (GtkWidget *widget)
{
  GtkWidget *box, *bin;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, PANEL_PADDING);
  bin = make_bin ();
  gtk_widget_show (bin);
  gtk_box_pack_start (GTK_BOX (box), bin, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
  bin = make_bin ();
  gtk_widget_show (bin);
  gtk_box_pack_start (GTK_BOX (box), bin, TRUE, TRUE, 0);
  return box;
}

static GtkWidget *
make_scrollable (GtkWidget *widget)
{
  GtkWidget *sw, *box;
  sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                     "hscrollbar-policy", GTK_POLICY_NEVER,
                     "min-content-height", 450,
                     "propagate-natural-height", TRUE,
                     NULL);
  box = wrap_in_boxes (widget);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (sw), box);
  return sw;
}

static GtkWidget *
make_bold_label (const gchar *text)
{
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkWidget *label = gtk_label_new (text);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "label { font-weight: bold; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (label),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  return label;
}

static GtkWidget *
make_main_vbox (GtkSizeGroup *size_group)
{
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (vbox, PANEL_PADDING);
  gtk_widget_set_margin_bottom (vbox, PANEL_PADDING);

  if (size_group)
    gtk_size_group_add_widget (size_group, vbox);

  return vbox;
}

static GtkWidget *
make_row (GtkSizeGroup *size_group,
          GtkWidget    *start_widget,
          GtkWidget    *end_widget)
{
  GtkWidget *row, *box;

  row = g_object_new (CC_TYPE_LIST_BOX_ROW, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_widget_show (box);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);

  if (start_widget)
    {
      gtk_widget_set_halign (start_widget, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (box), start_widget, FALSE, FALSE, 0);
    }
  if (end_widget)
    {
      gtk_widget_set_halign (end_widget, GTK_ALIGN_END);
      gtk_box_pack_end (GTK_BOX (box), end_widget, FALSE, FALSE, 0);
    }

  gtk_container_add (GTK_CONTAINER (row), box);

  if (size_group)
    gtk_size_group_add_widget (size_group, row);

  return row;
}

static GtkWidget *
make_frame (const gchar *title, const gchar *subtitle)
{
  GtkWidget *frame;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

  if (title)
    {
      GtkWidget *vbox, *label;

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, HEADING_PADDING/2);
      gtk_widget_show (vbox);
      gtk_widget_set_margin_bottom (vbox, HEADING_PADDING);

      label = make_bold_label (title);
      gtk_widget_show (label);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_container_add (GTK_CONTAINER (vbox), label);

      if (subtitle)
        {
          label = gtk_label_new (subtitle);
          gtk_widget_show (label);
          gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
          gtk_label_set_xalign (GTK_LABEL (label), 0.0);
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_container_add (GTK_CONTAINER (vbox), label);
          gtk_style_context_add_class (gtk_widget_get_style_context (label),
                                       GTK_STYLE_CLASS_DIM_LABEL);
        }

      gtk_frame_set_label_widget (GTK_FRAME (frame), vbox);
      gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 1.0);
    }

  return frame;
}

static GtkWidget *
make_list_box (void)
{
  GtkWidget *listbox;

  listbox = g_object_new (CC_TYPE_LIST_BOX, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (listbox),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  return listbox;
}

static void
make_list_transparent (GtkWidget *listbox)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "list { border-style: none; background-color: transparent; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (listbox),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static GtkWidget *
make_list_popover (GtkWidget *listbox)
{
  GtkWidget *popover = g_object_new (GTK_TYPE_POPOVER,
                                     "position", GTK_POS_BOTTOM,
                                     NULL);
  GtkWidget *sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                "hscrollbar-policy", GTK_POLICY_NEVER,
                                "max-content-height", 400,
                                "propagate-natural-height", TRUE,
                                NULL);
  gtk_widget_show (sw);
  make_list_transparent (listbox);
  gtk_container_add (GTK_CONTAINER (sw), listbox);

  gtk_container_add (GTK_CONTAINER (popover), sw);
  g_signal_connect_object (listbox, "row-activated", G_CALLBACK (gtk_widget_hide),
                           popover, G_CONNECT_SWAPPED);
  return popover;
}

static GtkWidget *
make_popover_label (const gchar *text)
{
  return g_object_new (GTK_TYPE_LABEL,
                       "label", text,
                       "margin", 12,
                       "xalign", 0.0,
                       "width-chars", 20,
                       "max-width-chars", 20,
                       NULL);
}

static void
udpate_display_settings (GtkWidget      *frame,
                         CcDisplayPanel *panel)
{
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
}

static GtkWidget *
make_single_output_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *frame, *ui;

  panel->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = make_main_vbox (panel->main_size_group);
  gtk_widget_show (vbox);

  frame = make_frame (cc_display_monitor_get_ui_name (panel->current_output), NULL);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  panel->settings = cc_display_settings_new ();
  cc_display_settings_set_config (panel->settings, panel->current_config);
  cc_display_settings_set_has_accelerometer (panel->settings, panel->has_accelerometer);
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
  gtk_widget_show (GTK_WIDGET (panel->settings));
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (panel->settings));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (udpate_display_settings),
                           frame, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), panel,
                           G_CONNECT_SWAPPED);

  ui = make_night_light_widget (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  g_clear_object (&panel->rows_size_group);
  return make_scrollable (vbox);
}

static void
arrangement_notify_selected_ouptut_cb (CcDisplayPanel       *panel,
				       GParamSpec           *pspec,
				       CcDisplayArrangement *arr)
{
  CcDisplayMonitor *output = cc_display_arrangement_get_selected_output (arr);

  if (output && output != panel->current_output)
    set_current_output (panel, output);
}

static void
arrangement_update_selected_output (CcDisplayArrangement *arr,
				    CcDisplayPanel       *panel)
{
  cc_display_arrangement_set_selected_output (arr, panel->current_output);
}

static GtkWidget *
make_arrangement_row (CcDisplayPanel *panel)
{
  GtkWidget *row;
  CcDisplayArrangement *arr;

  arr = cc_display_arrangement_new (panel->current_config);
  gtk_widget_show (GTK_WIDGET (arr));
  cc_display_arrangement_set_selected_output (arr, panel->current_output);
  g_signal_connect_object (arr, "updated",
			   G_CALLBACK (update_apply_button), panel,
			   G_CONNECT_SWAPPED);
  g_signal_connect_object (arr, "notify::selected-output",
			   G_CALLBACK (arrangement_notify_selected_ouptut_cb), panel,
			   G_CONNECT_SWAPPED);
  g_signal_connect_object (panel, "current-output",
			   G_CALLBACK (arrangement_update_selected_output), arr,
			   G_CONNECT_SWAPPED);

  gtk_widget_set_size_request (GTK_WIDGET (arr), 400, 175);

  row = g_object_new (CC_TYPE_LIST_BOX_ROW, NULL);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (arr));

  return row;
}

static void
primary_chooser_sync (GtkPopover      *popover,
                      CcDisplayConfig *config)
{
  GtkWidget *label;
  GList *outputs, *l;

  label = gtk_popover_get_relative_to (popover);
  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      if (cc_display_monitor_is_primary (output))
        {
          const gchar *text = cc_display_monitor_get_ui_number_name (output);
          gtk_label_set_text (GTK_LABEL (label), text);
          return;
        }
    }
}

static void
primary_chooser_row_activated (CcDisplayPanel *panel,
                               GtkListBoxRow  *row)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (row), "output");

  cc_display_monitor_set_primary (output, TRUE);
  update_apply_button (panel);
}

static GtkWidget *
make_primary_chooser_popover (CcDisplayPanel *panel)
{
  GtkWidget *listbox;
  GList *outputs, *l;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);

  listbox = make_list_box ();
  gtk_widget_show (listbox);
  g_object_set (listbox, "margin", 12, NULL);

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *label, *row;
      const gchar *text;

      if (!cc_display_monitor_is_useful (output))
        continue;

      text = cc_display_monitor_get_ui_number_name (output);
      label = make_popover_label (text);
      gtk_widget_show (label);
      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", label,
                          NULL);
      gtk_widget_show (row);
      g_object_set_data (G_OBJECT (row), "output", output);

      g_signal_connect_object (row, "activated", G_CALLBACK (primary_chooser_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  return make_list_popover (listbox);
}

static GtkWidget *
make_primary_chooser_row (CcDisplayPanel *panel)
{
  GtkWidget *row, *label, *heading_label, *popover;

  label = gtk_label_new (NULL);
  gtk_widget_show (label);
  popover = make_primary_chooser_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  heading_label = gtk_label_new (_("Primary Display"));
  gtk_widget_show (heading_label);
  row = make_row (panel->rows_size_group, heading_label, label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->current_config, "primary", G_CALLBACK (primary_chooser_sync),
                           popover, G_CONNECT_SWAPPED);
  primary_chooser_sync (GTK_POPOVER (popover), panel->current_config);

  return row;
}

static GtkWidget *
make_arrangement_ui (CcDisplayPanel *panel)
{
  GtkWidget *frame, *listbox, *row;

  frame = make_frame (_("Display Arrangement"),
                      _("Drag displays to match your setup. The top bar is placed on the primary display."));
  listbox = make_list_box ();
  gtk_widget_show (listbox);
  gtk_container_add (GTK_CONTAINER (frame), listbox);

  row = make_arrangement_row (panel);
  gtk_widget_show (row);
  gtk_container_add (GTK_CONTAINER (listbox), row);

  row = make_primary_chooser_row (panel);
  gtk_widget_show (row);
  gtk_container_add (GTK_CONTAINER (listbox), row);

  return frame;
}

static void
two_output_chooser_active (CcDisplayPanel *panel,
                           GParamSpec     *pspec,
                           GtkWidget      *button)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    set_current_output (panel, output);
}

static void
two_output_chooser_sync (GtkWidget      *box,
                         CcDisplayPanel *panel)
{
  g_autoptr(GList) children = NULL;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (l = children; l; l = l->next)
    {
      GtkWidget *button = l->data;
      CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");
      if (panel->current_output == output)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
}

static GtkWidget *
make_two_output_chooser (CcDisplayPanel *panel)
{
  GtkWidget *box;
  GtkRadioButton *group;
  GList *outputs, *l;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);

  box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_EXPAND);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

  group = NULL;
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *button = gtk_radio_button_new_from_widget (group);

      gtk_widget_show (button);
      gtk_button_set_label (GTK_BUTTON (button), cc_display_monitor_get_ui_name (output));
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

      g_object_set_data (G_OBJECT (button), "output", output);
      g_signal_connect_object (button, "notify::active", G_CALLBACK (two_output_chooser_active),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (box), button);
      group = GTK_RADIO_BUTTON (button);
    }

  g_signal_connect_object (panel, "current-output", G_CALLBACK (two_output_chooser_sync),
                           box, G_CONNECT_SWAPPED);
  two_output_chooser_sync (box, panel);

  return box;
}

static GtkWidget *
make_two_join_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *ui, *frame, *box;

  panel->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  ui = make_arrangement_ui (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  box = make_two_output_chooser (panel);
  gtk_widget_show (box);
  gtk_widget_set_margin_top (box, SECTION_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  frame = make_frame (NULL, NULL);
  gtk_widget_show (frame);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  panel->settings = cc_display_settings_new ();
  cc_display_settings_set_config (panel->settings, panel->current_config);
  cc_display_settings_set_has_accelerometer (panel->settings, panel->has_accelerometer);
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
  gtk_widget_show (GTK_WIDGET (panel->settings));
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (panel->settings));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (udpate_display_settings),
                           frame, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), panel,
                           G_CONNECT_SWAPPED);

  ui = make_night_light_widget (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  g_clear_object (&panel->rows_size_group);
  return vbox;
}

static void
two_output_chooser_activate_output (CcDisplayPanel *panel,
                                    GParamSpec     *pspec,
                                    GtkWidget      *button)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      GList *outputs, *l;

      cc_display_monitor_set_active (output, TRUE);

      outputs = cc_display_config_get_monitors (panel->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *other = l->data;
          if (other != output)
            cc_display_monitor_set_active (other, FALSE);
        }

      update_apply_button (panel);
    }
}

static void
connect_activate_output (GtkWidget *button,
                         gpointer   panel)
{
  g_signal_connect_object (button, "notify::active", G_CALLBACK (two_output_chooser_activate_output),
                           panel, G_CONNECT_SWAPPED);
}

static GtkWidget *
make_two_single_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *frame, *ui, *box;

  panel->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  box = make_two_output_chooser (panel);
  gtk_widget_show (box);
  gtk_container_foreach (GTK_CONTAINER (box), connect_activate_output, panel);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  frame = make_frame (NULL, NULL);
  gtk_widget_show (frame);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  panel->settings = cc_display_settings_new ();
  cc_display_settings_set_config (panel->settings, panel->current_config);
  cc_display_settings_set_has_accelerometer (panel->settings, panel->has_accelerometer);
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
  gtk_widget_show (GTK_WIDGET (panel->settings));
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (panel->settings));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (udpate_display_settings),
                           frame, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), panel,
                           G_CONNECT_SWAPPED);

  ui = make_night_light_widget (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  g_clear_object (&panel->rows_size_group);
  return vbox;
}

static GtkWidget *
make_two_mirror_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *frame, *ui;

  if (!cc_display_config_is_cloning (panel->current_config))
    {
      GList *modes = cc_display_config_get_cloning_modes (panel->current_config);
      gint bw, bh;
      CcDisplayMode *best = NULL;

      while (modes)
        {
          CcDisplayMode *mode = modes->data;
          gint w, h;

          cc_display_mode_get_resolution (mode, &w, &h);
          if (best == NULL || (bw*bh < w*h))
            {
              best = mode;
              cc_display_mode_get_resolution (best, &bw, &bh);
            }

          modes = modes->next;
        }
      cc_display_config_set_cloning (panel->current_config, TRUE);
      cc_display_config_set_mode_on_all_outputs (panel->current_config, best);
    }

  panel->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  frame = make_frame (NULL, NULL);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  panel->settings = cc_display_settings_new ();
  cc_display_settings_set_config (panel->settings, panel->current_config);
  cc_display_settings_set_has_accelerometer (panel->settings, panel->has_accelerometer);
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
  gtk_widget_show (GTK_WIDGET (panel->settings));
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (panel->settings));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (udpate_display_settings),
                           frame, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), panel,
                           G_CONNECT_SWAPPED);

  ui = make_night_light_widget (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  g_clear_object (&panel->rows_size_group);
  return vbox;
}

static void
two_output_visible_child_changed (CcDisplayPanel *panel,
                                  GParamSpec     *pspec,
                                  GtkWidget      *stack)
{
  GtkWidget *bin, *ui;
  g_autoptr(GList) children = NULL;
  GList *l;

  reset_current_config (panel);

  children = gtk_container_get_children (GTK_CONTAINER (stack));
  for (l = children; l; l = l->next)
    {
      GtkWidget *ui = gtk_bin_get_child (GTK_BIN (l->data));
      if (ui)
        gtk_widget_destroy (ui);
    }

  bin = gtk_stack_get_visible_child (GTK_STACK (stack));

  if (g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (stack)), "mirror"))
    {
      ui = make_two_mirror_ui (panel);
    }
  else
    {
      gboolean single;
      GList *outputs, *l;

      if (cc_display_config_is_cloning (panel->current_config))
        {
          cc_display_config_set_cloning (panel->current_config, FALSE);
        }
      single = g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (stack)), "single");
      outputs = cc_display_config_get_monitors (panel->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;
          cc_display_monitor_set_active (output, (!single || output == panel->current_output));
        }

      if (single)
        ui = make_two_single_ui (panel);
      else
        ui = make_two_join_ui (panel);
    }

  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (bin), ui);

  ensure_monitor_labels (panel);
  update_apply_button (panel);
}

static gboolean
transform_stack_to_button (GBinding     *binding,
                           const GValue *from_value,
                           GValue       *to_value,
                           gpointer      user_data)
{
  GtkWidget *visible_child = g_value_get_object (from_value);
  GtkWidget *button_child = user_data;

  g_value_set_boolean (to_value, visible_child == button_child);
  return TRUE;
}

static gboolean
transform_button_to_stack (GBinding     *binding,
                           const GValue *from_value,
                           GValue       *to_value,
                           gpointer      user_data)
{
  GtkWidget *button_child = user_data;

  if (g_value_get_boolean (from_value))
    g_value_set_object (to_value, button_child);
  return TRUE;
}

static void
add_two_output_page (GtkWidget   *switcher,
                     GtkWidget   *stack,
                     const gchar *name,
                     const gchar *title,
                     const gchar *icon)
{
  GtkWidget *button, *bin, *image;

  bin = make_bin ();
  gtk_widget_show (bin);
  gtk_stack_add_named (GTK_STACK (stack), bin, name);
  image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (image);
  g_object_set (G_OBJECT (image), "margin", HEADING_PADDING, NULL);
  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "image", image,
                         "image-position", GTK_POS_LEFT,
                         "always-show-image", TRUE,
                         "label", title,
                         NULL);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (switcher), button);
  g_object_bind_property_full (stack, "visible-child", button, "active", G_BINDING_BIDIRECTIONAL,
                               transform_stack_to_button, transform_button_to_stack, bin, NULL);
}

static GtkWidget *
make_two_output_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *switcher, *stack, *bin, *label;
  gboolean show_mirror;

  show_mirror = g_list_length (cc_display_config_get_cloning_modes (panel->current_config)) > 0;

  vbox = make_main_vbox (panel->main_size_group);
  gtk_widget_show (vbox);

  label = make_bold_label (_("Display Mode"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  switcher = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_show (switcher);
  gtk_widget_set_margin_bottom (switcher, SECTION_PADDING);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (switcher), GTK_BUTTONBOX_EXPAND);
  gtk_container_add (GTK_CONTAINER (vbox), switcher);

  stack = gtk_stack_new ();
  gtk_widget_show (stack);
  gtk_container_add (GTK_CONTAINER (vbox), stack);
  /* Add a dummy first stack page so that setting the visible child
   * below triggers a visible-child-name notification. */
  bin = make_bin ();
  gtk_widget_show (bin);
  gtk_stack_add_named (GTK_STACK (stack), bin, "dummy");

  add_two_output_page (switcher, stack, "join", _("Join Displays"),
                       "video-joined-displays-symbolic");
  if (show_mirror)
    add_two_output_page (switcher, stack, "mirror", _("Mirror"),
                         "view-mirror-symbolic");

  add_two_output_page (switcher, stack, "single", _("Single Display"),
                       "video-single-display-symbolic");

  g_signal_connect_object (stack, "notify::visible-child-name",
                           G_CALLBACK (two_output_visible_child_changed),
                           panel, G_CONNECT_SWAPPED);

  if (cc_display_config_is_cloning (panel->current_config) && show_mirror)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "mirror");
  else if (cc_display_config_count_useful_monitors (panel->current_config) > 1)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "join");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "single");

  return make_scrollable (vbox);
}

static void
output_switch_active (CcDisplayPanel *panel,
                      GParamSpec     *pspec,
                      GtkWidget      *button)
{
  cc_display_monitor_set_active (panel->current_output,
                                 gtk_switch_get_active (GTK_SWITCH (button)));
  cc_display_config_snap_output (panel->current_config, panel->current_output);
  update_apply_button (panel);
}

static void
output_switch_sync (GtkWidget        *button,
                    CcDisplayMonitor *output)
{
  gtk_switch_set_active (GTK_SWITCH (button), cc_display_monitor_is_active (output));
}

static GtkWidget *
make_output_switch (CcDisplayPanel *panel)
{
  GtkWidget *button = gtk_switch_new ();

  g_signal_connect_object (button, "notify::active", G_CALLBACK (output_switch_active),
                           panel, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->current_output, "active", G_CALLBACK (output_switch_sync),
                           button, G_CONNECT_SWAPPED);
  output_switch_sync (button, panel->current_output);

  if ((cc_display_config_count_useful_monitors (panel->current_config) < 2 && cc_display_monitor_is_active (panel->current_output)) ||
      !cc_display_monitor_is_usable (panel->current_output))
    gtk_widget_set_sensitive (button, FALSE);

  return button;
}

static void
replace_output_switch (GtkWidget      *frame,
                       CcDisplayPanel *panel)
{
  GtkWidget *sw;

  gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (frame)));
  sw = make_output_switch (panel);
  gtk_widget_show (sw);
  gtk_container_add (GTK_CONTAINER (frame), sw);
}

static void
output_chooser_row_activated (CcDisplayPanel *panel,
                              GtkWidget      *row)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (row), "output");
  set_current_output (panel, output);
}

static void
output_chooser_sync (GtkWidget      *button,
                     CcDisplayPanel *panel)
{
  const gchar *text = cc_display_monitor_get_ui_number_name (panel->current_output);
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (button));

  gtk_label_set_text (GTK_LABEL (label), text);
}

static GtkWidget *
make_output_chooser_button (CcDisplayPanel *panel)
{
  GtkWidget *listbox, *button, *popover, *label;
  GList *outputs, *l;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);

  listbox = make_list_box ();
  gtk_widget_show (listbox);

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *label, *row;
      const gchar *text;

      text = cc_display_monitor_get_ui_number_name (output);
      label = make_popover_label (text);
      gtk_widget_show (label);
      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", label,
                          NULL);
      gtk_widget_show (row);
      g_object_set_data (G_OBJECT (row), "output", output);

      g_signal_connect_object (row, "activated", G_CALLBACK (output_chooser_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  popover = make_list_popover (listbox);
  button = gtk_menu_button_new ();
  label = make_bold_label (NULL);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  g_signal_connect_object (panel, "current-output", G_CALLBACK (output_chooser_sync),
                           button, G_CONNECT_SWAPPED);
  output_chooser_sync (button, panel);

  return button;
}

static GtkWidget *
make_multi_output_ui (CcDisplayPanel *panel)
{
  GtkWidget *vbox, *ui, *frame, *sw, *hbox, *button;

  panel->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = make_main_vbox (panel->main_size_group);
  gtk_widget_show (vbox);

  ui = make_arrangement_ui (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (hbox);
  gtk_widget_set_margin_top (hbox, SECTION_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  button = make_output_chooser_button (panel);
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  frame = make_bin ();
  gtk_widget_show (frame);
  gtk_box_pack_end (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  sw = make_output_switch (panel);
  gtk_widget_show (sw);
  gtk_container_add (GTK_CONTAINER (frame), sw);
  g_signal_connect_object (panel, "current-output", G_CALLBACK (replace_output_switch),
                           frame, G_CONNECT_SWAPPED);

  frame = make_frame (NULL, NULL);
  gtk_widget_show (frame);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  panel->settings = cc_display_settings_new ();
  cc_display_settings_set_config (panel->settings, panel->current_config);
  cc_display_settings_set_has_accelerometer (panel->settings, panel->has_accelerometer);
  cc_display_settings_set_selected_output (panel->settings, panel->current_output);
  gtk_widget_show (GTK_WIDGET (panel->settings));
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (panel->settings));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (udpate_display_settings),
                           frame, G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), panel,
                           G_CONNECT_SWAPPED);

  ui = make_night_light_widget (panel);
  gtk_widget_show (ui);
  gtk_container_add (GTK_CONTAINER (vbox), ui);

  g_clear_object (&panel->rows_size_group);
  return make_scrollable (vbox);
}

static void
reset_current_config (CcDisplayPanel *panel)
{
  GList *outputs, *l;
  CcDisplayConfig *current;

  g_clear_object (&panel->current_config);
  panel->current_output = NULL;

  current = cc_display_config_manager_get_current (panel->manager);
  if (!current)
    return;

  panel->current_config = current;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      /* Mark any builtin monitor as unusable if the lid is closed. */
      if (cc_display_monitor_is_builtin (output) && panel->lid_is_closed)
        cc_display_monitor_set_usable (output, FALSE);

      if (!cc_display_monitor_is_useful (output))
        continue;

      panel->current_output = output;
      break;
    }
}

static void
on_screen_changed (CcDisplayPanel *panel)
{
  GtkWidget *main_widget;
  GList *outputs;
  guint n_outputs;

  if (!panel->manager)
    return;

  reset_titlebar (panel);

  main_widget = gtk_stack_get_child_by_name (GTK_STACK (panel->stack), "main");
  if (main_widget)
    gtk_widget_destroy (main_widget);

  reset_current_config (panel);

  if (!panel->current_config)
    goto show_error;

  ensure_monitor_labels (panel);

  if (!panel->current_output)
    goto show_error;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  n_outputs = g_list_length (outputs);
  if (panel->lid_is_closed)
    {
      if (n_outputs <= 2)
        main_widget = make_single_output_ui (panel);
      else
        main_widget = make_multi_output_ui (panel);
    }
  else
    {
      if (n_outputs == 1)
        main_widget = make_single_output_ui (panel);
      else if (n_outputs == 2)
        main_widget = make_two_output_ui (panel);
      else
        main_widget = make_multi_output_ui (panel);
    }

  gtk_widget_show (main_widget);
  gtk_stack_add_named (GTK_STACK (panel->stack), main_widget, "main");
  gtk_stack_set_visible_child (GTK_STACK (panel->stack), main_widget);
  return;

 show_error:
  gtk_stack_set_visible_child_name (GTK_STACK (panel->stack), "error");
}

static gboolean
on_toplevel_key_press (GtkWidget   *button,
                       GdkEventKey *event)
{
  if (event->keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  g_signal_emit_by_name (button, "activate");
  return GDK_EVENT_STOP;
}

static void
show_apply_titlebar (CcDisplayPanel *panel, gboolean is_applicable)
{
  if (!panel->apply_titlebar)
    {
      g_autoptr(GtkSizeGroup) size_group = NULL;
      GtkWidget *header, *button, *toplevel;
      panel->apply_titlebar = header = gtk_header_bar_new ();
      gtk_widget_show (header);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

      button = gtk_button_new_with_mnemonic (_("_Cancel"));
      gtk_widget_show (button);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);
      gtk_size_group_add_widget (size_group, button);
      g_signal_connect_object (button, "clicked", G_CALLBACK (on_screen_changed),
                               panel, G_CONNECT_SWAPPED);

      toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));
      g_signal_connect_object (toplevel, "key-press-event", G_CALLBACK (on_toplevel_key_press),
                               button, G_CONNECT_SWAPPED);

      panel->apply_titlebar_apply = button = gtk_button_new_with_mnemonic (_("_Apply"));
      gtk_widget_show (button);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
      gtk_size_group_add_widget (size_group, button);
      g_signal_connect_object (button, "clicked", G_CALLBACK (apply_current_configuration),
                               panel, G_CONNECT_SWAPPED);
      gtk_style_context_add_class (gtk_widget_get_style_context (button),
                                   GTK_STYLE_CLASS_SUGGESTED_ACTION);

      header = gtk_window_get_titlebar (GTK_WINDOW (toplevel));
      if (header)
        panel->main_titlebar = g_object_ref (header);
      panel->main_title = g_strdup (gtk_window_get_title (GTK_WINDOW (toplevel)));

      gtk_window_set_titlebar (GTK_WINDOW (toplevel), panel->apply_titlebar);
      g_object_ref (panel->apply_titlebar);
      g_object_ref (panel->apply_titlebar_apply);
    }

  if (is_applicable)
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (panel->apply_titlebar), _("Apply Changes?"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (panel->apply_titlebar), NULL);
    }
  else
    {
      gtk_header_bar_set_title (GTK_HEADER_BAR (panel->apply_titlebar), _("Changes Cannot be Applied"));
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (panel->apply_titlebar), _("This could be due to hardware limitations."));
    }
  gtk_widget_set_sensitive (panel->apply_titlebar_apply, is_applicable);
}

static void
update_apply_button (CcDisplayPanel *panel)
{
  gboolean config_equal;
  g_autoptr(CcDisplayConfig) applied_config = NULL;

  applied_config = cc_display_config_manager_get_current (panel->manager);

  config_equal = cc_display_config_equal (panel->current_config,
                                          applied_config);

  if (config_equal)
    reset_titlebar (panel);
  else
    show_apply_titlebar (panel, cc_display_config_is_applicable (panel->current_config));
}

static void
apply_current_configuration (CcDisplayPanel *self)
{
  g_autoptr(GError) error = NULL;

  cc_display_config_apply (self->current_config, &error);

  /* re-read the configuration */
  on_screen_changed (self);

  if (error)
    g_warning ("Error applying configuration: %s", error->message);
}

static void
cc_display_panel_night_light_activated (CcDisplayPanel *panel)
{
  GtkWindow *toplevel;
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));
  gtk_window_set_transient_for (GTK_WINDOW (panel->night_light_dialog), toplevel);
  gtk_window_present (GTK_WINDOW (panel->night_light_dialog));
}

static void
mapped_cb (CcDisplayPanel *panel)
{
  CcShell *shell;
  GtkWidget *toplevel;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  toplevel = cc_shell_get_toplevel (shell);
  if (toplevel && !panel->focus_id)
    panel->focus_id = g_signal_connect_swapped (toplevel, "notify::has-toplevel-focus",
                                               G_CALLBACK (dialog_toplevel_focus_changed), panel);
}

static void
cc_display_panel_up_client_changed (UpClient       *client,
                                    GParamSpec     *pspec,
                                    CcDisplayPanel *self)
{
  gboolean lid_is_closed;

  lid_is_closed = up_client_get_lid_is_closed (client);

  if (lid_is_closed != self->lid_is_closed)
    {
      self->lid_is_closed = lid_is_closed;

      on_screen_changed (self);
    }
}

static void
shell_proxy_ready (GObject        *source,
                   GAsyncResult   *res,
                   CcDisplayPanel *self)
{
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (!proxy)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to contact gnome-shell: %s", error->message);
      return;
    }

  self->shell_proxy = proxy;

  ensure_monitor_labels (self);
}

static void
update_has_accel (CcDisplayPanel *self)
{
  g_autoptr(GVariant) v = NULL;

  if (self->iio_sensor_proxy == NULL)
    {
      g_debug ("Has no accelerometer");
      self->has_accelerometer = FALSE;
      return;
    }

  v = g_dbus_proxy_get_cached_property (self->iio_sensor_proxy, "HasAccelerometer");
  if (v)
    {
      self->has_accelerometer = g_variant_get_boolean (v);
    }
  else
    {
      self->has_accelerometer = FALSE;
    }

  g_debug ("Has %saccelerometer", self->has_accelerometer ? "" : "no ");
}

static void
sensor_proxy_properties_changed_cb (GDBusProxy     *proxy,
                                    GVariant       *changed_properties,
                                    GStrv           invalidated_properties,
                                    CcDisplayPanel *self)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, changed_properties);

  if (g_variant_dict_contains (&dict, "HasAccelerometer"))
    update_has_accel (self);
}

static void
sensor_proxy_appeared_cb (GDBusConnection *connection,
                          const gchar     *name,
                          const gchar     *name_owner,
                          gpointer         user_data)
{
  CcDisplayPanel *self = user_data;

  g_debug ("SensorProxy appeared");

  self->iio_sensor_proxy = g_dbus_proxy_new_sync (connection,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        NULL,
                                                        "net.hadess.SensorProxy",
                                                        "/net/hadess/SensorProxy",
                                                        "net.hadess.SensorProxy",
                                                        NULL,
                                                        NULL);
  g_return_if_fail (self->iio_sensor_proxy);

  g_signal_connect (self->iio_sensor_proxy, "g-properties-changed",
                    G_CALLBACK (sensor_proxy_properties_changed_cb), self);
  update_has_accel (self);
}

static void
sensor_proxy_vanished_cb (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  CcDisplayPanel *self = user_data;

  g_debug ("SensorProxy vanished");

  g_clear_object (&self->iio_sensor_proxy);
  update_has_accel (self);
}

static void
night_light_sync_label (GtkWidget *label, GSettings *settings)
{
  gboolean ret = g_settings_get_boolean (settings, "night-light-enabled");
  gtk_label_set_label (GTK_LABEL (label),
                       /* TRANSLATORS: the state of the night light setting */
                       ret ? _("On") : _("Off"));
}

static void
settings_color_changed_cb (GSettings *settings, gchar *key, GtkWidget *label)
{
  if (g_strcmp0 (key, "night-light-enabled") == 0)
    night_light_sync_label (label, settings);
}

static GtkWidget *
make_night_light_widget (CcDisplayPanel *self)
{
  GtkWidget *frame, *row, *label, *state_label;
  GtkWidget *night_light_listbox;

  frame = make_frame (NULL, NULL);
  night_light_listbox = make_list_box ();
  gtk_widget_show (night_light_listbox);
  gtk_container_add (GTK_CONTAINER (frame), night_light_listbox);

  label = gtk_label_new (_("_Night Light"));
  gtk_widget_show (label);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);

  state_label = gtk_label_new ("");
  gtk_widget_show (state_label);
  g_signal_connect_object (self->settings_color, "changed",
                           G_CALLBACK (settings_color_changed_cb), state_label, 0);
  night_light_sync_label (state_label, self->settings_color);

  row = make_row (self->rows_size_group, label, state_label);
  gtk_widget_show (row);
  gtk_container_add (GTK_CONTAINER (night_light_listbox), row);
  g_signal_connect_object (row, "activated",
                           G_CALLBACK (cc_display_panel_night_light_activated),
                           self, G_CONNECT_SWAPPED);

  gtk_widget_set_margin_top (frame, SECTION_PADDING);
  return frame;
}

static void
session_bus_ready (GObject        *source,
                   GAsyncResult   *res,
                   CcDisplayPanel *self)
{
  GDBusConnection *bus;
  g_autoptr(GError) error = NULL;

  bus = g_bus_get_finish (res, &error);
  if (!bus)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to get session bus: %s", error->message);
          gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "error");
        }
      return;
    }

  self->manager = cc_display_config_manager_dbus_new ();
  g_signal_connect_object (self->manager, "changed",
                           G_CALLBACK (on_screen_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
cc_display_panel_init (CcDisplayPanel *self)
{
  g_autoptr (GtkCssProvider) provider = NULL;
  GtkWidget *bin, *label;

  g_resources_register (cc_display_get_resource ());

  self->stack = gtk_stack_new ();
  gtk_widget_show (self->stack);

  bin = make_bin ();
  gtk_widget_show (bin);
  gtk_widget_set_size_request (bin, 500, -1);
  gtk_stack_add_named (GTK_STACK (self->stack), bin, "main-size-group");
  self->main_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (self->main_size_group, bin);

  label = gtk_label_new (_("Could not get screen information"));
  gtk_widget_show (label);
  gtk_stack_add_named (GTK_STACK (self->stack),
                       label,
                       "error");

  gtk_container_add (GTK_CONTAINER (self), self->stack);

  self->night_light_dialog = cc_night_light_dialog_new ();
  self->settings_color = g_settings_new ("org.gnome.settings-daemon.plugins.color");

  self->up_client = up_client_new ();
  if (up_client_get_lid_is_present (self->up_client))
    {
      g_signal_connect (self->up_client, "notify::lid-is-closed",
                        G_CALLBACK (cc_display_panel_up_client_changed), self);
      cc_display_panel_up_client_changed (self->up_client, NULL, self);
    }
  else
    g_clear_object (&self->up_client);

  g_signal_connect (self, "map", G_CALLBACK (mapped_cb), NULL);

  self->shell_cancellable = g_cancellable_new ();
  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                       "org.gnome.Shell",
                                       "/org/gnome/Shell",
                                       "org.gnome.Shell",
                                       self->shell_cancellable,
                                       (GAsyncReadyCallback) shell_proxy_ready,
                                       self);

  g_bus_get (G_BUS_TYPE_SESSION,
             self->shell_cancellable,
             (GAsyncReadyCallback) session_bus_ready,
             self);

  self->sensor_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                            "net.hadess.SensorProxy",
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            sensor_proxy_appeared_cb,
                                            sensor_proxy_vanished_cb,
                                            self,
                                            NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/display/display-arrangement.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
