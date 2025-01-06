/* cc-content-row.h
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
 *           2025 Matthijs Velsink <mvelsink@gnome.org>
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_CONTENT_ROW (cc_content_row_get_type())
G_DECLARE_DERIVABLE_TYPE (CcContentRow, cc_content_row, CC, CONTENT_ROW, AdwPreferencesRow)

/**
 * CcContentRowClass
 * @parent_class: The parent class
 * @activate: Activates the row to trigger its main action.
 */
struct _CcContentRowClass
{
  AdwPreferencesRowClass parent_class;

  void (*activate) (CcContentRow *self);
};

GtkWidget *cc_content_row_new (void);

void cc_content_row_add_prefix (CcContentRow *self,
                                GtkWidget    *widget);
void cc_content_row_add_suffix (CcContentRow *self,
                                GtkWidget    *widget);
void cc_content_row_add_content (CcContentRow *self,
                                 GtkWidget    *widget);
void cc_content_row_remove     (CcContentRow *self,
                                GtkWidget    *widget);

const char *cc_content_row_get_subtitle (CcContentRow *self);
void        cc_content_row_set_subtitle (CcContentRow *self,
                                         const char   *subtitle);

GtkWidget *cc_content_row_get_activatable_widget (CcContentRow *self);
void       cc_content_row_set_activatable_widget (CcContentRow *self,
                                                  GtkWidget    *widget);

int  cc_content_row_get_title_lines (CcContentRow *self);
void cc_content_row_set_title_lines (CcContentRow *self,
                                     int           title_lines);

int  cc_content_row_get_subtitle_lines (CcContentRow *self);
void cc_content_row_set_subtitle_lines (CcContentRow *self,
                                        int           subtitle_lines);

gboolean cc_content_row_get_subtitle_selectable (CcContentRow *self);
void     cc_content_row_set_subtitle_selectable (CcContentRow *self,
                                                 gboolean      subtitle_selectable);

void cc_content_row_activate (CcContentRow *self);

G_END_DECLS
