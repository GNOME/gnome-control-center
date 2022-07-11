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
  GtkWidget        *hsi_icon;
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

  GHashTable       *hsi0_dict;
  GHashTable       *hsi1_dict;
  GHashTable       *hsi2_dict;
  GHashTable       *hsi3_dict;
  GHashTable       *hsi4_dict;

  guint             hsi_number;
  SecureBootState   secure_boot_state;
};

CC_PANEL_REGISTER (CcfirmwareSecurityPanel, cc_firmware_security_panel)

static void
set_secure_boot_button_view (CcfirmwareSecurityPanel *self)
{
  guint64 sb_flags = 0;
  guint64 pk_flags = 0;
  guint64 *result;

  /* get HSI-0 flags if set */
  result = g_hash_table_lookup (self->hsi0_dict, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT);
  if (result != NULL)
    sb_flags = GPOINTER_TO_INT (result);
  result = g_hash_table_lookup (self->hsi0_dict, FWUPD_SECURITY_ATTR_ID_UEFI_PK);
  if (result != NULL)
    pk_flags = GPOINTER_TO_INT (result);

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
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot is Inactive"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("No protection when the device is started."));
      gtk_widget_add_css_class (self->secure_boot_icon, "error");
    }
}

static void
parse_event_variant_iter (CcfirmwareSecurityPanel *self,
                          GVariantIter            *iter)
{
  FwupdSecurityAttrResult result = 0;
  FwupdSecurityAttrFlags flags = 0;
  g_autofree gchar *date_string = NULL;
  g_autoptr (GDateTime) date = NULL;
  const gchar *appstream_id = NULL;
  const gchar *key;
  const gchar *event_msg;
  const gchar *description = NULL;
  guint64 timestamp = 0;
  GVariant *value;
  GtkWidget *row;
  GtkWidget *subrow;

  while (g_variant_iter_next (iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "AppstreamId") == 0)
        appstream_id = g_variant_get_string (value, NULL);
      else if (g_strcmp0 (key, "Flags") == 0)
        flags = g_variant_get_uint64(value);
      else if (g_strcmp0 (key, "HsiResult") == 0)
        result = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "Created") == 0)
        timestamp = g_variant_get_uint64 (value);
      else if (g_strcmp0 (key, "Description") == 0)
        description = g_variant_get_string (value, NULL);
      g_variant_unref (value);
    }

  /* unknown to us */
  if (appstream_id == NULL)
    return;

  event_msg = fwupd_event_to_log (appstream_id, result);
  if (event_msg == NULL)
    return;

  /* build new row */
  date = g_date_time_new_from_unix_local (timestamp);
  date_string = g_date_time_format (date, "\%F \%H:\%m:\%S");

  row = adw_expander_row_new ();
  if (flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "emblem-default-symbolic");
      gtk_widget_add_css_class (row, "success-icon");
    }
  else
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "dialog-warning-symbolic");
      gtk_widget_add_css_class (row, "warning-icon");
    }

  if (description)
    {
       subrow = adw_action_row_new ();
       adw_preferences_row_set_title (ADW_PREFERENCES_ROW (subrow), dgettext ("fwupd", description));
       adw_expander_row_add_row (ADW_EXPANDER_ROW (row), subrow);
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (row), false);
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), event_msg);
  adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), date_string);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->firmware_security_log_pgroup), GTK_WIDGET (row));

  adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->firmware_security_log_stack), "page2");
}

static void
parse_variant_iter (CcfirmwareSecurityPanel *self,
                    GVariantIter            *iter)
{
  GVariant *value;
  const gchar *key;
  const gchar *appstream_id = NULL;
  guint64 flags = 0;
  guint32 hsi_level = 0;

  while (g_variant_iter_next (iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "AppstreamId") == 0)
        appstream_id = g_variant_get_string (value, NULL);
      else if (g_strcmp0 (key, "Flags") == 0)
        flags = g_variant_get_uint64 (value);
      else if (g_strcmp0 (key, "HsiLevel") == 0)
        hsi_level = g_variant_get_uint32 (value);
      g_variant_unref (value);
    }

  /* invalid */
  if (appstream_id == NULL)
    return;

  /* insert into correct hash table */
  switch (hsi_level)
    {
      case 0:
        g_hash_table_insert (self->hsi0_dict,
                             g_strdup (appstream_id),
                             GINT_TO_POINTER (flags));
        break;
      case 1:
        g_hash_table_insert (self->hsi1_dict,
                             g_strdup (appstream_id),
                             GINT_TO_POINTER (flags));
        break;
      case 2:
        g_hash_table_insert (self->hsi2_dict,
                             g_strdup (appstream_id),
                             GINT_TO_POINTER (flags));
        break;
      case 3:
        g_hash_table_insert (self->hsi3_dict,
                             g_strdup (appstream_id),
                             GINT_TO_POINTER (flags));
        break;
      case 4:
        g_hash_table_insert (self->hsi4_dict,
                             g_strdup (appstream_id),
                             GINT_TO_POINTER (flags));
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
      g_warning ("failed to get Security Attribute: %s", error->message);
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
                             const gchar             *icon_name,
                             const gchar             *style,
                             gchar                   *title,
                             const gchar             *description)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi_icon), icon_name);
  gtk_widget_add_css_class (self->hsi_icon, style);
  gtk_label_set_text (GTK_LABEL (self->hsi_label), title);
  gtk_label_set_text (GTK_LABEL (self->hsi_description), description);
}

static void
set_hsi_button_view (CcfirmwareSecurityPanel *self)
{
  switch (self->hsi_number)
    {
      case 0:
        set_hsi_button_view_contain (self, "dialog-warning-symbolic",
                                     "error",
                                     /* TRANSLATORS: in reference to firmware protection: 0/4 stars */
                                     _("No Protection"),
                                     _("Highly exposed to security threats."));
        break;
      case 1:
        set_hsi_button_view_contain (self, "security-low-symbolic",
                                     "neutral",
                                     /* TRANSLATORS: in reference to firmware protection: 1/4 stars */
                                     _("Minimal Protection"),
                                     _("Limited protection against simple security threats."));
        break;
      case 2:
        set_hsi_button_view_contain (self, "security-medium-symbolic",
                                     "warning",
                                     /* TRANSLATORS: in reference to firmware protection: 2/4 stars */
                                     _("Basic Protection"),
                                     _("Protected against common security threats."));
        break;
      case 3:
        set_hsi_button_view_contain (self, "security-high-symbolic",
                                     "good",
                                     /* TRANSLATORS: in reference to firmware protection: 3/4 stars */
                                     _("Extended Protection"),
                                     _("Protected against a wide range of security threats."));
        break;
      case 4:
        set_hsi_button_view_contain (self, "security-high-symbolic",
                                     "good",
                                     /* TRANSLATORS: in reference to firmware protection: 4/4 stars */
                                     _("Comprehensive Protection"),
                                     _("Protected against a wide range of security threats."));
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
  if (hsi_str != NULL && g_str_has_prefix (hsi_str, "HSI:"))
    self->hsi_number = g_ascii_strtoll (hsi_str + 4, NULL, 10);
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
  gtk_widget_class_bind_template_child (widget_class, CcfirmwareSecurityPanel, hsi_icon);
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

  self->hsi0_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->hsi1_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->hsi2_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->hsi3_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->hsi4_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

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
