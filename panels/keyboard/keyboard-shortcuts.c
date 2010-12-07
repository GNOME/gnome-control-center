/*
 * Copyright (C) 2010 Intel, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include "keyboard-shortcuts.h"

GHashTable *kb_sections = NULL;

static void
free_key_list (gpointer list)
{
}

static void
setup_dialog (CcPanel *panel, GtkBuilder *builder)
{
}

void
keyboard_shortcuts_init (CcPanel *panel, GtkBuilder *builder)
{
  kb_sections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_key_list);
  setup_dialog (panel, builder);
}

void
keyboard_shortcuts_dispose (CcPanel *panel)
{
}
