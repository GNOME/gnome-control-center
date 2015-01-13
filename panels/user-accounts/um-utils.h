/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __UM_UTILS_H__
#define __UM_UTILS_H__

#include <gtk/gtk.h>
#include <act/act.h>

G_BEGIN_DECLS

typedef enum {
        UM_ICON_STYLE_NONE   = 0,
        UM_ICON_STYLE_FRAME  = 1 << 0,
        UM_ICON_STYLE_STATUS = 1 << 1
} UmIconStyle;

void     setup_tooltip_with_embedded_icon (GtkWidget   *widget,
                                           const gchar *text,
                                           const gchar *placeholder,
                                           GIcon       *icon);
gboolean show_tooltip_now                 (GtkWidget   *widget,
                                           GdkEvent    *event);

void     set_entry_generation_icon        (GtkEntry    *entry);
void     set_entry_validation_checkmark   (GtkEntry    *entry);
void     set_entry_validation_error       (GtkEntry    *entry,
                                           const gchar *text);
void     clear_entry_validation_error     (GtkEntry    *entry);

void     popup_menu_below_button          (GtkMenu     *menu,
                                           gint        *x,
                                           gint        *y,
                                           gboolean    *push_in,
                                           GtkWidget   *button);

void     rounded_rectangle                (cairo_t     *cr,
                                           gdouble      aspect,
                                           gdouble      x,
                                           gdouble      y,
                                           gdouble      corner_radius,
                                           gdouble      width,
                                           gdouble      height);

void     down_arrow                       (GtkStyleContext *context,
                                           cairo_t         *cr,
                                           gint             x,
                                           gint             y,
                                           gint             width,
                                           gint             height);

gboolean is_valid_name                    (const gchar     *name);
gboolean is_valid_username                (const gchar     *name,
                                           gchar          **tip);

void     generate_username_choices        (const gchar     *name,
                                           GtkListStore    *store);

gchar *  get_smart_date                   (GDateTime *date);

cairo_surface_t *render_user_icon         (ActUser         *user,
                                           UmIconStyle      style,
                                           gint             icon_size,
                                           gint             scale);

void     set_user_icon_data               (ActUser         *user,
                                           GdkPixbuf       *pixbuf);

gboolean would_demote_only_admin          (ActUser *user);

G_END_DECLS

#endif
