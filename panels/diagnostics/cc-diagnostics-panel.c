/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "list-box-helper.h"
#include "cc-diagnostics-panel.h"
#include "cc-diagnostics-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcDiagnosticsPanel
{
  CcPanel     parent_instance;

  GtkLabel   *diagnostics_explanation_label;
  GtkLabel   *diagnostics_learn_more_label;
  GtkListBox *diagnostics_list_box;
  GtkSwitch   *abrt_switch;

  guint        abrt_watch_id;

  GSettings  *privacy_settings;
};

CC_PANEL_REGISTER (CcDiagnosticsPanel, cc_diagnostics_panel)

static void
cc_diagnostics_panel_finalize (GObject *object)
{
  CcDiagnosticsPanel *self = CC_DIAGNOSTICS_PANEL (object);

  if (self->abrt_watch_id > 0)
    {
      g_bus_unwatch_name (self->abrt_watch_id);
      self->abrt_watch_id = 0;
    }

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_diagnostics_panel_parent_class)->finalize (object);
}

static char *
get_os_name (void)
{
  char *buffer;
  char *name;

  name = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
       char *start, *end;

       start = end = NULL;
       if ((start = strstr (buffer, "NAME=")) != NULL)
         {
           start += strlen ("NAME=");
           end = strchr (start, '\n');
         }

       if (start != NULL && end != NULL)
         {
           name = g_strndup (start, end - start);
         }

       g_free (buffer);
    }

  if (name && *name != '\0')
    {
      char *tmp;
      tmp = g_shell_unquote (name, NULL);
      g_free (name);
      name = tmp;
    }

  if (name == NULL)
    name = g_strdup ("GNOME");

  return name;
}

static char *
get_privacy_policy_url (void)
{
  char *buffer;
  char *url;

  url = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
       char *start, *end;

       start = end = NULL;
       if ((start = strstr (buffer, "PRIVACY_POLICY_URL=")) != NULL)
         {
           start += strlen ("PRIVACY_POLICY_URL=");
           end = strchr (start, '\n');
         }

       if (start != NULL && end != NULL)
         {
           url = g_strndup (start, end - start);
         }

       g_free (buffer);
    }

  if (url && *url != '\0')
    {
      char *tmp;
      tmp = g_shell_unquote (url, NULL);
      g_free (url);
      url = tmp;
    }

  if (url == NULL)
    url = g_strdup ("http://www.gnome.org/privacy-policy");

  return url;
}

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  CcDiagnosticsPanel *self = user_data;
  g_debug ("ABRT appeared");
  gtk_widget_show (GTK_WIDGET (self->diagnostics_list_box));
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  CcDiagnosticsPanel *self = user_data;
  g_debug ("ABRT vanished");
  gtk_widget_hide (GTK_WIDGET (self->diagnostics_list_box));
}

static void
cc_diagnostics_panel_init (CcDiagnosticsPanel *self)
{
  char *os_name, *url, *msg;

  g_resources_register (cc_diagnostics_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->diagnostics_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings, "report-technical-problems",
                   self->abrt_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  os_name = get_os_name ();
  /* translators: '%s' is the distributor's name, such as 'Fedora' */
  msg = g_strdup_printf (_("Sending reports of technical problems helps us improve %s. Reports are sent anonymously and are scrubbed of personal data."),
                         os_name);
  g_free (os_name);
  gtk_label_set_text (self->diagnostics_explanation_label, msg);

  url = get_privacy_policy_url ();
  if (!url)
    {
      g_debug ("Not watching for ABRT appearing, /etc/os-release lacks a privacy policy URL");
      return;
    }
  msg = g_strdup_printf ("%s <a href=\"%s\">Learn more</a>", _("Reports are sent anonymously and are scrubbed of personal data."), url);
  g_free (url);
  gtk_label_set_markup (self->diagnostics_learn_more_label, msg);
  g_free (msg);

  self->abrt_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                                "org.freedesktop.problems.daemon",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                abrt_appeared_cb,
                                                abrt_vanished_cb,
                                                self,
                                                NULL);
}


static void
cc_diagnostics_panel_class_init (CcDiagnosticsPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_diagnostics_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/diagnostics/cc-diagnostics-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, diagnostics_explanation_label);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, diagnostics_learn_more_label);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, diagnostics_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, abrt_switch);
}
