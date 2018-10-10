/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gsound.h>

#include "config.h"
#include "cc-alert-chooser.h"
#include "cc-sound-button.h"
#include "cc-sound-new-resources.h"

#define KEY_SOUNDS_SCHEMA "org.gnome.desktop.sound"

struct _CcAlertChooser
{
  GtkBox         parent_instance;

  CcSoundButton *bark_button;
  CcSoundButton *drip_button;
  CcSoundButton *glass_button;
  CcSoundButton *sonar_button;

  GSoundContext *context;
  GSettings     *sound_settings;
};

static void clicked_cb (CcAlertChooser *self,
                        CcSoundButton  *button);

G_DEFINE_TYPE (CcAlertChooser, cc_alert_chooser, GTK_TYPE_BOX)

#define CUSTOM_THEME_NAME "__custom"

static gchar *
get_theme_dir (void)
{
  return g_build_filename (g_get_user_data_dir (), "sounds", CUSTOM_THEME_NAME, NULL);
}

static gchar *
get_sound_path (const gchar *name)
{
  g_autofree gchar *filename = NULL;

  filename = g_strdup_printf ("%s.ogg", name);
  return g_build_filename (SOUND_DATA_DIR, "gnome", "default", "alerts", filename, NULL);
}

static gchar *
get_alert_name (void)
{
  g_autofree gchar *dir = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileInfo) info = NULL;
  const gchar *target;
  g_autofree gchar *basename = NULL;
  g_autoptr(GError) error = NULL;

  dir = get_theme_dir ();
  path = g_build_filename (dir, "bell-terminal.ogg", NULL);
  file = g_file_new_for_path (path);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  if (info == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("Failed to get sound theme symlink %s: %s", path, error->message);
      return NULL;
    }
  target = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
  if (target == NULL)
    return NULL;

  basename = g_path_get_basename (target);
  if (g_str_has_suffix (basename, ".ogg"))
    basename[strlen (basename) - 4] = '\0';

  return g_steal_pointer (&basename);
}

static void
set_sound_symlink (const gchar *alert_name,
                   const gchar *name)
{
  g_autofree gchar *dir = NULL;
  g_autofree gchar *source_filename = NULL;
  g_autofree gchar *source_path = NULL;
  g_autofree gchar *target_path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  dir = get_theme_dir ();
  source_filename = g_strdup_printf ("%s.ogg", alert_name);
  source_path = g_build_filename (dir, source_filename, NULL);
  target_path = get_sound_path (name);

  file = g_file_new_for_path (source_path);
  if (!g_file_delete (file, NULL, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("Failed to remove existing sound symbolic link %s: %s", source_path, error->message);
    }
  if (!g_file_make_symbolic_link (file, target_path, NULL, &error))
    g_warning ("Failed to make sound theme symbolic link %s->%s: %s", source_path, target_path, error->message);
}

static void
set_custom_theme (CcAlertChooser *self,
                  const gchar    *name)
{
  g_autofree gchar *dir = NULL;
  g_autofree gchar *theme_path = NULL;
  g_autoptr(GKeyFile) theme_file = NULL;
  g_autoptr(GVariant) default_theme = NULL;
  g_autoptr(GError) load_error = NULL;
  g_autoptr(GError) save_error = NULL;

  dir = get_theme_dir ();
  g_mkdir_with_parents (dir, USER_DIR_MODE);

  theme_path = g_build_filename (dir, "index.theme", NULL);

  default_theme = g_settings_get_default_value (self->sound_settings, "theme-name");

  theme_file = g_key_file_new ();
  if (!g_key_file_load_from_file (theme_file, theme_path, G_KEY_FILE_KEEP_COMMENTS, &load_error))
    {
      if (!g_error_matches (load_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_printerr ("Failed to load theme file %s: %s", theme_path, load_error->message);
    }
  g_key_file_set_string (theme_file, "Sound Theme", "Name", _("Custom"));
  if (default_theme != NULL)
    g_key_file_set_string (theme_file, "Sound Theme", "Inherits", g_variant_get_string (default_theme, NULL));
  g_key_file_set_string (theme_file, "Sound Theme", "Directories", ".");

  if (!g_key_file_save_to_file (theme_file, theme_path, &save_error))
    {
      g_warning ("Failed to save theme file %s: %s", theme_path, save_error->message);
    }

  set_sound_symlink ("bell-terminal", name);
  set_sound_symlink ("bell-window-system", name);

  g_settings_set_boolean (self->sound_settings, "event-sounds", TRUE);
  g_settings_set_string (self->sound_settings, "theme-name", CUSTOM_THEME_NAME);
}

static void
select_sound (CcAlertChooser *self,
              const gchar    *name)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GError) error = NULL;

  path = get_sound_path (name);
  if (!gsound_context_play_simple (self->context, NULL, &error,
                                   GSOUND_ATTR_MEDIA_FILENAME, path,
                                   NULL))
    {
      g_warning ("Failed to play alert sound %s: %s", path, error->message);
    }

  set_custom_theme (self, name);
}

static void
set_button (CcAlertChooser *self,
            CcSoundButton  *button,
            gboolean        active)
{
  g_signal_handlers_block_by_func (button, clicked_cb, self);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
  g_signal_handlers_unblock_by_func (button, clicked_cb, self);
}

static void
clicked_cb (CcAlertChooser *self,
            CcSoundButton  *button)
{
  if (button == self->bark_button)
    select_sound (self, "bark");
  else if (button == self->drip_button)
    select_sound (self, "drip");
  else if (button == self->glass_button)
    select_sound (self, "glass");
  else if (button == self->sonar_button)
    select_sound (self, "sonar");

  set_button (self, button, TRUE);
  if (button != self->bark_button)
    set_button (self, self->bark_button, FALSE);
  if (button != self->drip_button)
    set_button (self, self->drip_button, FALSE);
  if (button != self->glass_button)
    set_button (self, self->glass_button, FALSE);
  if (button != self->sonar_button)
    set_button (self, self->sonar_button, FALSE);
}

static void
cc_alert_chooser_dispose (GObject *object)
{
  CcAlertChooser *self = CC_ALERT_CHOOSER (object);

  g_clear_object (&self->context);
  g_clear_object (&self->sound_settings);

  G_OBJECT_CLASS (cc_alert_chooser_parent_class)->dispose (object);
}

void
cc_alert_chooser_class_init (CcAlertChooserClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_alert_chooser_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound-new/cc-alert-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, bark_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, drip_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, glass_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, sonar_button);

  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);

  g_type_ensure (CC_TYPE_SOUND_BUTTON);
}

void
cc_alert_chooser_init (CcAlertChooser *self)
{
  g_autofree gchar *alert_name = NULL;
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_sound_new_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->context = gsound_context_new (NULL, &error);
  if (self->context == NULL)
    g_error ("Failed to make sound context: %s", error->message);

  self->sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);

  alert_name = get_alert_name ();
  if (g_strcmp0 (alert_name, "bark") == 0)
    set_button (self, self->bark_button, TRUE);
  else if (g_strcmp0 (alert_name, "drip") == 0)
    set_button (self, self->drip_button, TRUE);
  else if (g_strcmp0 (alert_name, "glass") == 0)
    set_button (self, self->glass_button, TRUE);
  else if (g_strcmp0 (alert_name, "sonar") == 0)
    set_button (self, self->sonar_button, TRUE);
  else if (alert_name != NULL)
    g_warning ("Current alert sound has unknown name %s", alert_name);
}
