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

#include "cc-input-test-dialog.h"
#include "cc-sound-resources.h"

struct _CcInputTestDialog
{
  GtkDialog parent_instance;
};

G_DEFINE_TYPE (CcInputTestDialog, cc_input_test_dialog, GTK_TYPE_DIALOG)

static void
cc_input_test_dialog_dispose (GObject *object)
{
  CcInputTestDialog *self = CC_INPUT_TEST_DIALOG (object);

  G_OBJECT_CLASS (cc_input_test_dialog_parent_class)->dispose (object);
}

void
cc_input_test_dialog_class_init (CcInputTestDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_input_test_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-input-test-dialog.ui");
}

void
cc_input_test_dialog_init (CcInputTestDialog *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

CcInputTestDialog *
cc_input_test_dialog_new (void)
{
  return g_object_new (cc_input_test_dialog_get_type (),
                       "use-header-bar", 1,
                       NULL);
}
