/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkb.h
 * Copyright (C) 2003 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sf.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __GNOME_KEYBOARD_PROPERTY_XKB_H
#define __GNOME_KEYBOARD_PROPERTY_XKB_H

#include <libxklavier/xklavier_config.h>

G_BEGIN_DECLS
    extern void setup_xkb_tabs (GladeXML * dialog,
				GConfChangeSet * changeset);

extern void fill_available_layouts_tree (GladeXML * dialog);

extern void fill_available_options_tree (GladeXML * dialog);

extern void fill_selected_layouts_tree (GladeXML * dialog);

extern void fill_selected_options_tree (GladeXML * dialog);

extern void register_layouts_buttons_handlers (GladeXML * dialog);

extern void register_options_buttons_handlers (GladeXML * dialog);

extern void register_layouts_gconf_listener (GladeXML * dialog);

extern void register_options_gconf_listener (GladeXML * dialog);

extern void prepare_selected_layouts_tree (GladeXML * dialog);

extern void prepare_selected_options_tree (GladeXML * dialog);

extern void clear_xkb_elements_list (GSList * list);

extern char *xci_desc_to_utf8 (XklConfigItem * ci);

extern void sort_tree_content (GtkWidget * treeView);

extern void enable_disable_restoring(GladeXML * dialog);

G_END_DECLS
#endif				/* __GNOME_KEYBOARD_PROPERTY_XKB_H */
