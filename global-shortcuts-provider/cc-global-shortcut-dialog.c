/* cc-global-shortcut-dialog.c
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
 * Copyright (C) 2020 System76, Inc.
 * Copyright (C) 2022 Purism SPC
 * Copyright © 2024 GNOME Foundation Inc.
 * Copyright © 2024 Red Hat Inc.
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *         Mohammed Sadiq <sadiq@sadiqpk.org>
 *         Dorota Czaplejewicz <gnome@dorotac.eu>
 *         Jonas Ådahl <jadahl@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <adwaita.h>
#include <config.h>
#include <glib/gi18n.h>
#include <gxdp.h>

#include "panels/keyboard/cc-keyboard-resources.h"

#include "cc-global-shortcut-dialog.h"
#include "cc-keyboard-shortcut-group.h"
#include "cc-util.h"

enum
{
  DONE,

  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_0,
  PROP_APP_ID,
  PROP_PARENT_WINDOW_HANDLE,
  PROP_SHORTCUTS,
  N_PROPS,
};

static GParamSpec *props[N_PROPS];

struct _CcGlobalShortcutDialog
{
  AdwWindow parent_instance;

  AdwPreferencesPage *shortcut_list;
  GtkSizeGroup *accelerator_size_group;

  GVariant *app_shortcuts;

  /* Contains `CcKeyboardItem`s */
  GListStore *shortcuts;

  CcKeyboardManager *manager;

  AdwPreferencesGroup *shortcuts_group;

  char *app_id;
  char *parent_window_handle;
  GxdpExternalWindow *external_window;

  gboolean has_new_shortcuts;
};

G_DEFINE_TYPE (CcGlobalShortcutDialog,
               cc_global_shortcut_dialog,
               ADW_TYPE_WINDOW)

static void
populate_shortcuts_model (CcGlobalShortcutDialog *self,
                          const char             *section_id,
                          const char             *section_title)
{
  GtkWidget *shortcuts_group;

  self->shortcuts = g_list_store_new (CC_TYPE_KEYBOARD_ITEM);

  shortcuts_group =
    cc_keyboard_shortcut_group_new (G_LIST_MODEL (self->shortcuts),
                                    section_id, section_title,
                                    self->manager,
                                    self->accelerator_size_group);
  self->shortcuts_group = ADW_PREFERENCES_GROUP (shortcuts_group);
}

static void
shortcut_added_cb (CcGlobalShortcutDialog *self,
                   CcKeyboardItem         *item,
                   const char             *section_id,
                   const char             *section_title)
{
  if (strcmp (section_id, self->app_id) == 0)
    g_list_store_append (self->shortcuts, item);
}

static void
shortcuts_loaded_cb (CcGlobalShortcutDialog *self)
{
  adw_preferences_page_add (self->shortcut_list, self->shortcuts_group);
}

static void
emit_done (CcGlobalShortcutDialog *self,
           gboolean                success)
{
  g_autoptr(GVariant) response = NULL;

  if (success)
    {
      cc_keyboard_manager_store_global_shortcuts (self->manager, self->app_id);
      response = cc_keyboard_manager_get_global_shortcuts (self->manager,
                                                           self->app_id);
    }

  g_signal_emit (self, signals[DONE], 0, response);
}

static gboolean
close_request_cb (CcGlobalShortcutDialog *self)
{
  emit_done (self, FALSE);
  return GDK_EVENT_STOP;
}

static void
on_add_button_clicked_cb (CcGlobalShortcutDialog *self)
{
  emit_done (self, TRUE);
}

static void
cc_global_shortcut_dialog_finalize (GObject *object)
{
  CcGlobalShortcutDialog *self = CC_GLOBAL_SHORTCUT_DIALOG (object);

  g_clear_object (&self->external_window);
  g_clear_object (&self->manager);
  g_clear_object (&self->shortcut_list);
  g_clear_object (&self->accelerator_size_group);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_pointer (&self->parent_window_handle, g_free);
  g_clear_pointer (&self->app_shortcuts, g_variant_unref);

  G_OBJECT_CLASS (cc_global_shortcut_dialog_parent_class)->finalize (object);
}

static void
cc_global_shortcut_dialog_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  CcGlobalShortcutDialog *self = CC_GLOBAL_SHORTCUT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      self->app_id = g_value_dup_string (value);
      break;
    case PROP_PARENT_WINDOW_HANDLE:
      self->parent_window_handle = g_value_dup_string (value);
      break;
    case PROP_SHORTCUTS:
      self->app_shortcuts = g_value_dup_variant (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GVariant *
lookup_in_settings_variant (GVariant   *settings,
                            const char *shortcut_id)
{
  GVariantIter iter;
  g_autofree char *key = NULL;
  g_autoptr(GVariant) value = NULL;

  g_variant_iter_init (&iter, settings);

  while (g_variant_iter_next (&iter, "(s@a{sv})", &key, &value))
    {
      g_autofree char *shortcut = g_steal_pointer (&key);
      g_autoptr(GVariant) config = g_steal_pointer (&value);

      if (g_strcmp0 (shortcut_id, shortcut) == 0)
        return g_steal_pointer (&config);
    }

  return NULL;
}

static GVariant *
app_shortcuts_to_settings_variant (GVariant *app_shortcuts,
                                   GVariant *old_settings,
                                   gboolean *has_new_shortcuts)
{
  g_auto(GVariantBuilder) builder =
    G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(sa{sv})"));
  g_autoptr(GVariant) value = NULL;
  g_autofree char *key = NULL;
  GVariantIter iter;

  g_variant_iter_init (&iter, app_shortcuts);

  while (g_variant_iter_next (&iter, "(s@a{sv})", &key, &value))
    {
      g_autofree char *shortcut_id = g_steal_pointer (&key);
      g_autoptr(GVariant) prefs = g_steal_pointer (&value);
      g_autoptr(GVariant) setting = NULL;

      setting = lookup_in_settings_variant (old_settings, shortcut_id);

      if (setting)
        {
          /* Shortcut was configured previously */
          g_variant_builder_add (&builder, "(s@a{sv})", shortcut_id, setting);
        }
      else
        {
          g_auto(GVariantBuilder) new_shortcut =
            G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
          g_auto(GVariantDict) prefs_dict = G_VARIANT_DICT_INIT (prefs);
          g_autoptr(GVariant) description = NULL, preferred_trigger = NULL;
          g_autoptr(GVariant) shortcuts = NULL;

          /* Extract app preferences for new shortcut */
          description =
            g_variant_dict_lookup_value (&prefs_dict, "description",
                                         G_VARIANT_TYPE_STRING);
          if (!description)
            continue;

          g_variant_builder_add (&new_shortcut, "{sv}", "description",
                                 g_variant_ref_sink (description));
          preferred_trigger =
            g_variant_dict_lookup_value (&prefs_dict, "preferred_trigger",
                                         G_VARIANT_TYPE_STRING);
          shortcuts = g_variant_new_array (G_VARIANT_TYPE_STRING,
                                           preferred_trigger ? &preferred_trigger : NULL,
                                           preferred_trigger ? 1 : 0);
          g_variant_builder_add (&new_shortcut, "{sv}", "shortcuts",
                                 g_variant_ref_sink (shortcuts));

          g_variant_builder_add (&builder, "(s@a{sv})", shortcut_id,
                                 g_variant_builder_end (&new_shortcut));
          *has_new_shortcuts = TRUE;
        }
    }

  return g_variant_builder_end (&builder);
}

static void
cc_global_shortcut_dialog_constructed (GObject *object)
{
  CcGlobalShortcutDialog *self = CC_GLOBAL_SHORTCUT_DIALOG (object);
  g_autoptr(GVariant) saved_shortcuts = NULL, shortcuts = NULL;

  saved_shortcuts = cc_keyboard_manager_get_global_shortcuts (self->manager,
                                                              self->app_id);

  shortcuts = app_shortcuts_to_settings_variant (self->app_shortcuts,
                                                 saved_shortcuts,
                                                 &self->has_new_shortcuts);

  populate_shortcuts_model (self, self->app_id,
                            cc_util_app_id_to_display_name (self->app_id));

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (shortcut_added_cb),
                           self, G_CONNECT_SWAPPED);
  /* Shortcuts can not get removed from this dialog,
   * it's an offer, and offers don't change when you look away. */
  g_signal_connect_object (self->manager,
                           "shortcuts-loaded",
                           G_CALLBACK (shortcuts_loaded_cb),
                           self, G_CONNECT_SWAPPED);

  cc_keyboard_manager_load_global_shortcuts (self->manager,
                                             self->app_id,
                                             shortcuts);

  G_OBJECT_CLASS (cc_global_shortcut_dialog_parent_class)->constructed (object);
}

static void
cc_global_shortcut_dialog_class_init (CcGlobalShortcutDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_global_shortcut_dialog_finalize;
  object_class->set_property = cc_global_shortcut_dialog_set_property;
  object_class->constructed = cc_global_shortcut_dialog_constructed;

  props[PROP_APP_ID] =
    g_param_spec_string ("app-id", NULL, NULL, NULL,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  props[PROP_PARENT_WINDOW_HANDLE] =
    g_param_spec_string ("parent-window-handle", NULL, NULL, NULL,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  props[PROP_SHORTCUTS] =
    g_param_spec_variant ("shortcuts", NULL, NULL,
                          G_VARIANT_TYPE ("a(sa{sv})"), NULL,
                          G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, props);

  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);

  g_resources_register (cc_keyboard_get_resource ());

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/global-shortcuts-provider/"
                                               "cc-global-shortcut-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        CcGlobalShortcutDialog,
                                        shortcut_list);
  gtk_widget_class_bind_template_child (widget_class,
                                        CcGlobalShortcutDialog,
                                        accelerator_size_group);

  gtk_widget_class_bind_template_callback (widget_class,
                                           on_add_button_clicked_cb);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "window.close", NULL);
}

static void
cc_global_shortcut_dialog_init (CcGlobalShortcutDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = cc_keyboard_manager_new ();

  g_signal_connect (self, "close-request",
                    G_CALLBACK (close_request_cb), NULL);
}

CcGlobalShortcutDialog *
cc_global_shortcut_dialog_new (const char *app_id,
                               const char *parent_window,
                               GVariant   *shortcuts)
{
  return g_object_new (CC_TYPE_GLOBAL_SHORTCUT_DIALOG,
                       "app-id", app_id,
                       "parent-window-handle", parent_window,
                       "shortcuts", shortcuts,
                       NULL);
}

void
cc_global_shortcut_dialog_present (CcGlobalShortcutDialog *self)
{
  if (!self->has_new_shortcuts)
    {
      emit_done (self, TRUE);
      return;
    }

  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    {
      self->external_window =
        gxdp_external_window_new_from_handle (self->parent_window_handle);

      if (self->external_window)
        {
          GtkNative *native;
          GdkSurface *surface;

          gtk_widget_realize (GTK_WIDGET (self));

          native = gtk_widget_get_native (GTK_WIDGET (self));
          surface = gtk_native_get_surface (native);

          gxdp_external_window_set_parent_of (self->external_window,
                                              surface);
        }

      gtk_window_present (GTK_WINDOW (self));
    }
}
