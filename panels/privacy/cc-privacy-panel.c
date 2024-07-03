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


#include <config.h>

#include "cc-privacy-panel.h"

#ifdef BUILD_THUNDERBOLT
#include "bolt/cc-bolt-page.h"
#endif
#include "camera/cc-camera-page.h"
#include "diagnostics/cc-diagnostics-page.h"
#include "firmware-security/cc-firmware-security-page.h"
#include "cc-list-row.h"
#include "location/cc-location-page.h"
#include "cc-privacy-resources.h"
#include "screen/cc-screen-page.h"
#include "usage/cc-usage-page.h"

struct _CcPrivacyPanel
{
  CcPanel            parent_instance;

  CcListRow         *bolt_row;
  CcListRow         *location_row;
};

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

static const char *
cc_privacy_panel_get_help_uri (CcPanel *panel)
{
  AdwNavigationPage *page = cc_panel_get_visible_subpage (panel);
  const char *page_tag = adw_navigation_page_get_tag (page);

  if (g_strcmp0 (page_tag, "location") == 0)
    return "help:gnome-help/privacy-location";
  else if (g_strcmp0 (page_tag, "screenlock") == 0)
    return "help:gnome-help/privacy-screen-lock";
  else
    return "help:gnome-help/privacy";
}

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  panel_class->get_help_uri = cc_privacy_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-privacy-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, bolt_row);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_row);

  g_type_ensure (CC_TYPE_CAMERA_PAGE);
  g_type_ensure (CC_TYPE_DIAGNOSTICS_PAGE);
  g_type_ensure (CC_TYPE_FIRMWARE_SECURITY_PAGE);
  g_type_ensure (CC_TYPE_LOCATION_PAGE);
  g_type_ensure (CC_TYPE_SCREEN_PAGE);
  g_type_ensure (CC_TYPE_USAGE_PAGE);
}

static void
cc_privacy_panel_init (CcPrivacyPanel *self)
{
  g_resources_register (cc_privacy_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef BUILD_THUNDERBOLT
  CcBoltPage* bolt_page = cc_bolt_page_new ();

  cc_panel_add_subpage (CC_PANEL (self), "thunderbolt", ADW_NAVIGATION_PAGE (bolt_page));

  g_object_bind_property (bolt_page, "visible",
                          self->bolt_row, "visible", G_BINDING_SYNC_CREATE);
#endif

#ifdef HAVE_LOCATION_SERVICES
  CcLocationPage *location_page = g_object_new (CC_TYPE_LOCATION_PAGE, NULL);

  cc_panel_add_subpage (CC_PANEL (self), "thunderbolt", ADW_NAVIGATION_PAGE (location_page));

  g_object_bind_property (location_page, "visible",
                          self->location_row, "visible", G_BINDING_SYNC_CREATE);
#endif
}
