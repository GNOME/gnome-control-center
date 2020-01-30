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
#include "cc-os-release.h"
#include "cc-util.h"
#include "shell/cc-application.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcDiagnosticsPanel
{
  CcPanel     parent_instance;

  GtkLabel   *diagnostics_explanation_label;
  GtkListBox *diagnostics_list_box;
  GtkSwitch   *abrt_switch;

  GSettings  *privacy_settings;
};

CC_PANEL_REGISTER (CcDiagnosticsPanel, cc_diagnostics_panel)

/* Static init function */

static void
set_panel_visibility (CcPanelVisibility visibility)
{
  CcApplication *application;

  application = CC_APPLICATION (g_application_get_default ());
  cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                       "diagnostics",
                                       visibility);

}

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  g_debug ("ABRT appeared");
  set_panel_visibility (CC_PANEL_VISIBLE);
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_debug ("ABRT vanished");
  set_panel_visibility (CC_PANEL_VISIBLE_IN_SEARCH);
}

void
cc_diagnostics_panel_static_init_func (void)
{
  g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                    "org.freedesktop.problems.daemon",
                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                    abrt_appeared_cb,
                    abrt_vanished_cb,
                    NULL,
                    NULL);

  set_panel_visibility (CC_PANEL_VISIBLE_IN_SEARCH);
}

static void
cc_diagnostics_panel_finalize (GObject *object)
{
  CcDiagnosticsPanel *self = CC_DIAGNOSTICS_PANEL (object);

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_diagnostics_panel_parent_class)->finalize (object);
}

static void
cc_diagnostics_panel_class_init (CcDiagnosticsPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_diagnostics_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/diagnostics/cc-diagnostics-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, diagnostics_explanation_label);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, diagnostics_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcDiagnosticsPanel, abrt_switch);
}

static void
cc_diagnostics_panel_init (CcDiagnosticsPanel *self)
{
  g_autofree gchar *os_name = NULL;
  g_autofree gchar *url = NULL;
  g_autofree gchar *msg = NULL;
  g_autofree gchar *link = NULL;

  g_resources_register (cc_diagnostics_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->diagnostics_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings, "report-technical-problems",
                   self->abrt_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  os_name = cc_os_release_get_value ("NAME");
  url = cc_os_release_get_value ("PRIVACY_POLICY_URL");
  if (!url)
    url = g_strdup ("http://www.gnome.org/privacy-policy");
  /* translators: Text used in link to privacy policy */
  link = g_strdup_printf ("<a href=\"%s\">%s</a>", url, _("Learn more"));
  /* translators: The first '%s' is the distributor's name, such as 'Fedora', the second '%s' is a link to the privacy policy */
  msg = g_strdup_printf (_("Sending reports of technical problems helps us improve %s. Reports "
                           "are sent anonymously and are scrubbed of personal data. %s"),
                         os_name, link);
  gtk_label_set_markup (self->diagnostics_explanation_label, msg);
}
