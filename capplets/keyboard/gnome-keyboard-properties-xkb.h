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
#include <gconf/gconf-client.h>

#include "libgswitchit/gswitchit_config.h"

G_BEGIN_DECLS

#define SEL_LAYOUT_TREE_COL_DESCRIPTION 0
#define SEL_LAYOUT_TREE_COL_DEFAULT 1
#define SEL_LAYOUT_TREE_COL_ID 2

#define AVAIL_LAYOUT_TREE_COL_DESCRIPTION 0
#define AVAIL_LAYOUT_TREE_COL_ID 1

#define CWID(s) glade_xml_get_widget (chooserDialog, s)

extern GConfClient *xkbGConfClient;
extern GSwitchItKbdConfig initialConfig;

extern void setup_xkb_tabs (GladeXML * dialog, 
                            GConfChangeSet * changeset);

extern void xkb_layouts_fill_available_tree (GladeXML * dialog);

extern void xkb_options_fill_available_tree (GladeXML * dialog);

extern void xkb_layouts_fill_selected_tree (GladeXML * dialog);

extern void xkb_options_fill_selected_tree (GladeXML * dialog);

extern void xkb_layouts_register_buttons_handlers (GladeXML * dialog);

extern void xkb_options_register_buttons_handlers (GladeXML * dialog);

extern void xkb_layouts_register_gconf_listener (GladeXML * dialog);

extern void xkb_options_register_gconf_listener (GladeXML * dialog);

extern void xkb_layouts_prepare_selected_tree (GladeXML * dialog, 
                                               GConfChangeSet * changeset);

extern void xkb_options_prepare_selected_tree (GladeXML * dialog);

extern void xkb_options_load_options (GladeXML * dialog);

extern void clear_xkb_elements_list (GSList * list);

extern char *xci_desc_to_utf8 (XklConfigItem * ci);

extern void sort_tree_content (GtkWidget * treeView);

extern void enable_disable_restoring (GladeXML * dialog);

extern void preview_toggled (GladeXML * dialog, GtkWidget * button);

extern void choose_model (GladeXML * dialog);

extern void xkb_layout_choose (GladeXML * dialog);

extern void xkb_layouts_enable_disable_default (GladeXML * dialog, 
                                                gboolean enable);

extern GSList *xkb_layouts_get_selected_list (void);
extern GSList *xkb_options_get_selected_list (void);

#define xkb_layouts_set_selected_list(list) \
        gconf_client_set_list (gconf_client_get_default (), \
                               GSWITCHIT_KBD_CONFIG_KEY_LAYOUTS, \
                               GCONF_VALUE_STRING, (list), NULL)

#define xkb_options_set_selected_list(list) \
        gconf_client_set_list (gconf_client_get_default (), \
                               GSWITCHIT_KBD_CONFIG_KEY_OPTIONS, \
                               GCONF_VALUE_STRING, (list), NULL)

extern GtkWidget * xkb_layout_preview_create_widget (GladeXML * chooserDialog);

extern void xkb_layout_preview_update (GladeXML * chooserDialog);

G_END_DECLS
#endif				/* __GNOME_KEYBOARD_PROPERTY_XKB_H */
