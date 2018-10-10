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

#include "cc-input-test-dialog.h"
#include "cc-sound-resources.h"

struct _CcInputTestDialog
{
  GtkDialog         parent_instance;

  GtkButton        *play_button;
  GtkButton        *record_button;

  GvcMixerUIDevice *device;
};

G_DEFINE_TYPE (CcInputTestDialog, cc_input_test_dialog, GTK_TYPE_DIALOG)

static void
record_button_clicked_cb (CcInputTestDialog *self)
{
  // FIXME
}

static void
play_button_clicked_cb (CcInputTestDialog *self)
{
  // FIXME
}

static void
cc_input_test_dialog_dispose (GObject *object)
{
  CcInputTestDialog *self = CC_INPUT_TEST_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_input_test_dialog_parent_class)->dispose (object);
}

void
cc_input_test_dialog_class_init (CcInputTestDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_input_test_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-input-test-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInputTestDialog, play_button);
  gtk_widget_class_bind_template_child (widget_class, CcInputTestDialog, record_button);

  gtk_widget_class_bind_template_callback (widget_class, play_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, record_button_clicked_cb);
}

void
cc_input_test_dialog_init (CcInputTestDialog *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

CcInputTestDialog *
cc_input_test_dialog_new (GvcMixerUIDevice *device)
{
  CcInputTestDialog *self;
  g_autofree gchar *title = NULL;

  self = g_object_new (cc_input_test_dialog_get_type (),
                       "use-header-bar", 1,
                       NULL);
  self->device = g_object_ref (device);

  title = g_strdup_printf (_("Testing %s"), gvc_mixer_ui_device_get_description (device));
  gtk_header_bar_set_title (GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (self))), title);

  return self;
}
