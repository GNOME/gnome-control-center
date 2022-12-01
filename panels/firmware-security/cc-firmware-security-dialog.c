/* cc-firmware-security-dialog.c
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

#include "cc-firmware-security-panel.h"
#include "cc-firmware-security-dialog.h"
#include "cc-firmware-security-utils.h"

struct _CcFirmwareSecurityDialog
{
  AdwWindow            parent;

  GtkWidget           *dialog_hsi_circle_box;
  GtkWidget           *dialog_hsi_circle_number;

  GtkWidget           *hsi1_icon;
  GtkWidget           *hsi2_icon;
  GtkWidget           *hsi3_icon;
  GtkWidget           *hsi1_title;
  GtkWidget           *hsi2_title;
  GtkWidget           *hsi3_title;

  GtkWidget           *firmware_security_dialog_title_label;
  GtkWidget           *firmware_security_dialog_body_label;
  GtkWidget           *firmware_security_dialog_min_row;
  GtkWidget           *firmware_security_dialog_basic_row;
  GtkWidget           *firmware_security_dialog_extend_row;
  GtkWidget           *firmware_security_dialog_hsi1_pg;
  GtkWidget           *firmware_security_dialog_hsi2_pg;
  GtkWidget           *firmware_security_dialog_hsi3_pg;
  GtkWidget           *firmware_security_dialog_hsi_label;
  AdwLeaflet          *leaflet;
  AdwWindowTitle      *second_page_title;

  gboolean             is_created;

  GHashTable          *hsi1_dict;
  GHashTable          *hsi2_dict;
  GHashTable          *hsi3_dict;
  GHashTable          *hsi4_dict;

  guint                hsi_number;
};

G_DEFINE_TYPE (CcFirmwareSecurityDialog, cc_firmware_security_dialog, ADW_TYPE_WINDOW)

static void
set_dialog_item_layer1 (CcFirmwareSecurityDialog *self,
                        const gchar              *circle_str,
                        const gchar              *title,
                        const gchar              *body)
{
  g_autofree gchar *str = NULL;

  gtk_label_set_label (GTK_LABEL (self->dialog_hsi_circle_number), circle_str);
  gtk_label_set_text (GTK_LABEL (self->firmware_security_dialog_title_label), title);
  gtk_label_set_text (GTK_LABEL (self->firmware_security_dialog_body_label), body);

  if (self->hsi_number == G_MAXUINT)
    {
        gtk_widget_add_css_class (self->dialog_hsi_circle_box, "level1");
        gtk_widget_add_css_class (self->dialog_hsi_circle_number, "hsi1");
        gtk_widget_hide (self->hsi1_icon);
        gtk_widget_hide (self->hsi2_icon);
        gtk_widget_hide (self->hsi3_icon);
        gtk_widget_hide (self->hsi1_title);
        gtk_widget_hide (self->hsi2_title);
        gtk_widget_hide (self->hsi3_title);
        gtk_widget_hide (self->firmware_security_dialog_hsi_label);
        return;
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi1_icon), self->hsi_number >= 1 ? "emblem-ok" : "process-stop");
  gtk_label_set_text (GTK_LABEL (self->hsi1_title), self->hsi_number >= 1 ? _("Passed") : _("Failed"));
  gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi2_icon), self->hsi_number >= 2 ? "emblem-ok" : "process-stop");
  gtk_label_set_text (GTK_LABEL (self->hsi2_title), self->hsi_number >= 2 ? _("Passed") : _("Failed"));
  gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi3_icon), self->hsi_number >= 3 ? "emblem-ok" : "process-stop");
  gtk_label_set_text (GTK_LABEL (self->hsi3_title), self->hsi_number >= 3 ? _("Passed") : _("Failed"));

  gtk_widget_add_css_class (self->firmware_security_dialog_min_row,
                            self->hsi_number >= 1 ? "success-hsi-icon" : "error-hsi-icon");
  gtk_widget_add_css_class (self->firmware_security_dialog_min_row,
                            self->hsi_number >= 1 ? "success-title" : "error-title");
  gtk_widget_add_css_class (self->firmware_security_dialog_basic_row,
                            self->hsi_number >= 2 ? "success-hsi-icon" : "error-hsi-icon");
  gtk_widget_add_css_class (self->firmware_security_dialog_basic_row,
                            self->hsi_number >= 2 ? "success-title" : "error-title");
  gtk_widget_add_css_class (self->firmware_security_dialog_extend_row,
                            self->hsi_number >= 3 ? "success-hsi-icon" : "error-hsi-icon");
  gtk_widget_add_css_class (self->firmware_security_dialog_extend_row,
                            self->hsi_number >= 3 ? "success-title" : "error-title");

  switch (self->hsi_number)
    {
      case 0:
        gtk_widget_add_css_class (self->dialog_hsi_circle_box, "level0");
        gtk_widget_add_css_class (self->dialog_hsi_circle_number, "hsi0");
        break;
      case 1:
        gtk_widget_add_css_class (self->dialog_hsi_circle_box, "level1");
        gtk_widget_add_css_class (self->dialog_hsi_circle_number, "hsi1");
        break;
      case 2:
        gtk_widget_add_css_class (self->dialog_hsi_circle_box, "level2");
        gtk_widget_add_css_class (self->dialog_hsi_circle_number, "hsi2");
        break;
      case 3:
      case 4:
        gtk_widget_add_css_class (self->dialog_hsi_circle_box, "level3");
        gtk_widget_add_css_class (self->dialog_hsi_circle_number, "hsi3");
        break;
    }

  /* TRANSLATORS: HSI stands for Host Security ID and device refers to the computer as a whole */
  str = g_strdup_printf (_("Device conforms to HSI level %d"), self->hsi_number);
  gtk_label_set_text (GTK_LABEL (self->firmware_security_dialog_hsi_label), str);
}

static void
update_dialog (CcFirmwareSecurityDialog *self)
{
  switch (self->hsi_number)
    {
    case 0:
      set_dialog_item_layer1 (self,
                              "0",
                              _("Security Level 0"),
                              _("This device has no protection against hardware security issues. This could "
                                "be because of a hardware or firmware configuration issue. It is "
                                "recommended to contact your IT support provider."));
      break;

    case 1:
      set_dialog_item_layer1 (self,
                              "1",
                              _("Security Level 1"),
                              _("This device has minimal protection against hardware security issues. This "
                                "is the lowest device security level and only provides protection against "
                                "simple security threats."));
      break;

    case 2:
      set_dialog_item_layer1 (self,
                              "2",
                              _("Security Level 2"),
                              _("This device has basic protection against hardware security issues. This "
                                "provides protection against some common security threats."));
      break;

    case 3:
    case 4:
      set_dialog_item_layer1 (self,
                              "3",
                              _("Security Level 3"),
                              _("This device has extended protection against hardware security issues. This "
                                "is the highest device security level and provides protection against "
                                "advanced security threats."));
      break;

    default:
      set_dialog_item_layer1 (self,
                              "?",
                              _("Security Level"),
                              _("Security levels are not available for this device."));
    }
}

static gchar *
fu_security_attr_get_description_for_dialog (FwupdSecurityAttr *attr)
{
  GString *str = g_string_new (attr->description);

  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM &&
      attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW &&
      attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s %s",
                              /* TRANSLATORS: hardware manufacturer as in OEM */
                              _("Contact your hardware manufacturer for help with security updates."),
                              /* TRANSLATORS: support technician as in someone with root */
                              _("It might be possible to resolve this issue in the device’s UEFI "
                                "firmware settings, or by a support technician."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM &&
           attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s %s",
                              /* TRANSLATORS: hardware manufacturer as in OEM */
                              _("Contact your hardware manufacturer for help with security updates."),
                              _("It might be possible to resolve this issue in the device’s UEFI firmware settings."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: hardware manufacturer as in OEM */
                              _("Contact your hardware manufacturer for help with security updates."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s",
                              _("It might be possible to resolve this issue in the device’s UEFI firmware settings."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_OS)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: support technician as in someone with root */
                              _("It might be possible for a support technician to resolve this issue."));
    }

  return g_string_free (str, FALSE);
}

static GtkWidget *
hsi_create_pg_row (const gchar *icon_name,
                   const gchar *style,
                   FwupdSecurityAttr *attr)
{
  GtkWidget *row;
  GtkWidget *status_icon;
  GtkWidget *status_label;
  GtkWidget *actions_parent;
  const gchar *result_str = NULL;

  row = adw_expander_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), attr->title);

  result_str = fwupd_security_attr_result_to_string(attr->result);
  if (result_str)
    {
      status_label = gtk_label_new (result_str);
      if (firmware_security_attr_has_flag (attr, FWUPD_SECURITY_ATTR_FLAG_SUCCESS))
          status_icon = gtk_image_new_from_icon_name ("emblem-ok");
      else
          status_icon = gtk_image_new_from_icon_name ("process-stop");

      adw_expander_row_add_action (ADW_EXPANDER_ROW (row), status_label);
      adw_expander_row_add_action (ADW_EXPANDER_ROW (row), status_icon);

      gtk_widget_add_css_class (status_icon, "icon");
      gtk_widget_add_css_class (status_label, "hsi_label");

      actions_parent = gtk_widget_get_parent (status_icon);
      gtk_box_set_spacing (GTK_BOX (actions_parent), 6);
      gtk_widget_set_margin_end (actions_parent, 12);
    }

  if (attr->description != NULL)
    {
      GtkWidget *subrow = adw_action_row_new ();
      g_autofree gchar *str = fu_security_attr_get_description_for_dialog (attr);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (subrow), str);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (row), subrow);
      gtk_widget_add_css_class (subrow, "security-description-row");
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (row), FALSE);
      gtk_widget_add_css_class (row, "hide-arrow");
    }

  return row;
}

static void
update_hsi_listbox (CcFirmwareSecurityDialog *self,
                    const gint                hsi_level)
{
  g_autoptr (GList) hash_keys = NULL;
  GHashTable *hsi_dict = NULL;
  GtkWidget *pg_row;
  GtkWidget *hsi_pg;

  switch (hsi_level)
    {
      case 1:
        hsi_dict = self->hsi1_dict;
        hsi_pg = self->firmware_security_dialog_hsi1_pg;
        break;
      case 2:
        hsi_dict = self->hsi2_dict;
        hsi_pg = self->firmware_security_dialog_hsi2_pg;
        break;
      case 3:
        hsi_dict = self->hsi3_dict;
        hsi_pg = self->firmware_security_dialog_hsi3_pg;
        break;
      case 4:
        hsi_dict = self->hsi4_dict;
        hsi_pg = self->firmware_security_dialog_hsi3_pg;
        break;
    }

  hash_keys = g_hash_table_get_keys (hsi_dict);
  for (GList *item = g_list_first (hash_keys); item != NULL; item = g_list_next (item))
    {
      FwupdSecurityAttr *attr = g_hash_table_lookup (hsi_dict, item->data);
      if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_SUPPORTED_CPU) == 0)
        continue;
      if (attr->title == NULL)
        continue;
      if (firmware_security_attr_has_flag (attr, FWUPD_SECURITY_ATTR_FLAG_SUCCESS))
        {
          pg_row = hsi_create_pg_row ("emblem-ok", "color_green", attr);
          gtk_widget_add_css_class (pg_row, "success-icon");
          gtk_widget_add_css_class (pg_row, "success-title");
        }
      else
        {
          pg_row = hsi_create_pg_row ("process-stop", "color_dim", attr);
          gtk_widget_add_css_class (pg_row, "error-icon");
          gtk_widget_add_css_class (pg_row, "error-title");
        }
      adw_preferences_group_add (ADW_PREFERENCES_GROUP (hsi_pg), GTK_WIDGET (pg_row));
    }
  self->is_created = TRUE;
}

static void
on_hsi_clicked_cb (GtkWidget                *widget,
                   CcFirmwareSecurityDialog *self)
{
  adw_leaflet_navigate (self->leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);

  if (!self->is_created)
    {
      update_hsi_listbox (self, 1);
      update_hsi_listbox (self, 2);
      update_hsi_listbox (self, 3);
      update_hsi_listbox (self, 4);
      self->is_created = TRUE;
    }

  if (widget == self->firmware_security_dialog_min_row)
    {
      adw_window_title_set_title (self->second_page_title, _("Security Level 1"));
      gtk_widget_set_visible (self->firmware_security_dialog_hsi1_pg, TRUE);
    }
  else if (widget == self->firmware_security_dialog_basic_row)
    {
      adw_window_title_set_title (self->second_page_title, _("Security Level 2"));
      gtk_widget_set_visible (self->firmware_security_dialog_hsi2_pg, TRUE);
    }
  else if (widget == self->firmware_security_dialog_extend_row)
    {
      adw_window_title_set_title (self->second_page_title, _("Security Level 3"));
      gtk_widget_set_visible (self->firmware_security_dialog_hsi3_pg, TRUE);
    }
}

static void
on_fw_back_button_clicked_cb (GtkWidget *widget,
                              gpointer   data)
{
  CcFirmwareSecurityDialog *self = CC_FIRMWARE_SECURITY_DIALOG (data);

  adw_leaflet_navigate (self->leaflet, ADW_NAVIGATION_DIRECTION_BACK);

  gtk_widget_set_visible (self->firmware_security_dialog_hsi1_pg, FALSE);
  gtk_widget_set_visible (self->firmware_security_dialog_hsi2_pg, FALSE);
  gtk_widget_set_visible (self->firmware_security_dialog_hsi3_pg, FALSE);
}

static void
cc_firmware_security_dialog_class_init (CcFirmwareSecurityDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/firmware-security/cc-firmware-security-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, dialog_hsi_circle_box);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, dialog_hsi_circle_number);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi1_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi2_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi3_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi1_title);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi2_title);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, hsi3_title);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_title_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_body_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_hsi_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_min_row);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_basic_row);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_extend_row);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_hsi1_pg);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_hsi2_pg);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_hsi3_pg);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, leaflet);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, second_page_title);

  gtk_widget_class_bind_template_callback (widget_class, on_hsi_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_fw_back_button_clicked_cb);
}

static void
cc_firmware_security_dialog_init (CcFirmwareSecurityDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  load_custom_css ("/org/gnome/control-center/firmware-security/security-level.css");
}

GtkWidget *
cc_firmware_security_dialog_new (guint       hsi_number,
                                 GHashTable *hsi1_dict,
                                 GHashTable *hsi2_dict,
                                 GHashTable *hsi3_dict,
                                 GHashTable *hsi4_dict)
{
  CcFirmwareSecurityDialog *dialog;

  dialog = g_object_new (CC_TYPE_FIRMWARE_SECURITY_DIALOG, NULL);
  dialog->hsi_number = hsi_number;
  dialog->is_created = FALSE;
  dialog->hsi1_dict = hsi1_dict;
  dialog->hsi2_dict = hsi2_dict;
  dialog->hsi3_dict = hsi3_dict;
  dialog->hsi4_dict = hsi4_dict;
  update_dialog (dialog);

  return GTK_WIDGET (dialog);
}
