/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2024 Adrien Plazas <aplazas@gnome.org>
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-ui-util.h"

gboolean
cc_util_keynav_propagate_vertical (GtkWidget        *self,
                                   GtkDirectionType  direction)
{
  GtkWidget *root = GTK_WIDGET (gtk_widget_get_root (self));

  if (root == NULL)
    return FALSE;

  switch (direction) {
  case GTK_DIR_UP:
    return gtk_widget_child_focus (root, GTK_DIR_TAB_BACKWARD);
  case GTK_DIR_DOWN:
    return gtk_widget_child_focus (root, GTK_DIR_TAB_FORWARD);
  default:
    return FALSE;
  }
}

gboolean
cc_util_keynav_propagate_up (GtkWidget        *self,
                             GtkDirectionType  direction)
{
  GtkWidget *root = GTK_WIDGET (gtk_widget_get_root (self));

  if (root == NULL)
    return FALSE;

  switch (direction) {
  case GTK_DIR_UP:
    return gtk_widget_child_focus (root, GTK_DIR_TAB_BACKWARD);
  default:
    return FALSE;
  }
}
