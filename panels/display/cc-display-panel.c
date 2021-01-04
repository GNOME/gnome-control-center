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
#include <handy.h>

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
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

typedef enum {
  CC_DISPLAY_CONFIG_SINGLE,
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
  GtkDialog *night_light_dialog;

  UpClient *up_client;
  gboolean lid_is_closed;

  GDBusProxy *shell_proxy;

  gchar     *main_title;
  GtkWidget *main_titlebar;
  GtkWidget *apply_titlebar;
  GtkWidget *apply_titlebar_apply;
  GtkWidget *apply_titlebar_warning;

  GListStore     *primary_display_list;
  GtkListStore   *output_selection_list;

  GtkWidget      *arrangement_frame;
  GtkAlignment   *arrangement_bin;
  GtkRadioButton *config_type_join;
  GtkRadioButton *config_type_mirror;
  GtkRadioButton *config_type_single;
  GtkWidget      *config_type_switcher_frame;
  GtkLabel       *current_output_label;
  GtkWidget      *display_settings_frame;
  GtkBox         *multi_selection_box;
  GtkSwitch      *output_enabled_switch;
  GtkComboBox    *output_selection_combo;
  GtkStack       *output_selection_stack;
  GtkButtonBox   *output_selection_two_buttonbox;
  GtkRadioButton *output_selection_two_first;
  GtkRadioButton *output_selection_two_second;
  HdyComboRow    *primary_display_row;
  GtkWidget      *stack_switcher;
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

  if (n_active_outputs == 1)
    return CC_DISPLAY_CONFIG_SINGLE;

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
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (panel->config_type_single)))
    return CC_DISPLAY_CONFIG_SINGLE;
  else
    g_assert_not_reached ();
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
    case CC_DISPLAY_CONFIG_SINGLE:
      g_debug ("Creating new single config");
      /* Disable all but the current primary output */
      cc_display_config_set_cloning (panel->current_config, FALSE);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          /* Select the current primary output as the active one */
          if (cc_display_monitor_is_primary (output))
            {
              cc_display_monitor_set_active (output, TRUE);
              cc_display_monitor_set_mode (output, cc_display_monitor_get_preferred_mode (output));
              set_current_output (panel, output, FALSE);
            }
          else
            {
              cc_display_monitor_set_active (output, FALSE);
              cc_display_monitor_set_mode (output, cc_display_monitor_get_preferred_mode (output));
            }
        }
      break;

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
        GList *modes = cc_display_config_get_cloning_modes (panel->current_config);
        gint bw, bh;
        CcDisplayMode *best = NULL;

        /* Turn on cloning and select the best mode we can find by default */
        cc_display_config_set_cloning (panel->current_config, TRUE);

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

        /* Take the preferred scale by default, */
        scale = cc_display_mode_get_preferred_scale (best);
        /* but prefer the old primary scale if that is valid. */
        if (current_primary &&
            cc_display_config_is_scaled_mode_valid (panel->current_config,
                                                    best,
                                                    old_primary_scale))
          scale = old_primary_scale;

        for (l = outputs; l; l = l->next)
          {
            CcDisplayMonitor *output = l->data;

            cc_display_monitor_set_mode (output, best);
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
    case CC_DISPLAY_CONFIG_SINGLE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panel->config_type_single), TRUE);
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
    return monitor_labeler_hide (self);

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

  reset_titlebar (CC_DISPLAY_PANEL (object));

  if (self->focus_id)
    {
      self->focus_id = 0;
      monitor_labeler_hide (CC_DISPLAY_PANEL (object));
    }

  g_clear_object (&self->manager);
  g_clear_object (&self->current_config);
  g_clear_object (&self->up_client);

  g_clear_object (&self->shell_proxy);

  g_clear_pointer ((GtkWidget **) &self->night_light_dialog, gtk_widget_destroy);

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
on_config_type_toggled_cb (CcDisplayPanel *panel,
                           GtkRadioButton *btn)
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
on_night_light_list_box_row_activated_cb (CcDisplayPanel *panel)
{
  GtkWindow *toplevel;
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));

  if (!panel->night_light_dialog)
    {
      GtkWidget *content_area;

      panel->night_light_dialog = (GtkDialog *)gtk_dialog_new ();

      content_area = gtk_dialog_get_content_area (panel->night_light_dialog);
      gtk_container_add (GTK_CONTAINER (content_area),
                         GTK_WIDGET (panel->night_light_page));
      gtk_widget_show (GTK_WIDGET (panel->night_light_page));
    }

  gtk_window_set_transient_for (GTK_WINDOW (panel->night_light_dialog), toplevel);
  gtk_window_present (GTK_WINDOW (panel->night_light_dialog));
}

static void
on_output_enabled_active_changed_cb (CcDisplayPanel *panel)
{
  gboolean active;

  if (!panel->current_output)
    return;

  active = gtk_switch_get_active (panel->output_enabled_switch);

  if (cc_display_monitor_is_active (panel->current_output) == active)
    return;

  cc_display_monitor_set_active (panel->current_output, active);

  /* Prevent the invalid configuration of disabling the last monitor
   * by switching on a different one. */
  if (config_get_current_type (panel) == CC_DISPLAY_CONFIG_INVALID_NONE)
    {
      GList *outputs, *l;

      outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = CC_DISPLAY_MONITOR (l->data);

          if (output == panel->current_output)
            continue;

          if (!cc_display_monitor_is_usable (output))
            continue;

          cc_display_monitor_set_active (output, TRUE);
          cc_display_monitor_set_primary (output, TRUE);
          break;
        }
    }

  /* Changing the active state requires a UI rebuild. */
  rebuild_ui (panel);
}

static void
on_output_selection_combo_changed_cb (CcDisplayPanel *panel)
{
  GtkTreeIter iter;
  g_autoptr(CcDisplayMonitor) output = NULL;

  if (!panel->current_config)
    return;

  if (!gtk_combo_box_get_active_iter (panel->output_selection_combo, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (panel->output_selection_list), &iter,
                      1, &output,
                      -1);

  set_current_output (panel, output, FALSE);
}

static void
on_output_selection_two_toggled_cb (CcDisplayPanel *panel, GtkRadioButton *btn)
{
  CcDisplayMonitor *output;

  if (panel->rebuilding_counter > 0)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    return;

  output = g_object_get_data (G_OBJECT (btn), "display");

  /* Stay in single mode when we are in single mode.
   * This UI must never cause a switch between the configuration type.
   * this is in contrast to the combobox monitor selection, which may
   * switch to a disabled output both in SINGLE/MULTI mode without
   * anything changing.
   */
  if (cc_panel_get_selected_type (panel) == CC_DISPLAY_CONFIG_SINGLE)
    {
      if (panel->current_output)
        cc_display_monitor_set_active (panel->current_output, FALSE);
      if (output)
        cc_display_monitor_set_active (output, TRUE);

      update_apply_button (panel);
    }

  set_current_output (panel, g_object_get_data (G_OBJECT (btn), "display"), FALSE);
}

static void
on_primary_display_selected_index_changed_cb (CcDisplayPanel *panel)
{
  gint idx = hdy_combo_row_get_selected_index (panel->primary_display_row);
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
cc_display_panel_constructed (GObject *object)
{
  g_signal_connect_object (cc_panel_get_shell (CC_PANEL (object)), "notify::active-panel",
                           G_CALLBACK (active_panel_changed), object, G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->constructed (object);
}

static const char *
cc_display_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-display";
}

static GtkWidget *
cc_display_panel_get_title_widget (CcPanel *panel)
{
  CcDisplayPanel *self = CC_DISPLAY_PANEL (panel);

  return self->stack_switcher;
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_NIGHT_LIGHT_PAGE);

  panel_class->get_help_uri = cc_display_panel_get_help_uri;
  panel_class->get_title_widget = cc_display_panel_get_title_widget;

  object_class->constructed = cc_display_panel_constructed;
  object_class->dispose = cc_display_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/display/cc-display-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, arrangement_frame);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, arrangement_bin);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_switcher_frame);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_join);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_mirror);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, config_type_single);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, current_output_label);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, display_settings_frame);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, multi_selection_box);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, night_light_page);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_enabled_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_selection_combo);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_selection_stack);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_selection_two_buttonbox);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_selection_two_first);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, output_selection_two_second);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, primary_display_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplayPanel, stack_switcher);

  gtk_widget_class_bind_template_callback (widget_class, on_config_type_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_night_light_list_box_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_output_enabled_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_output_selection_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_output_selection_two_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_primary_display_selected_index_changed_cb);
}

static void
set_current_output (CcDisplayPanel   *panel,
                    CcDisplayMonitor *output,
                    gboolean          force)
{
  GtkTreeIter iter;
  gboolean changed;

  /* Note, this function is also called if the internal UI needs updating after a rebuild. */
  changed = (output != panel->current_output);

  if (!changed && !force)
    return;

  panel->rebuilding_counter++;

  panel->current_output = output;

  if (panel->current_output)
    {
      gtk_label_set_text (panel->current_output_label, cc_display_monitor_get_ui_name (panel->current_output));
      gtk_switch_set_active (panel->output_enabled_switch, cc_display_monitor_is_active (panel->current_output));
      gtk_widget_set_sensitive (GTK_WIDGET (panel->output_enabled_switch), cc_display_monitor_is_usable (panel->current_output));
    }
  else
    {
      gtk_label_set_text (panel->current_output_label, "");
      gtk_switch_set_active (panel->output_enabled_switch, FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (panel->output_enabled_switch), FALSE);
    }

  if (g_object_get_data (G_OBJECT (panel->output_selection_two_first), "display") == output)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panel->output_selection_two_first), TRUE);
  if (g_object_get_data (G_OBJECT (panel->output_selection_two_second), "display") == output)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panel->output_selection_two_second), TRUE);

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (panel->output_selection_list), &iter);
  while (gtk_list_store_iter_is_valid (panel->output_selection_list, &iter))
    {
      g_autoptr(CcDisplayMonitor) o = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (panel->output_selection_list), &iter,
                          1, &o,
                          -1);

      if (o == panel->current_output)
        {
          gtk_combo_box_set_active_iter (panel->output_selection_combo, &iter);
          break;
        }

      gtk_tree_model_iter_next (GTK_TREE_MODEL (panel->output_selection_list), &iter);
    }

  if (changed)
    {
      cc_display_settings_set_selected_output (panel->settings, panel->current_output);
      cc_display_arrangement_set_selected_output (panel->arrangement, panel->current_output);
    }

  panel->rebuilding_counter--;
}

static void
rebuild_ui (CcDisplayPanel *panel)
{
  guint n_outputs, n_active_outputs, n_usable_outputs;
  GList *outputs, *l;
  CcDisplayConfigType type;

  panel->rebuilding_counter++;

  g_list_store_remove_all (panel->primary_display_list);
  gtk_list_store_clear (panel->output_selection_list);

  if (!panel->current_config)
    {
      panel->rebuilding_counter--;
      return;
    }

  n_active_outputs = 0;
  n_usable_outputs = 0;
  outputs = cc_display_config_get_ui_sorted_monitors (panel->current_config);
  for (l = outputs; l; l = l->next)
    {
      GtkTreeIter iter;
      CcDisplayMonitor *output = l->data;

      gtk_list_store_append (panel->output_selection_list, &iter);
      gtk_list_store_set (panel->output_selection_list,
                          &iter,
                          0, cc_display_monitor_get_ui_number_name (output),
                          1, output,
                          -1);

      if (!cc_display_monitor_is_usable (output))
        continue;

      n_usable_outputs += 1;

      if (n_usable_outputs == 1)
        {
          gtk_button_set_label (GTK_BUTTON (panel->output_selection_two_first),
                                cc_display_monitor_get_ui_name (output));
          g_object_set_data (G_OBJECT (panel->output_selection_two_first),
                             "display",
                             output);
        }
      else if (n_usable_outputs == 2)
        {
          gtk_button_set_label (GTK_BUTTON (panel->output_selection_two_second),
                                cc_display_monitor_get_ui_name (output));
          g_object_set_data (G_OBJECT (panel->output_selection_two_second),
                             "display",
                             output);
        }

      if (cc_display_monitor_is_active (output))
        {
          n_active_outputs += 1;

          g_list_store_append (panel->primary_display_list, output);
          if (cc_display_monitor_is_primary (output))
            hdy_combo_row_set_selected_index (panel->primary_display_row,
                                              g_list_model_get_n_items (G_LIST_MODEL (panel->primary_display_list)) - 1);

          /* Ensure that an output is selected; note that this doesn't ensure
           * the selected output is any useful (i.e. when switching types).
           */
          if (!panel->current_output)
            set_current_output (panel, output, FALSE);
        }
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

      gtk_widget_set_visible (panel->config_type_switcher_frame, TRUE);
      gtk_widget_set_visible (panel->arrangement_frame, type == CC_DISPLAY_CONFIG_JOIN);

      /* We need a switcher except in CLONE mode */
      if (type == CC_DISPLAY_CONFIG_CLONE)
        gtk_stack_set_visible_child (panel->output_selection_stack, GTK_WIDGET (panel->current_output_label));
      else
        gtk_stack_set_visible_child (panel->output_selection_stack, GTK_WIDGET (panel->output_selection_two_buttonbox));
    }
  else if (n_usable_outputs > 1)
    {
      /* We have more than one usable monitor. In this case there is no chooser,
       * and we always show the arrangement widget even if we are in SINGLE mode.
       */
      gtk_widget_set_visible (panel->config_type_switcher_frame, FALSE);
      gtk_widget_set_visible (panel->arrangement_frame, TRUE);

      /* Mirror is also invalid as it cannot be configured using this UI. */
      if (type == CC_DISPLAY_CONFIG_CLONE || type > CC_DISPLAY_CONFIG_LAST_VALID)
        type = CC_DISPLAY_CONFIG_JOIN;

      gtk_stack_set_visible_child (panel->output_selection_stack, GTK_WIDGET (panel->multi_selection_box));
    }
  else
    {
      /* We only have a single usable monitor, show neither configuration type
       * switcher nor arrangement widget and ensure we really are in SINGLE
       * mode (and not e.g. mirroring across one display) */
      type = CC_DISPLAY_CONFIG_SINGLE;

      gtk_widget_set_visible (panel->config_type_switcher_frame, FALSE);
      gtk_widget_set_visible (panel->arrangement_frame, FALSE);

      gtk_stack_set_visible_child (panel->output_selection_stack, GTK_WIDGET (panel->current_output_label));
    }

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
  gtk_list_store_clear (panel->output_selection_list);

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
}

static void
mapped_cb (CcDisplayPanel *panel)
{
  CcShell *shell;
  GtkWidget *toplevel;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  toplevel = cc_shell_get_toplevel (shell);
  if (toplevel && !panel->focus_id)
    panel->focus_id = g_signal_connect_object (toplevel, "notify::has-toplevel-focus",
                                               G_CALLBACK (dialog_toplevel_focus_changed), panel, G_CONNECT_SWAPPED);
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
  GtkCellRenderer *renderer;

  g_resources_register (cc_display_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->arrangement = cc_display_arrangement_new (NULL);

  gtk_widget_show (GTK_WIDGET (self->arrangement));
  gtk_widget_set_size_request (GTK_WIDGET (self->arrangement), 400, 175);
  gtk_container_add (GTK_CONTAINER (self->arrangement_bin), GTK_WIDGET (self->arrangement));

  g_signal_connect_object (self->arrangement, "updated",
			   G_CALLBACK (update_apply_button), self,
			   G_CONNECT_SWAPPED);
  g_signal_connect_object (self->arrangement, "notify::selected-output",
			   G_CALLBACK (on_arrangement_selected_ouptut_changed_cb), self,
			   G_CONNECT_SWAPPED);

  self->settings = cc_display_settings_new ();
  gtk_widget_show (GTK_WIDGET (self->settings));
  gtk_container_add (GTK_CONTAINER (self->display_settings_frame), GTK_WIDGET (self->settings));
  g_signal_connect_object (self->settings, "updated",
                           G_CALLBACK (on_monitor_settings_updated_cb), self,
                           G_CONNECT_SWAPPED);

  self->primary_display_list = g_list_store_new (CC_TYPE_DISPLAY_MONITOR);
  hdy_combo_row_bind_name_model (self->primary_display_row,
                                 G_LIST_MODEL (self->primary_display_list),
                                 (HdyComboRowGetNameFunc) cc_display_monitor_dup_ui_number_name,
                                 NULL, NULL);

  self->output_selection_list = gtk_list_store_new (2, G_TYPE_STRING, CC_TYPE_DISPLAY_MONITOR);
  gtk_combo_box_set_model (self->output_selection_combo, GTK_TREE_MODEL (self->output_selection_list));
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->output_selection_combo));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->output_selection_combo),
                              renderer,
                              TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->output_selection_combo),
                                 renderer,
                                 "text",
                                 0);
  gtk_cell_renderer_set_visible (renderer, TRUE);

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
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
