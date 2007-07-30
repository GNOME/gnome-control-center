/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _GNOME_DA_ITEM_H_
#define _GNOME_DA_ITEM_H_

#include <glib.h>

typedef struct _GnomeDAItem GnomeDAItem;

typedef struct _GnomeDAWebItem GnomeDAWebItem;
typedef struct _GnomeDATermItem GnomeDATermItem;
typedef struct _GnomeDASimpleItem GnomeDASimpleItem;
typedef struct _GnomeDAVisualItem GnomeDAVisualItem;
typedef struct _GnomeDAMobilityItem GnomeDAMobilityItem;

struct _GnomeDAItem {
    gchar *name;
    gchar *executable;
    gchar *command;
    gchar *icon_name;
    gchar *icon_path;
};

struct _GnomeDAWebItem {
    GnomeDAItem generic;
    gboolean run_in_terminal;
    gboolean netscape_remote;
    gchar *tab_command;
    gchar *win_command;
};

struct _GnomeDASimpleItem {
    GnomeDAItem generic;
    gboolean run_in_terminal;
};

struct _GnomeDATermItem {
    GnomeDAItem generic;
    gchar *exec_flag;
};

struct _GnomeDAVisualItem {
    GnomeDAItem generic;
    gboolean run_at_startup;
};

struct _GnomeDAMobilityItem {
    GnomeDAItem generic;
    gboolean run_at_startup;
};

GnomeDAWebItem* gnome_da_web_item_new (void);
GnomeDATermItem* gnome_da_term_item_new (void);
GnomeDASimpleItem* gnome_da_simple_item_new (void);
GnomeDAVisualItem* gnome_da_visual_item_new (void);
GnomeDAMobilityItem* gnome_da_mobility_item_new (void);
void gnome_da_web_item_free (GnomeDAWebItem *item);
void gnome_da_term_item_free (GnomeDATermItem *item);
void gnome_da_simple_item_free (GnomeDASimpleItem *item);
void gnome_da_visual_item_free (GnomeDAVisualItem *item);
void gnome_da_mobility_item_free (GnomeDAMobilityItem *item);

#endif
