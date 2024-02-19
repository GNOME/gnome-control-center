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

#include "about/cc-about-page.h"
#include "datetime/cc-datetime-page.h"
#include "region/cc-region-page.h"
#include "remote-desktop/cc-remote-desktop-page.h"
#include "remote-login/cc-remote-login-page.h"
#include "users/cc-users-page.h"

struct _CcSystemPanel
{
  CcPanel    parent_instance;

  AdwNavigationView *navigation;

  GtkWidget *remote_login_dialog;
};

enum
{
  PROP_0,
  PROP_PARAMETERS
};

CC_PANEL_REGISTER (CcSystemPanel, cc_system_panel)

static void
cc_system_panel_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcSystemPanel *self = CC_SYSTEM_PANEL (object);

  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters = g_value_get_variant (value);
          g_autoptr (GVariant) v = NULL;
          const gchar *parameter = NULL;
          AdwNavigationPage *subpage = NULL;

          if (parameters == NULL || g_variant_n_children (parameters) <= 0)
            return;

          g_variant_get_child (parameters, 0, "v", &v);
          if (!g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
            {
              g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                         (gchar *)g_variant_get_type (v));
              return;
            }

          parameter = g_variant_get_string (v, NULL);
          subpage = adw_navigation_view_find_page (self->navigation, parameter);
          if (subpage)
            adw_navigation_view_push (self->navigation, subpage);
          else
            g_warning ("No subpage named '%s' found", parameter);

          return;
        }
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
on_secure_shell_row_clicked (CcSystemPanel *self)
{
  if (self->remote_login_dialog == NULL) {
    GtkWidget *parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

    self->remote_login_dialog = g_object_new (CC_TYPE_REMOTE_LOGIN_PAGE, NULL);

    gtk_window_set_transient_for (GTK_WINDOW (self->remote_login_dialog),
                                  GTK_WINDOW (parent));
  }

  gtk_window_present (GTK_WINDOW (self->remote_login_dialog));
}

static void
cc_system_panel_class_init (CcSystemPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_system_panel_set_property;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/cc-system-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, navigation);

  gtk_widget_class_bind_template_callback (widget_class, on_secure_shell_row_clicked);

  g_type_ensure (CC_TYPE_ABOUT_PAGE);
  g_type_ensure (CC_TYPE_DATE_TIME_PAGE);
  g_type_ensure (CC_TYPE_REGION_PAGE);
  g_type_ensure (CC_TYPE_REMOTE_DESKTOP_PAGE);
  g_type_ensure (CC_TYPE_REMOTE_LOGIN_PAGE);
  g_type_ensure (CC_TYPE_USERS_PAGE);
}

static void
cc_system_panel_init (CcSystemPanel *self)
{
  g_resources_register (cc_system_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (self));
}
