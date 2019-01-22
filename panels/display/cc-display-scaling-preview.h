/* cc-display-scaling-preview.h
 *
 * Copyright (C) 2019  Red Hat, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_SCALING_PREVIEW (cc_display_scaling_preview_get_type())

G_DECLARE_FINAL_TYPE (CcDisplayScalingPreview, cc_display_scaling_preview, CC, DISPLAY_SCALING_PREVIEW, GtkDrawingArea)

CcDisplayScalingPreview *cc_display_scaling_preview_new (void);

void                     cc_display_scaling_preview_set_scaling  (CcDisplayScalingPreview *preview,
                                                                  gdouble                  scaling,
                                                                  gdouble                  active_scaling);
void                     cc_display_scaling_preview_get_scaling  (CcDisplayScalingPreview *preview,
                                                                  gdouble                 *scaling,
                                                                  gdouble                 *active_scaling);
void                     cc_display_scaling_preview_set_selected (CcDisplayScalingPreview *preview,
                                                                  gboolean                 selected);

G_END_DECLS
