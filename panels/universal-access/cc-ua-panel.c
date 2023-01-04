/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
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
 * Author(s):
 *   Thomas Wood <thomas.wood@intel.com>
 *   Rodrigo Moya <rodrigo@gnome.org>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <math.h>
#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

#include "cc-list-row.h"
#include "cc-ua-macros.h"
#include "cc-ua-panel.h"
#include "cc-ua-hearing-page.h"
#include "cc-ua-mouse-page.h"
#include "cc-ua-seeing-page.h"
#include "cc-ua-typing-page.h"
#include "cc-ua-zoom-page.h"
#include "cc-ua-resources.h"

struct _CcUaPanel
{
  CcPanel    parent_instance;

  AdwHeaderBar       *titlebar;
  AdwWindowTitle     *page_title;
  GtkButton          *back_button;

  AdwPreferencesPage *main_page;
  AdwLeaflet         *main_leaflet;
  CcListRow          *show_ua_menu_row;
  CcListRow          *seeing_row;
  CcListRow          *hearing_row;
  CcListRow          *typing_row;
  CcListRow          *mouse_row;
  CcListRow          *zoom_row;

  CcUaSeeingPage     *seeing_page;
  CcUaHearingPage    *hearing_page;
  CcUaTypingPage     *typing_page;
  CcUaMousePage      *mouse_page;
  CcUaZoomPage       *zoom_page;

  GSettings *a11y_settings;
};

CC_PANEL_REGISTER (CcUaPanel, cc_ua_panel)

static void
ua_panel_update_back_button_cb (CcUaPanel *self)
{
  GtkWidget *page;
  gboolean folded, is_main_page;

  g_assert (CC_IS_UA_PANEL (self));

  folded = cc_panel_get_folded (CC_PANEL (self));

  page = adw_leaflet_get_visible_child (self->main_leaflet);
  is_main_page = page == (gpointer)self->main_page;

  gtk_widget_set_visible (GTK_WIDGET (self->back_button), folded || !is_main_page);
}

static void
ua_panel_back_clicked_cb (CcUaPanel *self)
{
  GtkWidget *page;
  gboolean is_main_page;

  g_assert (CC_IS_UA_PANEL (self));

  page = adw_leaflet_get_visible_child (self->main_leaflet);
  is_main_page = page == (gpointer)self->main_page;

  if (is_main_page)
    gtk_widget_activate_action (GTK_WIDGET (self), "window.navigate", "i",
                                ADW_NAVIGATION_DIRECTION_BACK);
  else
    adw_leaflet_navigate (self->main_leaflet, ADW_NAVIGATION_DIRECTION_BACK);
}

static void
ua_panel_visible_child_changed_cb (CcUaPanel *self)
{
  GtkWidget *page;
  const char *title = NULL;

  g_assert (CC_IS_UA_PANEL (self));

  page = adw_leaflet_get_visible_child (self->main_leaflet);

  if (page == (gpointer)self->seeing_page)
    title = _("Seeing");
  else if (page == (gpointer)self->hearing_page)
    title = _("Hearing");
  else if (page == (gpointer)self->typing_page)
    title = _("Typing");
  else if (page == (gpointer)self->mouse_page)
    title = _("Pointing & Clicking");
  else if (page == (gpointer)self->zoom_page)
    title = _("Zoom");
  else
    title = _("Accessibility");

  adw_window_title_set_title (self->page_title, title);
}

static void
ua_panel_row_activated_cb (CcUaPanel *self,
                           CcListRow *row)
{
  GtkWidget *child;

  g_assert (CC_IS_UA_PANEL (self));
  g_assert (CC_IS_LIST_ROW (row));

  if (row == self->seeing_row)
    child = GTK_WIDGET (self->seeing_page);
  else if (row == self->hearing_row)
    child = GTK_WIDGET (self->hearing_page);
  else if (row == self->typing_row)
    child = GTK_WIDGET (self->typing_page);
  else if (row == self->mouse_row)
    child = GTK_WIDGET (self->mouse_page);
  else if (row == self->zoom_row)
    child = GTK_WIDGET (self->zoom_page);
  else
    g_assert_not_reached ();

  adw_leaflet_set_visible_child (self->main_leaflet, child);
}

static void
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanel *self = CC_UA_PANEL (object);

  g_clear_object (&self->a11y_settings);

  G_OBJECT_CLASS (cc_ua_panel_parent_class)->dispose (object);
}

static const char *
cc_ua_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/a11y";
}

static void
cc_ua_panel_class_init (CcUaPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_ua_panel_get_help_uri;

  object_class->dispose = cc_ua_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-ua-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, titlebar);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, page_title);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, back_button);

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, main_page);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, main_leaflet);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, show_ua_menu_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, seeing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, hearing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, mouse_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, zoom_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, seeing_page);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, hearing_page);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_page);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, mouse_page);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, zoom_page);

  gtk_widget_class_bind_template_callback (widget_class, ua_panel_update_back_button_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_panel_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_panel_visible_child_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_panel_row_activated_cb);

  g_type_ensure (CC_TYPE_UA_SEEING_PAGE);
  g_type_ensure (CC_TYPE_UA_HEARING_PAGE);
  g_type_ensure (CC_TYPE_UA_TYPING_PAGE);
  g_type_ensure (CC_TYPE_UA_MOUSE_PAGE);
  g_type_ensure (CC_TYPE_UA_ZOOM_PAGE);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  g_resources_register (cc_universal_access_get_resource ());
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                                    "/org/gnome/control-center/universal-access/icons");

  gtk_widget_init_template (GTK_WIDGET (self));
  ua_panel_visible_child_changed_cb (self);

  self->a11y_settings = g_settings_new (A11Y_SETTINGS);
  g_settings_bind (self->a11y_settings, KEY_ALWAYS_SHOW_STATUS,
                   self->show_ua_menu_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
}
