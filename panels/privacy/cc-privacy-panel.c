/*
 * Copyright 2023 Marco Melorio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "cc-privacy-panel.h"

#include "cc-privacy-resources.h"
#include "cc-screen-page.h"

struct _CcPrivacyPanel
{
  CcPanel          parent_instance;
};

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-privacy-panel.ui");

  g_type_ensure (CC_TYPE_SCREEN_PAGE);
}

static void
cc_privacy_panel_init (CcPrivacyPanel *self)
{
  g_resources_register (cc_privacy_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}
