/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2023 Cyber Phantom <inam123451@gmail.com>
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
 */

#include "cc-internet-resources.h"

#include "cc-internet-panel.h"

struct _CcInternetPanel
{
  CcPanel           parent;
};

CC_PANEL_REGISTER (CcInternetPanel, cc_internet_panel)

static void
cc_internet_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_internet_panel_parent_class)->dispose (object);
}

static void
cc_internet_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_internet_panel_parent_class)->finalize (object);
}

static const char *
cc_internet_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/internet";
}

static void
cc_internet_panel_class_init (CcInternetPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_internet_panel_get_help_uri;

  object_class->dispose = cc_internet_panel_dispose;
  object_class->finalize = cc_internet_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/internet/cc-internet-panel.ui");
}

static void
cc_internet_panel_init (CcInternetPanel *self)
{
  g_resources_register (cc_internet_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}
