/* cc-firmware-security-boot-dialog.c
 *
 * Copyright (C) 2021 Red Hat, Inc
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
 * Author: Kate Hsuan <hpa@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "cc-firmware-security-boot-dialog.h"

struct _CcFirmwareSecurityBootDialog
{
  GtkDialog         parent;

  GtkWidget        *secure_boot_icon;
  GtkWidget        *secure_boot_title;
};

G_DEFINE_TYPE (CcFirmwareSecurityBootDialog, cc_firmware_security_boot_dialog, GTK_TYPE_DIALOG)

static void
cc_firmware_security_boot_dialog_class_init (CcFirmwareSecurityBootDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/firmware-security/cc-firmware-security-boot-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityBootDialog, secure_boot_title);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityBootDialog, secure_boot_icon);
}

static void
cc_firmware_security_boot_dialog_init (CcFirmwareSecurityBootDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  load_custom_css ("/org/gnome/control-center/firmware-security/security-level.css");
}

GtkWidget *
cc_firmware_security_boot_dialog_new (SecureBootState secure_boot_state)
{
  CcFirmwareSecurityBootDialog *dialog;

  dialog = g_object_new (CC_TYPE_FIRMWARE_SECURITY_BOOT_DIALOG,
                         "use-header-bar", TRUE,
                         NULL);

  switch (secure_boot_state)
    {
    case SECURE_BOOT_STATE_ACTIVE:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL(dialog->secure_boot_title), _("Secure Boot is Active"));
      gtk_widget_add_css_class (dialog->secure_boot_icon, "good");
      break;

    case SECURE_BOOT_STATE_PROBLEMS:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL (dialog->secure_boot_title), _("Secure Boot has Problems"));
      gtk_widget_add_css_class (dialog->secure_boot_icon, "error");
      break;

    case SECURE_BOOT_STATE_INACTIVE:
    case SECURE_BOOT_STATE_UNKNOWN:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL (dialog->secure_boot_title), _("Secure Boot is Inactive"));
      gtk_widget_add_css_class (dialog->secure_boot_icon, "error");
      break;
    }

  return GTK_WIDGET (dialog);
}
