/*
 * cc-system-panel.c
 *
 * Copyright 2023 Gotam Gorabh <gautamy672@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <config.h>

#include <glib/gi18n-lib.h>

#include "cc-list-row.h"
#include "cc-system-panel.h"
#include "cc-system-resources.h"
#include "cc-systemd-service.h"

#include "about/cc-about-page.h"
#include "datetime/cc-datetime-page.h"
#include "region/cc-region-page.h"
#include "remote-desktop/cc-remote-desktop-page.h"
#include "secure-shell/cc-secure-shell-page.h"
#include "users/cc-users-page.h"

struct _CcSystemPanel
{
  CcPanel    parent_instance;

  AdwActionRow *about_row;
  AdwActionRow *datetime_row;
  AdwActionRow *region_row;
  AdwActionRow *remote_desktop_row;
  AdwActionRow *users_row;

  CcSecureShellPage *secure_shell_dialog;
  AdwNavigationPage *software_updates_group;
};

CC_PANEL_REGISTER (CcSystemPanel, cc_system_panel)

static gboolean
gnome_software_allows_updates (void)
{
  const gchar *schema_id  = "org.gnome.software";
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GSettings) settings = NULL;

  source = g_settings_schema_source_get_default ();

  if (source == NULL)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, schema_id, FALSE);

  if (schema == NULL)
    return FALSE;

  settings = g_settings_new (schema_id);
  return g_settings_get_boolean (settings, "allow-updates");
}

static gboolean
gnome_software_exists (void)
{
  g_autofree gchar *path = g_find_program_in_path ("gnome-software");
  return path != NULL;
}

static gboolean
gpk_update_viewer_exists (void)
{
  g_autofree gchar *path = g_find_program_in_path ("gpk-update-viewer");
  return path != NULL;
}

static gboolean
show_software_updates_group (CcSystemPanel *self)
{
  return (gnome_software_exists () && gnome_software_allows_updates ()) ||
         gpk_update_viewer_exists ();
}

static void
cc_system_page_open_software_update (CcSystemPanel *self)
{
  g_autoptr(GError) error = NULL;
  gboolean ret;
  char *argv[3];

  if (gnome_software_exists ())
    {
      argv[0] = "gnome-software";
      argv[1] = "--mode=updates";
      argv[2] = NULL;
    }
  else
    {
      argv[0] = "gpk-update-viewer";
      argv[1] = NULL;
    }

  ret = g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
  if (!ret)
    g_warning ("Failed to spawn %s: %s", argv[0], error->message);
}

static void
on_secure_shell_row_clicked (CcSystemPanel *self)
{
  if (self->secure_shell_dialog == NULL)
    {
      self->secure_shell_dialog = g_object_new (CC_TYPE_SECURE_SHELL_PAGE, NULL);
      g_object_add_weak_pointer (G_OBJECT (self->secure_shell_dialog),
                                 (gpointer *) &self->secure_shell_dialog);
    }

  adw_dialog_present (ADW_DIALOG (self->secure_shell_dialog), GTK_WIDGET (self));
}

static void
cc_system_panel_class_init (CcSystemPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/cc-system-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, about_row);
  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, datetime_row);
  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, region_row);
  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, remote_desktop_row);
  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, users_row);
  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, software_updates_group);

  gtk_widget_class_bind_template_callback (widget_class, cc_system_page_open_software_update);
  gtk_widget_class_bind_template_callback (widget_class, on_secure_shell_row_clicked);

  g_type_ensure (CC_TYPE_ABOUT_PAGE);
  g_type_ensure (CC_TYPE_DATE_TIME_PAGE);
  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_REGION_PAGE);
  g_type_ensure (CC_TYPE_REMOTE_DESKTOP_PAGE);
  g_type_ensure (CC_TYPE_SECURE_SHELL_PAGE);
  g_type_ensure (CC_TYPE_USERS_PAGE);
}

static void
cc_system_panel_init (CcSystemPanel *self)
{
  CcServiceState service_state;

  g_resources_register (cc_system_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (self));

  service_state = cc_get_service_state (REMOTE_DESKTOP_SERVICE, G_BUS_TYPE_SYSTEM);
  /* Hide the remote-desktop page if the g-r-d service is either "masked", "static", or "not-found". */
  gtk_widget_set_visible (GTK_WIDGET (self->remote_desktop_row), service_state == CC_SERVICE_STATE_ENABLED ||
                                                                 service_state == CC_SERVICE_STATE_DISABLED);
  gtk_widget_set_visible (GTK_WIDGET (self->software_updates_group), show_software_updates_group (self));

  cc_panel_add_static_subpage (CC_PANEL (self), "about", CC_TYPE_ABOUT_PAGE);
  cc_panel_add_static_subpage (CC_PANEL (self), "datetime", CC_TYPE_DATE_TIME_PAGE);
  cc_panel_add_static_subpage (CC_PANEL (self), "region", CC_TYPE_REGION_PAGE);
  cc_panel_add_static_subpage (CC_PANEL (self), "remote-desktop", CC_TYPE_REMOTE_DESKTOP_PAGE);
  cc_panel_add_static_subpage (CC_PANEL (self), "users", CC_TYPE_USERS_PAGE);
}
