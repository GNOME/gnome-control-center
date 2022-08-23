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
#include <libupower-glib/upower.h>

#include "cc-display-config-manager-dbus.h"
#include "cc-display-config.h"
#include "cc-display-arrangement.h"
#include "cc-night-light-page.h"
#include "cc-display-resources.h"
#include "cc-display-settings.h"

/* The minimum supported size for the panel
 * Note that WIDTH is assumed to be the larger size and we accept portrait
 * mode too effectively (in principle we should probably restrict the rotation
 * setting in that case). */
#define MINIMUM_WIDTH  720
#define MINIMUM_HEIGHT 360

#define PANEL_PADDING   32
#define SECTION_PADDING 32
#define HEADING_PADDING 12

#define DISPLAY_SCHEMA   "org.gnome.settings-daemon.plugins.color"

typedef enum {
  CC_DISPLAY_CONFIG_JOIN,
  CC_DISPLAY_CONFIG_CLONE,

  CC_DISPLAY_CONFIG_INVALID_NONE,
} CcDisplayConfigType;

#define CC_DISPLAY_CONFIG_LAST_VALID CC_DISPLAY_CONFIG_CLONE

struct _CcDisplayPanel
{
  CcPanel parent_instance;

  CcDisplayConfigManager *manager;
  CcDisplayConfig *current_config;
  CcDisplayMonitor *current_output;

  gint                  rebuilding_counter;

  CcDisplayArrangement *arrangement;
  CcDisplaySettings    *settings;

  guint           focus_id;

  CcNightLightPage *night_light_page;
  GtkLabel *night_light_state_label;

  UpClient *up_client;
  gboolean lid_is_closed;

  GDBusProxy *shell_proxy;

  GtkWidget      *apply_titlebar;
  GtkWidget      *apply_button;
  GtkWidget      *cancel_button;
  AdwWindowTitle *apply_titlebar_title_widget;

  GListStore     *primary_display_list;
  GList          *monitor_rows;

  GtkWidget      *display_settings_disabled_group;

  GtkWidget      *arrangement_row;
  AdwBin         *arrangement_bin;
  GtkToggleButton *config_type_join;
  GtkToggleButton *config_type_mirror;
  GtkWidget      *display_multiple_displays;
  AdwBin         *display_settings_bin;
  GtkWidget      *display_settings_group;
  AdwWindowTitle *display_settings_title_widget;
  AdwLeaflet     *leaflet;
  AdwComboRow    *primary_display_row;
  AdwPreferencesGroup *single_display_settings_group;

  GtkShortcutController *toplevel_shortcuts;
  GtkShortcut *escape_shortcut;

  GSettings           *display_settings;
};

CC_PANEL_REGISTER (CcDisplayPanel, cc_display_panel)

static void
update_apply_button (CcDisplayPanel *panel);
static void
apply_current_configuration (CcDisplayPanel *self);
static void
reset_current_config (CcDisplayPanel *panel);
static void
rebuild_ui (CcDisplayPanel *panel);
static void
set_current_output (CcDisplayPanel   *panel,
                    CcDisplayMonitor *output,
                    gboolean          force);
static void
on_screen_changed (CcDisplayPanel *panel);


static CcDisplayConfigType
config_get_current_type (CcDisplayPanel *panel)
{
  guint n_active_outputs;
  GList *outputs, *l;

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  n_active_outputs = 0;
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (cc_display_monitor_is_useful (output))
        n_active_outputs += 1;
    }

  if (n_active_outputs == 0)
    return CC_DISPLAY_CONFIG_INVALID_NONE;

  if (cc_display_config_is_cloning (panel->current_config))
    return CC_DISPLAY_CONFIG_CLONE;

  return CC_DISPLAY_CONFIG_JOIN;
}

static CcDisplayConfigType
cc_panel_get_selected_type (CcDisplayPanel *panel)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (panel->config_type_join)))
    return CC_DISPLAY_CONFIG_JOIN;
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (panel->config_type_mirror)))
    return CC_DISPLAY_CONFIG_CLONE;
  else
    g_assert_not_reached ();
}

static CcDisplayMode *
find_preferred_mode (GList *modes)
{
  GList *l;

  for (l = modes; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;

      if (cc_display_mode_is_preferred (mode))
        return mode;
    }

  return NULL;
}

static void
config_ensure_of_type (CcDisplayPanel *panel, CcDisplayConfigType type)
{
  CcDisplayConfigType current_type = config_get_current_type (panel);
  GList *outputs, *l;
  CcDisplayMonitor *current_primary = NULL;
  gdouble old_primary_scale = -1;

  /* Do not do anything if the current detected configuration type is
   * identitcal to what we expect. */
  if (type == current_type)
    return;

  reset_current_config (panel);

  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (cc_display_monitor_is_primary (output))
        {
          current_primary = output;
          old_primary_scale = cc_display_monitor_get_scale (current_primary);
          break;
        }
    }

  switch (type)
    {
    case CC_DISPLAY_CONFIG_JOIN:
      g_debug ("Creating new join config");
      /* Enable all usable outputs
       * Note that this might result in invalid configurations as we
       * we might not be able to drive all attached monitors. */
      cc_display_config_set_cloning (panel->current_config, FALSE);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;
          gdouble scale;
          CcDisplayMode *mode;

          mode = cc_display_monitor_get_preferred_mode (output);
          /* If the monitor was active, try using the current scale, otherwise
           * try picking the preferred scale. */
          if (cc_display_monitor_is_active (output))
            scale = cc_display_monitor_get_scale (output);
          else
            scale = cc_display_mode_get_preferred_scale (mode);

          /* If we cannot use the current/preferred scale, try to fall back to
           * the previously scale of the primary monitor instead.
           * This is not guaranteed to result in a valid configuration! */
          if (!cc_display_config_is_scaled_mode_valid (panel->current_config,
                                                       mode,
                                                       scale))
            {
              if (current_primary &&
                  cc_display_config_is_scaled_mode_valid (panel->current_config,
                                                          mode,
                                                          old_primary_scale))
                scale = old_primary_scale;
            }

          cc_display_monitor_set_active (output, cc_display_monitor_is_usable (output));
          cc_display_monitor_set_mode (output, mode);
          cc_display_monitor_set_scale (output, scale);
        }
      break;

    case CC_DISPLAY_CONFIG_CLONE:
      {
        g_debug ("Creating new clone config");
        gdouble scale;
        g_autolist(CcDisplayMode) modes = NULL;
        CcDisplayMode *clone_mode;

        /* Turn on cloning and select the best mode we can find by default */
        cc_display_config_set_cloning (panel->current_config, TRUE);

        modes = cc_display_config_generate_cloning_modes (panel->current_config);
        clone_mode = find_preferred_mode (modes);
        g_return_if_fail (clone_mode);

        /* Take the preferred scale by default, */
        scale = cc_display_mode_get_preferred_scale (clone_mode);
        /* but prefer the old primary scale if that is valid. */
        if (current_primary &&
            cc_display_config_is_scaled_mode_valid (panel->current_config,
                                                    clone_mode,
                                                    old_primary_scale))
          scale = old_primary_scale;

        for (l = outputs; l; l = l->next)
          {
            CcDisplayMonitor *output = l->data;

            cc_display_monitor_set_compatible_clone_mode (output, clone_mode);
            cc_display_monitor_set_scale (output, scale);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }

  if (!panel->rebuilding_counter)
    rebuild_ui (panel);
}

static void
cc_panel_set_selected_type (CcDisplayPanel *panel, CcDisplayConfigType type)
{
  switch (type)
    {
    case CC_DISPLAY_CONFIG_JOIN:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panel->config_type_join), TRUE);
      break;
    case CC_DISPLAY_CONFIG_CLONE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panel->config_type_mirror), TRUE);
      break;
    default:
      g_assert_not_reached ();
    }

  config_ensure_of_type (panel, type);
}

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
    {
      g_variant_builder_clear (&builder);
      return monitor_labeler_hide (self);
    }

  g_dbus_proxy_call (self->shell_proxy,
                     "ShowMonitorLabels",
                     g_variant_builder_end (&builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
ensure_monitor_labels (CcDisplayPanel *self)
{
  g_autoptr(GList) windows = NULL;
  GList *w;

  windows = gtk_window_list_toplevels ();

  for (w = windows; w; w = w->next)
    {
      if (gtk_window_is_active (GTK_WINDOW (w->data)))
        {
          monitor_labeler_show (self);
          break;
        }
    }

  if (!w)
    monitor_labeler_hide (self);
}

static void
dialog_toplevel_is_active_changed (CcDisplayPanel *self)
{
  ensure_monitor_labels (self);
}

static void
reset_titlebar (CcDisplayPanel *self)
{
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->toplevel_shortcuts),
                                              GTK_PHASE_NONE);
  gtk_widget_hide (self->apply_titlebar);
}

static void
active_panel_changed (CcPanel *self)
{
  CcShell *shell;
  g_autoptr(CcPanel) panel = NULL;

  shell = cc_panel_get_shell (CC_PANEL (self));
  g_object_get (shell, "active-panel", &panel, NULL);
  if (panel != self)
    reset_titlebar (CC_DISPLAY_PANEL (self));
}

static void
cc_display_panel_dispose (GObject *object)
{
  CcDisplayPanel *self = CC_DISPLAY_PANEL (object);
  GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  reset_titlebar (CC_DISPLAY_PANEL (object));

  if (self->focus_id)
    {
      self->focus_id = 0;
      monitor_labeler_hide (CC_DISPLAY_PANEL (object));
    }

  g_clear_pointer (&self->monitor_rows, g_list_free);
  g_clear_object (&self->manager);
  g_clear_object (&self->current_config);
  g_clear_object (&self->up_client);

  g_clear_object (&self->shell_proxy);

  g_signal_handlers_disconnect_by_data (toplevel, self);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
on_arrangement_selected_ouptut_changed_cb (CcDisplayPanel *panel)
{
  set_current_output (panel, cc_display_arrangement_get_selected_output (panel->arrangement), FALSE);
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
on_back_button_clicked_cb (GtkButton      *button,
                           CcDisplayPanel *self)
{
  adw_leaflet_set_visible_child_name (self->leaflet, "displays");
}

static void
on_config_type_toggled_cb (CcDisplayPanel *panel,
                           GtkCheckButton *btn)
{
  CcDisplayConfigType type;

  if (panel->rebuilding_counter > 0)
    return;

  if (!panel->current_config)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    return;

  type = cc_panel_get_selected_type (panel);
  config_ensure_of_type (panel, type);
}

static void
on_night_light_enabled_changed_cb (GSettings      *settings,
                                   const gchar    *key,
                                   CcDisplayPanel *self)
{
  if (g_settings_get_boolean (self->display_settings, "night-light-enabled"))
    gtk_label_set_label (self->night_light_state_label, _("On"));
  else
    gtk_label_set_label (self->night_light_state_label, _("Off"));
}

static void
on_night_light_row_activated_cb (GtkListBoxRow  *row,
                                 CcDisplayPanel *self)
{
  adw_leaflet_set_visible_child_name (self->leaflet, "night-light");
}

static void
on_primary_display_selected_item_changed_cb (CcDisplayPanel *panel)
{
  gint idx = adw_combo_row_get_selected (panel->primary_display_row);
  g_autoptr(CcDisplayMonitor) output = NULL;

  if (idx < 0 || panel->rebuilding_counter > 0)
    return;

  output = g_list_model_get_item (G_LIST_MODEL (panel->primary_display_list), idx);

  if (cc_display_monitor_is_primary (output))
    return;

  cc_display_monitor_set_primary (output, TRUE);
  update_apply_button (panel);
}

static void
on_toplevel_folded (CcDisplayPanel *panel, GParamSpec *pspec, GtkWidget *toplevel)
{
  gboolean folded;

  g_object_get (toplevel, "folded", &folded, NULL);
  cc_display_settings_refresh_layout (panel->settings, folded);
}

static gboolean
on_toplevel_escape_pressed_cb (GtkWidget      *widget,
                               GVariant       *args,
                               CcDisplayPanel *self)
{
  if (gtk_widget_get_visible (self->apply_titlebar))
    {
      gtk_widget_activate (self->cancel_button);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
cc_display_panel_constructed (GObject *object)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (object));
  GtkWidget *toplevel = cc_shell_get_toplevel (shell);

  g_signal_connect_object (cc_panel_get_shell (CC_PANEL (object)), "notify::active-panel",
                           G_CALLBACK (active_panel_changed), object, G_CONNECT_SWAPPED);

  g_signal_connect_swapped (toplevel, "notify::folded", G_CALLBACK (on_toplevel_folded), object);
  on_toplevel_folded (CC_DISPLAY_PANEL (object), NULL, toplevel);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_NIGHT_LIGHT_PAGE);

  panel_class->get_help_uri = cc_display_panel_get_help_uri;

  object_class->constructed = cc_display_panel_constructed;
  object_class->dispose = cc_display_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/display/cc-display-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, apply_button);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, apply_titlebar);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, apply_titlebar_title_widget);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_settings_disabled_group);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, arrangement_bin);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, arrangement_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_multiple_displays);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_join);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_mirror);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_settings_bin);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_settings_group);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_settings_title_widget);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, escape_shortcut);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, leaflet);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, night_light_page);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, night_light_state_label);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, primary_display_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, single_display_settings_group);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, toplevel_shortcuts);

  gtk_widget_class_bind_template_callback (widget_class, apply_current_configuration);
  gtk_widget_class_bind_template_callback (widget_class, on_back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_config_type_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_night_light_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_primary_display_selected_item_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_screen_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_toplevel_escape_pressed_cb);
}

static void
set_current_output (CcDisplayPanel   *panel,
                    CcDisplayMonitor *output,
                    gboolean          force)
{
  gboolean changed;

  /* Note, this function is also called if the internal UI needs updating after a rebuild. */
  changed = (output != panel->current_output);

  if (!changed && !force)
    return;

  panel->rebuilding_counter++;

  panel->current_output = output;

  if (changed)
    {
      cc_display_settings_set_selected_output (panel->settings, panel->current_output);
      cc_display_arrangement_set_selected_output (panel->arrangement, panel->current_output);

      adw_window_title_set_title (panel->display_settings_title_widget,
                                  output ? cc_display_monitor_get_ui_name (output) : "");
    }

  panel->rebuilding_counter--;
}

static void
on_monitor_row_activated_cb (GtkListBoxRow  *row,
                             CcDisplayPanel *self)
{
  CcDisplayMonitor *monitor;

  monitor = g_object_get_data (G_OBJECT (row), "monitor");
  set_current_output (self, monitor, FALSE);

  adw_leaflet_set_visible_child_name (self->leaflet, "display-settings");
}

static void
add_display_row (CcDisplayPanel   *self,
                 CcDisplayMonitor *monitor)
{
  g_autofree gchar *number_string = NULL;
  GtkWidget *number_label;
  GtkWidget *icon;
  GtkWidget *row;

  row = adw_action_row_new ();
  g_object_set_data (G_OBJECT (row), "monitor", monitor);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 cc_display_monitor_get_ui_name (monitor));

  number_string = g_strdup_printf ("%d", cc_display_monitor_get_ui_number (monitor));
  number_label = gtk_label_new (number_string);
  gtk_widget_set_valign (number_label, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (number_label, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (number_label, "monitor-label");
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), number_label);

  icon = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), icon);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), icon);

  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->display_settings_group), row);

  g_signal_connect (row, "activated", G_CALLBACK (on_monitor_row_activated_cb), self);

  self->monitor_rows = g_list_prepend (self->monitor_rows, row);
}

static void
move_display_settings_to_main_page (CcDisplayPanel *self)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self->settings));

  if (parent != GTK_WIDGET (self->display_settings_bin))
    return;

  g_object_ref (self->settings);
  adw_bin_set_child (self->display_settings_bin, NULL);
  adw_preferences_group_add (self->single_display_settings_group,
                             GTK_WIDGET (self->settings));
  g_object_unref (self->settings);

  gtk_widget_show (GTK_WIDGET (self->single_display_settings_group));
}

static void
move_display_settings_to_separate_page (CcDisplayPanel *self)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self->settings));

  if (parent == GTK_WIDGET (self->display_settings_bin))
    return;

  g_object_ref (self->settings);
  adw_preferences_group_remove (self->single_display_settings_group,
                                GTK_WIDGET (self->settings));
  adw_bin_set_child (self->display_settings_bin,
                     GTK_WIDGET (self->settings));
  g_object_unref (self->settings);

  gtk_widget_hide (GTK_WIDGET (self->single_display_settings_group));
}

static void
rebuild_ui (CcDisplayPanel *panel)
{
  guint n_outputs, n_active_outputs, n_usable_outputs;
  GList *outputs, *l;
  CcDisplayConfigType type;

  if (!cc_display_config_manager_get_apply_allowed (panel->manager))
    {
      gtk_widget_set_visible (panel->display_settings_disabled_group, TRUE);
      gtk_widget_set_visible (panel->display_settings_group, FALSE);
      gtk_widget_set_visible (panel->arrangement_row, FALSE);
      return;
    }

  panel->rebuilding_counter++;

  g_list_store_remove_all (panel->primary_display_list);

  /* Remove all monitor rows */
  while (panel->monitor_rows)
    {
      adw_preferences_group_remove (ADW_PREFERENCES_GROUP (panel->display_settings_group),
                                    panel->monitor_rows->data);
      panel->monitor_rows = g_list_remove_link (panel->monitor_rows, panel->monitor_rows);
    }

  if (!panel->current_config)
    {
      panel->rebuilding_counter--;
      return;
    }

  gtk_widget_set_visible (panel->display_settings_disabled_group, FALSE);

  n_active_outputs = 0;
  n_usable_outputs = 0;
  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (!cc_display_monitor_is_usable (output))
        continue;

      n_usable_outputs += 1;

      if (cc_display_monitor_is_active (output))
        {
          n_active_outputs += 1;

          g_list_store_append (panel->primary_display_list, output);
          if (cc_display_monitor_is_primary (output))
            adw_combo_row_set_selected (panel->primary_display_row,
                                        g_list_model_get_n_items (G_LIST_MODEL (panel->primary_display_list)) - 1);

          /* Ensure that an output is selected; note that this doesn't ensure
           * the selected output is any useful (i.e. when switching types).
           */
          if (!panel->current_output)
            set_current_output (panel, output, FALSE);
        }

      add_display_row (panel, l->data);
    }

  /* Sync the rebuild lists/buttons */
  set_current_output (panel, panel->current_output, TRUE);

  n_outputs = g_list_length (outputs);
  type = config_get_current_type (panel);

  if (n_outputs == 2 && n_usable_outputs == 2)
    {
      /* We only show the top chooser with two monitors that are
       * both usable (i.e. two monitors incl. internal and lid not closed).
       * In this case, the arrangement widget is shown if we are in JOIN mode.
       */
      if (type > CC_DISPLAY_CONFIG_LAST_VALID)
        type = CC_DISPLAY_CONFIG_JOIN;

      gtk_widget_set_visible (panel->display_settings_group, type == CC_DISPLAY_CONFIG_JOIN);
      gtk_widget_set_visible (panel->display_multiple_displays, TRUE);
      gtk_widget_set_visible (panel->arrangement_row, type == CC_DISPLAY_CONFIG_JOIN);

      if (type == CC_DISPLAY_CONFIG_CLONE)
        move_display_settings_to_main_page (panel);
      else
        move_display_settings_to_separate_page (panel);
    }
  else if (n_usable_outputs > 1)
    {
      /* We have more than one usable monitor. In this case there is no chooser,
       * and we always show the arrangement widget.
       */
      gtk_widget_set_visible (panel->display_settings_group, TRUE);
      gtk_widget_set_visible (panel->display_multiple_displays, FALSE);
      gtk_widget_set_visible (panel->arrangement_row, TRUE);

      /* Mirror is also invalid as it cannot be configured using this UI. */
      if (type == CC_DISPLAY_CONFIG_CLONE || type > CC_DISPLAY_CONFIG_LAST_VALID)
        type = CC_DISPLAY_CONFIG_JOIN;

      move_display_settings_to_separate_page (panel);
    }
  else
    {
      type = CC_DISPLAY_CONFIG_JOIN;

      gtk_widget_set_visible (panel->display_settings_group, FALSE);
      gtk_widget_set_visible (panel->display_multiple_displays, FALSE);
      gtk_widget_set_visible (panel->arrangement_row, FALSE);

      move_display_settings_to_main_page (panel);
    }

  cc_display_settings_set_multimonitor (panel->settings,
                                        n_outputs > 1 &&
                                        type != CC_DISPLAY_CONFIG_CLONE);

  cc_panel_set_selected_type (panel, type);

  panel->rebuilding_counter--;
  update_apply_button (panel);
}

static void
update_panel_orientation_managed (CcDisplayPanel *panel,
                                  gboolean        managed)
{
  cc_display_settings_set_has_accelerometer (panel->settings, managed);
}

static void
reset_current_config (CcDisplayPanel *panel)
{
  CcDisplayConfig *current;
  CcDisplayConfig *old;
  GList *outputs, *l;

  g_debug ("Resetting current config!");

  /* We need to hold on to the config until all display references are dropped. */
  old = panel->current_config;
  panel->current_output = NULL;

  current = cc_display_config_manager_get_current (panel->manager);

  if (!current)
    return;

  cc_display_config_set_minimum_size (current, MINIMUM_WIDTH, MINIMUM_HEIGHT);
  panel->current_config = current;

  g_signal_connect_object (current, "panel-orientation-managed",
                           G_CALLBACK (update_panel_orientation_managed), panel,
                           G_CONNECT_SWAPPED);
  update_panel_orientation_managed (panel,
                                    cc_display_config_get_panel_orientation_managed (current));

  g_list_store_remove_all (panel->primary_display_list);

  if (panel->current_config)
    {
      outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          /* Mark any builtin monitor as unusable if the lid is closed. */
          if (cc_display_monitor_is_builtin (output) && panel->lid_is_closed)
            cc_display_monitor_set_usable (output, FALSE);
        }
    }

  cc_display_arrangement_set_config (panel->arrangement, panel->current_config);
  cc_display_settings_set_config (panel->settings, panel->current_config);
  set_current_output (panel, NULL, FALSE);

  g_clear_object (&old);

  update_apply_button (panel);
}

static void
on_screen_changed (CcDisplayPanel *panel)
{
  if (!panel->manager)
    return;

  reset_titlebar (panel);

  reset_current_config (panel);
  rebuild_ui (panel);

  if (!panel->current_config)
    return;

  ensure_monitor_labels (panel);
}

static void
show_apply_titlebar (CcDisplayPanel *panel, gboolean is_applicable)
{
  gtk_widget_show (panel->apply_titlebar);
  gtk_widget_set_sensitive (panel->apply_button, is_applicable);

  if (is_applicable)
    {
      adw_window_title_set_title (panel->apply_titlebar_title_widget,
                                  _("Apply Changes?"));
    }
  else
    {
      adw_window_title_set_title (panel->apply_titlebar_title_widget,
                                  _("Changes Cannot be Applied"));
      adw_window_title_set_subtitle (panel->apply_titlebar_title_widget,
                                  _("This could be due to hardware limitations."));
    }

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (panel->toplevel_shortcuts),
                                              GTK_PHASE_BUBBLE);
}

static void
update_apply_button (CcDisplayPanel *panel)
{
  gboolean config_equal;
  g_autoptr(CcDisplayConfig) applied_config = NULL;

  if (!panel->current_config)
    {
      reset_titlebar (panel);
      return;
    }

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

  adw_leaflet_set_visible_child_name (self->leaflet, "displays");
}

static void
mapped_cb (CcDisplayPanel *panel)
{
  CcShell *shell;
  GtkWidget *toplevel;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  toplevel = cc_shell_get_toplevel (shell);
  if (toplevel && !panel->focus_id)
    panel->focus_id = g_signal_connect_object (toplevel, "notify::is-active",
                                               G_CALLBACK (dialog_toplevel_is_active_changed), panel, G_CONNECT_SWAPPED);
}

static void
cc_display_panel_up_client_changed (CcDisplayPanel *self)
{
  gboolean lid_is_closed;

  lid_is_closed = up_client_get_lid_is_closed (self->up_client);

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
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkExpression *expression;

  g_resources_register (cc_display_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->arrangement = cc_display_arrangement_new (NULL);
  gtk_widget_set_size_request (GTK_WIDGET (self->arrangement), 375, 175);
  adw_bin_set_child (self->arrangement_bin, GTK_WIDGET (self->arrangement));

  g_signal_connect_object (self->arrangement, "updated",
			   G_CALLBACK (update_apply_button), self,
			   G_CONNECT_SWAPPED);
  g_signal_connect_object (self->arrangement, "notify::selected-output",
			   G_CALLBACK (on_arrangement_selected_ouptut_changed_cb), self,
			   G_CONNECT_SWAPPED);

  self->settings = cc_display_settings_new ();
  adw_bin_set_child (self->display_settings_bin, GTK_WIDGET (self->settings));
  g_signal_connect_object (self->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), self,
                           G_CONNECT_SWAPPED);

  self->primary_display_list = g_list_store_new (CC_TYPE_DISPLAY_MONITOR);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL, 0, NULL,
                                            G_CALLBACK (cc_display_monitor_dup_ui_number_name),
                                            self, NULL);
  adw_combo_row_set_expression (self->primary_display_row, expression);
  adw_combo_row_set_model (self->primary_display_row,
                           G_LIST_MODEL (self->primary_display_list));

  self->up_client = up_client_new ();
  if (up_client_get_lid_is_present (self->up_client))
    {
      g_signal_connect_object (self->up_client, "notify::lid-is-closed",
                               G_CALLBACK (cc_display_panel_up_client_changed), self, G_CONNECT_SWAPPED);
      cc_display_panel_up_client_changed (self);
    }
  else
    g_clear_object (&self->up_client);

  g_signal_connect (self, "map", G_CALLBACK (mapped_cb), NULL);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                       "org.gnome.Shell",
                                       "/org/gnome/Shell",
                                       "org.gnome.Shell",
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       (GAsyncReadyCallback) shell_proxy_ready,
                                       self);

  g_bus_get (G_BUS_TYPE_SESSION,
             cc_panel_get_cancellable (CC_PANEL (self)),
             (GAsyncReadyCallback) session_bus_ready,
             self);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/display/display-arrangement.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_shortcut_set_action (self->escape_shortcut,
                           gtk_callback_action_new ((GtkShortcutFunc) on_toplevel_escape_pressed_cb,
                                                    self,
                                                    NULL));

  self->display_settings = g_settings_new (DISPLAY_SCHEMA);
  g_signal_connect_object (self->display_settings,
                           "changed::night-light-enabled",
                           G_CALLBACK (on_night_light_enabled_changed_cb),
                           self,
                           0);
  on_night_light_enabled_changed_cb (NULL, NULL, self);
}
