/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
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
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>

#include "cc-wellbeing-panel.h"
#include "cc-wellbeing-resources.h"

struct _CcWellbeingPanel {
  CcPanel parent_instance;
};

struct _CcWellbeingPanelClass {
  CcPanelClass parent;
};

CC_PANEL_REGISTER (CcWellbeingPanel, cc_wellbeing_panel);

static gboolean
keynav_failed_cb (CcWellbeingPanel *self,
                  GtkDirectionType  direction)
{
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

  if (!toplevel)
    return FALSE;

  if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    return FALSE;

  return gtk_widget_child_focus (toplevel, direction == GTK_DIR_UP ?
                                 GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

static void
cc_wellbeing_panel_init (CcWellbeingPanel *self)
{
  g_resources_register (cc_wellbeing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_wellbeing_panel_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_WELLBEING_PANEL);

  G_OBJECT_CLASS (cc_wellbeing_panel_parent_class)->dispose (object);
}

static const char *
cc_wellbeing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/shell-wellbeing";
}

static void
cc_wellbeing_panel_class_init (CcWellbeingPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class  = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_wellbeing_panel_dispose;

  panel_class->get_help_uri = cc_wellbeing_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-wellbeing-panel.ui");

  gtk_widget_class_bind_template_callback (widget_class, keynav_failed_cb);
}

