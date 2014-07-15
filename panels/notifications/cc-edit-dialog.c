/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "cc-notifications-panel.h"
#include "cc-edit-dialog.h"

static struct {
  const char *setting_key;
  const char *label;
  gboolean bold;
} policy_settings[] = {
  /* TRANSLATORS: this is the per application switch for message tray usage. */
  { "enable",                  NC_("notifications", "Notifications"),               TRUE  },
  /* TRANSLATORS: this is the setting to configure sounds associated with notifications */
  { "enable-sound-alerts",     NC_("notifications", "Sound Alerts"),                FALSE },
  { "show-banners",            NC_("notifications", "Show Popup Banners"),          FALSE },
  /* TRANSLATORS: banners here refers to message tray notifications in the middle of the screen */
  { "force-expanded",          NC_("notifications", "Show Details in Banners"),     FALSE },
  { "show-in-lock-screen",     NC_("notifications", "View in Lock Screen"),         FALSE },
  { "details-in-lock-screen",  NC_("notifications", "Show Details in Lock Screen"), FALSE }
};

void
cc_build_edit_dialog (CcNotificationsPanel *panel,
                      GAppInfo             *app,
                      GSettings            *settings)
{
  GtkWindow *shell;
  GtkDialog *win;
  GtkWidget *content_area;
  GtkGrid *content_grid;
  int i;

  shell = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel)));

  win = g_object_new (GTK_TYPE_DIALOG,
                      "modal", TRUE,
                      "title", g_app_info_get_name (app),
                      "width-request", 450,
                      "transient-for", shell,
                      "resizable", FALSE,
                      "use-header-bar", TRUE,
                      NULL);

  content_area = gtk_dialog_get_content_area (win);
  content_grid = GTK_GRID (gtk_grid_new ());
  g_object_set (content_grid,
                "row-spacing", 10,
                "margin-top", 12,
                "margin-start", 15,
                "margin-end", 15,
                "margin-bottom", 12,
                NULL);
  gtk_container_add (GTK_CONTAINER (content_area), GTK_WIDGET (content_grid));

  for (i = 0; i < G_N_ELEMENTS (policy_settings); i++)
    {
      GtkWidget *label;
      GtkWidget *_switch;

      label = gtk_label_new (g_dpgettext2 (GETTEXT_PACKAGE,
                                           "notifications",
                                           policy_settings[i].label));
      g_object_set (label,
                    "xalign", 0.0,
                    "hexpand", TRUE,
                    NULL);

      if (policy_settings[i].bold)
        {
          PangoAttrList *list;
          PangoAttribute *weight;
          list = pango_attr_list_new ();
          weight = pango_attr_weight_new (PANGO_WEIGHT_BOLD);

          pango_attr_list_insert (list, weight);
          gtk_label_set_attributes (GTK_LABEL (label), list);

          pango_attr_list_unref (list);
        }

      _switch = gtk_switch_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), _switch);
      g_settings_bind (settings, policy_settings[i].setting_key,
                       _switch, "active",
                       G_SETTINGS_BIND_DEFAULT);

      gtk_grid_attach (content_grid, GTK_WIDGET (label),
                       0, i, 1, 1);
      gtk_grid_attach (content_grid, _switch,
                       1, i, 1, 1);
    }

  g_signal_connect (win, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_widget_show_all (GTK_WIDGET (win));
}
