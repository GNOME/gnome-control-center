/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2025 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include "cc-network-activity-page.h"

#include <glib/gi18n.h>

struct _CcNetworkActivityPage
{
  AdwNavigationPage    parent_instance;
};

G_DEFINE_TYPE (CcNetworkActivityPage, cc_network_activity_page, ADW_TYPE_NAVIGATION_PAGE)

static void
cc_network_activity_page_class_init (CcNetworkActivityPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/network-activity/cc-network-activity-page.ui");
}

static void
cc_network_activity_page_init (CcNetworkActivityPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

}
