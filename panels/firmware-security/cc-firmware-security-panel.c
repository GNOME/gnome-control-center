/* cc-firmware-security-panel.c
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

#include "shell/cc-application.h"

#include "cc-firmware-security-panel.h"
#include "cc-firmware-security-resources.h"
#include "cc-firmware-security-dialog.h"
#include "cc-firmware-security-boot-dialog.h"
#include "cc-firmware-security-utils.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcfirmwareSecurityPanel
{
  CcPanel           parent_instance;

  GtkButton        *hsi_button;
  GtkButton        *secure_boot_button;

  /* HSI button */
  GtkWidget        *hsi_grid;

  GtkWidget        *hsi_circle_box;
  GtkWidget        *hsi_circle_number;

  GtkWidget        *hsi_label;
  GtkWidget        *hsi_description;

  /* secure boot button */
  GtkWidget        *secure_boot_button_grid;
  GtkWidget        *secure_boot_icon;
  GtkWidget        *secure_boot_label;
  GtkWidget        *secure_boot_description;

  /* event listbox */
  GtkWidget        *firmware_security_log_listbox;
  GtkWidget        *firmware_security_log_stack;
  GtkWidget        *firmware_security_log_pgroup;

  GDBusProxy       *bus_proxy;
  GDBusProxy       *properties_bus_proxy;

  GHashTable       *hsi1_dict;
  GHashTable       *hsi2_dict;
  GHashTable       *hsi3_dict;
  GHashTable       *hsi4_dict;

  guint             hsi_number;
  SecureBootState   secure_boot_state;
};

CC_PANEL_REGISTER (CcfirmwareSecurityPanel, cc_firmware_security_panel)

static void
set_hsi_button_view (CcfirmwareSecurityPanel *self);

static void
set_secure_boot_button_view (CcfirmwareSecurityPanel *self)
{
  FwupdSecurityAttr *attr;
  guint64 sb_flags = 0;
  guint64 pk_flags = 0;

  /* get HSI-1 flags if set */
  attr = g_hash_table_lookup (self->hsi1_dict, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT);
  if (attr != NULL)
    sb_flags = attr->flags;
  attr = g_hash_table_lookup (self->hsi1_dict, FWUPD_SECURITY_ATTR_ID_UEFI_PK);
  if (attr != NULL)
    pk_flags = attr->flags;

  /* enabled and valid */
  if ((sb_flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) > 0 &&
      (pk_flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) > 0)
    {
      self->secure_boot_state = SECURE_BOOT_STATE_ACTIVE;
    }
  else if ((sb_flags & FWUPD_SECURITY_ATTR_RESULT_ENABLED) > 0)
    {
      self->secure_boot_state = SECURE_BOOT_STATE_PROBLEMS;
    }
  else
    {
      self->secure_boot_state = SECURE_BOOT_STATE_INACTIVE;
    }

  /* update UI */
  if (self->secure_boot_state == SECURE_BOOT_STATE_ACTIVE)
   {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot is Active"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("Protected against malicious software when the device starts."));
      gtk_image_set_from_icon_name (GTK_IMAGE (self->secure_boot_icon), "channel-secure-symbolic");
      gtk_widget_add_css_class (self->secure_boot_icon, "good");
    }
  else if (self->secure_boot_state == SECURE_BOOT_STATE_PROBLEMS)
   {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot has Problems"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("Some protection when the device is started."));
      gtk_widget_add_css_class (self->secure_boot_icon, "error");
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot is Off"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("No protection when the device is started."));
      gtk_widget_add_css_class (self->secure_boot_icon, "error");
    }
}

static gchar *
fu_security_attr_get_description_for_eventlog (FwupdSecurityAttr *attr)
{
  GString *str = g_string_new (attr->description);

  /* nothing to do */
  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    return g_string_free (str, FALSE);

  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM &&
      attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by a change in UEFI firmware "
                                "settings, an operating system configuration change, or because of "
                                "malicious software on this system."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by a change in the UEFI firmware "
                                "settings, or because of malicious software on this system."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_OS)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by an operating system configuration "
                                "change, or because of malicious software on this system."));
    }

  return g_string_free (str, FALSE);
}

static void
parse_event_variant_iter (CcfirmwareSecurityPanel *self,
                          GVariantIter            *iter)
{
  g_autofree gchar *date_string = NULL;
  g_autoptr (GDateTime) date = NULL;
  g_autoptr (FwupdSecurityAttr) attr = fu_security_attr_new_from_variant(iter);
  GtkWidget *row;

  /* unknown to us */
  if (attr->appstream_id == NULL || attr->title == NULL)
    return;

  /* skip events that have either been added or removed with no prior value */
  if (attr->result == FWUPD_SECURITY_ATTR_RESULT_UNKNOWN ||
      attr->result_fallback == FWUPD_SECURITY_ATTR_RESULT_UNKNOWN)
    return;

  /* build new row */
  date = g_date_time_new_from_unix_local (attr->timestamp);
  date_string = g_date_time_format (date, "\%F \%H:\%m:\%S");

  row = adw_expander_row_new ();
  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "emblem-ok");
      gtk_widget_add_css_class (row, "success-icon");
    }
  else
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "process-stop");
      gtk_widget_add_css_class (row, "error-icon");
    }

  if (attr->description != NULL)
    {
      GtkWidget *subrow = adw_action_row_new ();
      g_autofree gchar *str = fu_security_attr_get_description_for_eventlog (attr);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (subrow), str);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (row), subrow);
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (row), FALSE);
      gtk_widget_add_css_class (row, "hide-arrow");
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), attr->title);
  adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), date_string);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->firmware_security_log_pgroup), GTK_WIDGET (row));

  adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->firmware_security_log_stack), "page2");
}

static void
parse_variant_iter (CcfirmwareSecurityPanel *self,
                    GVariantIter            *iter)
{
  g_autoptr (FwupdSecurityAttr) attr = fu_security_attr_new_from_variant(iter);
  const gchar *appstream_id = attr->appstream_id;

  /* invalid */
  if (appstream_id == NULL)
    return;

  /* in fwupd <= 1.8.3 org.fwupd.hsi.Uefi.SecureBoot was incorrectly marked as HSI-0,
   * so lower the HSI number forcefully if this attribute failed -- the correct thing
   * to do of course is to update fwupd to a newer build */
  if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT) == 0 &&
      (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) == 0)
    {
      self->hsi_number = 0;
      set_hsi_button_view (self);
    }

  /* insert into correct hash table */
  switch (attr->hsi_level)
    {
      case 1:
        g_hash_table_insert (self->hsi1_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 2:
        g_hash_table_insert (self->hsi2_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 3:
        g_hash_table_insert (self->hsi3_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 4:
        g_hash_table_insert (self->hsi4_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
    }
}

static void
parse_data_from_variant (CcfirmwareSecurityPanel *self,
                         GVariant                *value,
                         const gboolean           is_event)
{
  const gchar *type_string;
  g_autoptr (GVariantIter) iter = NULL;

  type_string = g_variant_get_type_string (value);
  if (g_strcmp0 (type_string, "(a{sv})") == 0)
    {
      g_variant_get (value, "(a{sv})", &iter);
      if (is_event)
        parse_event_variant_iter (self, iter);
      else
        parse_variant_iter (self, iter);
    }
  else if (g_strcmp0 (type_string, "a{sv}") == 0)
    {
      g_variant_get (value, "a{sv}", &iter);
      if (is_event)
        parse_event_variant_iter (self, iter);
      else
        parse_variant_iter (self, iter);
    }
  else
    {
      g_warning ("type %s not known", type_string);
    }
}

static void
parse_array_from_variant (CcfirmwareSecurityPanel *self,
                          GVariant                *value,
                          const gboolean           is_event)
{
  gsize sz;
  g_autoptr (GVariant) untuple = NULL;

  untuple = g_variant_get_child_value (value, 0);
  sz = g_variant_n_children (untuple);
  for (guint i = 0; i < sz; i++)
    {
      g_autoptr (GVariant) data = NULL;
      data = g_variant_get_child_value (untuple, i);
      if (is_event)
        parse_data_from_variant (self, data, TRUE);
      else
        parse_data_from_variant (self, data, FALSE);
    }
}

static void
on_bus_event_done_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (user_data);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      g_warning ("failed to get Security Attribute Event: %s", error->message);
      return;
    }

  parse_array_from_variant (self, val, TRUE);
}

static void
on_bus_done (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      CcApplication *application = CC_APPLICATION (g_application_get_default ());
      g_warning ("failed to get Security Attribute: %s", error->message);
      cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                           "firmware-security",
                                           CC_PANEL_HIDDEN);
      set_secure_boot_button_view (self);
      return;
    }

  parse_array_from_variant (self, val, FALSE);
  set_secure_boot_button_view (self);
}

static void
on_bus_ready_cb (GObject       *source_object,
                 GAsyncResult  *res,
                 gpointer       user_data)
{
  g_autoptr (GError) error = NULL;
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (user_data);

  self->bus_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (self->bus_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("failed to connect fwupd: %s", error->message);

      return;
    }

  g_dbus_proxy_call (self->bus_proxy,
                     "GetHostSecurityAttrs",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_bus_done,
                     self);
  g_dbus_proxy_call (self->bus_proxy,
                     "GetHostSecurityEvents",
                     g_variant_new ("(u)",
                                    100),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_bus_event_done_cb,
                     self);
}

static void
on_hsi_button_clicked_cb (GtkWidget *widget,
                          gpointer   data)
{
  GtkWidget *toplevel;
  CcShell *shell;
  GtkWidget *dialog;
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (data);

  dialog = cc_firmware_security_dialog_new (self->hsi_number,
                                            self->hsi1_dict,
                                            self->hsi2_dict,
                                            self->hsi3_dict,
                                            self->hsi4_dict);
  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  gtk_widget_show (GTK_WIDGET (dialog));
}

static void
on_secure_boot_button_clicked_cb (GtkWidget *widget,
                                  gpointer   data)
{
  GtkWidget *toplevel;
  CcShell *shell;
  GtkWidget *boot_dialog;
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (data);

  boot_dialog = cc_firmware_security_boot_dialog_new (self->secure_boot_state);
  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  gtk_window_set_transient_for (GTK_WINDOW (boot_dialog), GTK_WINDOW (toplevel));
  gtk_widget_show (boot_dialog);
}

static void
set_hsi_button_view_contain (CcfirmwareSecurityPanel *self,
                             guint                    hsi_number,
                             gchar                   *title,
                             const gchar             *description)
{
  switch (hsi_number)
    {
      case 0:
        gtk_label_set_label (GTK_LABEL (self->hsi_circle_number), "0");
        gtk_widget_add_css_class (self->hsi_circle_box, "level0");
        gtk_widget_add_css_class (self->hsi_circle_number, "hsi0");
        break;
      case 1:
        gtk_label_set_label (GTK_LABEL (self->hsi_circle_number), "1");
        gtk_widget_add_css_class (self->hsi_circle_box, "level1");
        gtk_widget_add_css_class (self->hsi_circle_number, "hsi1");
        break;
      case 2:
        gtk_label_set_label (GTK_LABEL (self->hsi_circle_number), "2");
        gtk_widget_add_css_class (self->hsi_circle_box, "level2");
        gtk_widget_add_css_class (self->hsi_circle_number, "hsi2");
        break;
      case 3:
      case 4:
        gtk_label_set_label (GTK_LABEL (self->hsi_circle_number), "3");
        gtk_widget_add_css_class (self->hsi_circle_box, "level3");
        gtk_widget_add_css_class (self->hsi_circle_number, "hsi3");
        break;
      default:
        gtk_label_set_label (GTK_LABEL (self->hsi_circle_number), "?");
        gtk_widget_add_css_class (self->hsi_circle_box, "level1");
        gtk_widget_add_css_class (self->hsi_circle_number, "hsi1");
        break;
    }

  gtk_label_set_text (GTK_LABEL (self->hsi_label), title);
  gtk_label_set_text (GTK_LABEL (self->hsi_description), description);
}

static void
set_hsi_button_view (CcfirmwareSecurityPanel *self)
{
  switch (self->hsi_number)
    {
      case 0:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 0/4 stars */
                                     _("Security Level 0"),
                                     _("Exposed to serious security threats."));
        break;
      case 1:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 1/4 stars */
                                     _("Security Level 1"),
                                     _("Limited protection against simple security threats."));
        break;
      case 2:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 2/4 stars */
                                     _("Security Level 2"),
                                     _("Protected against common security threats."));
        break;
      case 3:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 3/4 stars */
                                     _("Security Level 3"),
                                     _("Protected against a wide range of security threats."));
        break;
      case 4:
        set_hsi_button_view_contain (self,
                                     /* Based on current HSI definition, the max HSI value would be 3. */
                                     3,
                                     /* TRANSLATORS: in reference to firmware protection: 4/4 stars */
                                     _("Comprehensive Protection"),
                                     _("Protected against a wide range of security threats."));
        break;
      case G_MAXUINT:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: ??? stars */
                                     _("Security Level"),
                                     _("Security levels are not available for this device."));
        break;
      default:
        g_warning ("incorrect HSI number %u", self->hsi_number);
    }
}

static void
on_properties_bus_done_cb (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;
  const gchar *hsi_str = NULL;
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (user_data);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      g_warning ("failed to get HSI number");
      return;
    }

  /* parse value */
  hsi_str = g_variant_get_data (val);
  if (hsi_str != NULL && g_str_has_prefix (hsi_str, "HSI:INVALID"))
    {
      self->hsi_number = G_MAXUINT;
    }
  else if (hsi_str != NULL && g_str_has_prefix (hsi_str, "HSI:"))
    {
      self->hsi_number = g_ascii_strtoll (hsi_str + 4, NULL, 10);
    }
  set_hsi_button_view (self);
}

static void
on_properties_bus_ready_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (user_data);
  g_autoptr (GError) error = NULL;

  self->properties_bus_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (self->properties_bus_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect fwupd: %s", error->message);

      return;
    }

  g_dbus_proxy_call (self->properties_bus_proxy,
                     "Get",
                     g_variant_new ("(ss)",
                                    "org.freedesktop.fwupd",
                                    "HostSecurityId"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_properties_bus_done_cb,
                     self);
}

static void
update_panel_visibility (const gchar *chassis_type)
{
  CcApplication *application;
  gboolean visible = TRUE;

  /* there's no point showing this */
  if (g_strcmp0 (chassis_type, "vm") == 0 || g_strcmp0 (chassis_type, "") == 0)
    visible = FALSE;
  application = CC_APPLICATION (g_application_get_default ());
  cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                       "firmware-security",
                                       visible ? CC_PANEL_VISIBLE : CC_PANEL_HIDDEN);
  g_debug ("Firmware Security panel visible: %s as chassis was %s",
           visible ? "yes" : "no",
           chassis_type);
}

void
cc_firmware_security_panel_static_init_func (void)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GVariant) variant = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_warning ("system bus not available: %s", error->message);
      return;
    }
  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.hostname1",
                                         "/org/freedesktop/hostname1",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new ("(ss)",
                                                        "org.freedesktop.hostname1",
                                                        "Chassis"),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
  if (!variant)
    {
      g_warning ("Cannot get org.freedesktop.hostname1.Chassis: %s", error->message);
      return;
    }
  g_variant_get (variant, "(v)", &inner);
  update_panel_visibility (g_variant_get_string (inner, NULL));
}

static void
cc_firmware_security_panel_finalize (GObject *object)
{
  CcfirmwareSecurityPanel *self = CC_FIRMWARE_SECURITY_PANEL (object);

  g_clear_pointer (&self->hsi1_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi2_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi3_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi4_dict, g_hash_table_unref);

  g_clear_object (&self->bus_proxy);
  g_clear_object (&self->properties_bus_proxy);

  G_OBJECT_CLASS (cc_firmware_security_panel_parent_class)->finalize (object);
}


static void
cc_firmware_security_panel_class_init (CcfirmwareSecurityPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_firmware_security_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/firmware-security/cc-firmware-security-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, firmware_security_log_pgroup);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, firmware_security_log_stack);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_button);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_description);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_circle_box);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_circle_number);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_label);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, secure_boot_button);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, secure_boot_description);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, secure_boot_icon);
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, secure_boot_label);

  gtk_widget_class_bind_template_callback (widget_class, on_hsi_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_secure_boot_button_clicked_cb);
}

static void
cc_firmware_security_panel_init (CcfirmwareSecurityPanel *self)
{
  g_resources_register (cc_firmware_security_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->hsi1_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi2_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi3_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi4_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);

  load_custom_css ("/org/gnome/control-center/firmware-security/security-level.css");

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.fwupd",
                            "/",
                            "org.freedesktop.DBus.Properties",
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            on_properties_bus_ready_cb,
                            self);
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.fwupd",
                            "/",
                            "org.freedesktop.fwupd",
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            on_bus_ready_cb,
                            self);
}
