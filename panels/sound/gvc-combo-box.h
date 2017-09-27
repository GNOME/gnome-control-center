/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifndef __GVC_COMBO_BOX_H
#define __GVC_COMBO_BOX_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GVC_TYPE_COMBO_BOX (gvc_combo_box_get_type ())
G_DECLARE_FINAL_TYPE (GvcComboBox, gvc_combo_box, GVC, COMBO_BOX, GtkBox)

GtkWidget *         gvc_combo_box_new                 (const char   *label);

void                gvc_combo_box_set_size_group      (GvcComboBox  *combo_box,
                                                       GtkSizeGroup *group,
                                                       gboolean      symmetric);

void                gvc_combo_box_set_profiles        (GvcComboBox  *combo_box,
                                                       const GList  *profiles);
void                gvc_combo_box_set_ports           (GvcComboBox  *combo_box,
                                                       const GList  *ports);
void                gvc_combo_box_set_active          (GvcComboBox  *combo_box,
                                                       const char   *id);

G_END_DECLS

#endif /* __GVC_COMBO_BOX_H */
