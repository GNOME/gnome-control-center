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

#include <gsound.h>

#include "cc-alert-chooser.h"
#include "cc-sound-button.h"
#include "cc-sound-resources.h"

struct _CcAlertChooser {
  GtkBox         parent_instance;

  CcSoundButton *bark_button;
  CcSoundButton *drip_button;
  CcSoundButton *glass_button;
  CcSoundButton *sonar_button;

  GSoundContext *context;
};

G_DEFINE_TYPE (CcAlertChooser, cc_alert_chooser, GTK_TYPE_BOX)

static void
select_sound (CcAlertChooser *self,
              const gchar    *name)
{
  g_autofree gchar *filename = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GError) error = NULL;

  filename = g_strdup_printf ("%s.ogg", name);
  path = g_build_filename (SOUND_DATA_DIR, "gnome", "default", "alerts", filename, NULL);
  if (!gsound_context_play_simple (self->context, NULL, &error,
                                   GSOUND_ATTR_MEDIA_FILENAME, path,
                                   NULL))
    g_warning ("Failed to play alert sound %s: %s", path, error->message);
}

static void
clicked_cb (CcAlertChooser *self,
            CcSoundButton  *button);

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

  G_OBJECT_CLASS (cc_alert_chooser_parent_class)->dispose (object);
}

void
cc_alert_chooser_class_init (CcAlertChooserClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_alert_chooser_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-alert-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, bark_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, drip_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, glass_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooser, sonar_button);

  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
}

void
cc_alert_chooser_init (CcAlertChooser *self)
{
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_sound_get_resource ());

  cc_sound_button_get_type ();

  gtk_widget_init_template (GTK_WIDGET (self));

  self->context = gsound_context_new (NULL, &error);
  if (self->context == NULL)
    g_error ("Failed to make sound context: %s", error->message);
}
