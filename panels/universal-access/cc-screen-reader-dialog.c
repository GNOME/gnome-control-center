/*
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "cc-screen-reader-dialog.h"

#define APPLICATION_SETTINGS         "org.gnome.desktop.a11y.applications"
#define KEY_SCREEN_READER_ENABLED    "screen-reader-enabled"

struct _CcScreenReaderDialog
{
  GtkDialog parent;

  GtkSwitch *enable_switch;

  GSettings *application_settings;
};

G_DEFINE_TYPE (CcScreenReaderDialog, cc_screen_reader_dialog, GTK_TYPE_DIALOG);

static void
cc_screen_reader_dialog_dispose (GObject *object)
{
  CcScreenReaderDialog *self = CC_SCREEN_READER_DIALOG (object);

  g_clear_object (&self->application_settings);

  G_OBJECT_CLASS (cc_screen_reader_dialog_parent_class)->dispose (object);
}

static void
cc_screen_reader_dialog_class_init (CcScreenReaderDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_screen_reader_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-screen-reader-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcScreenReaderDialog, enable_switch);
}

static void
cc_screen_reader_dialog_init (CcScreenReaderDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->application_settings = g_settings_new (APPLICATION_SETTINGS);

  g_settings_bind (self->application_settings, KEY_SCREEN_READER_ENABLED,
                   self->enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

CcScreenReaderDialog *
cc_screen_reader_dialog_new (void)
{
  return g_object_new (cc_screen_reader_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
