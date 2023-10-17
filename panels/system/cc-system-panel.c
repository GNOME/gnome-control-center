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
#include "users/cc-users-page.h"

struct _CcSystemPanel
{
  CcPanel    parent_instance;

  AdwNavigationView *navigation;
};

CC_PANEL_REGISTER (CcSystemPanel, cc_system_panel)

static void
cc_system_panel_class_init (CcSystemPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/cc-system-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSystemPanel, navigation);

  g_type_ensure (CC_TYPE_ABOUT_PAGE);
  g_type_ensure (CC_TYPE_DATE_TIME_PAGE);
  g_type_ensure (CC_TYPE_REGION_PAGE);
  g_type_ensure (CC_TYPE_REMOTE_DESKTOP_PAGE);
  g_type_ensure (CC_TYPE_USERS_PAGE);
}

static void
cc_system_panel_init (CcSystemPanel *self)
{
  g_resources_register (cc_system_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (self));
}
