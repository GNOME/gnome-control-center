/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_OPTION         (cc_keyboard_option_get_type ())
G_DECLARE_FINAL_TYPE (CcKeyboardOption, cc_keyboard_option, CC, KEYBOARD_OPTION, GObject)

enum
{
  XKB_OPTION_DESCRIPTION_COLUMN,
  XKB_OPTION_ID_COLUMN,
  XKB_OPTION_N_COLUMNS
};

GList *         cc_keyboard_option_get_all              (void);
const gchar *   cc_keyboard_option_get_description      (CcKeyboardOption *self);
GtkListStore *  cc_keyboard_option_get_store            (CcKeyboardOption *self);
const gchar *   cc_keyboard_option_get_current_value_description (CcKeyboardOption *self);
void            cc_keyboard_option_set_selection        (CcKeyboardOption *self,
                                                         GtkTreeIter      *iter);
void            cc_keyboard_option_clear_all            (void);

G_END_DECLS
