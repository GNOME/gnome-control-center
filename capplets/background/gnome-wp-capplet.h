/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
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

#ifndef _GNOME_WP_CAPPLET_H_
#define _GNOME_WP_CAPPLET_H_

#include <config.h>
#include <gnome.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <gconf/gconf-client.h>
#include <libxml/parser.h>
#include <glade/glade.h>

typedef struct _GnomeWPCapplet GnomeWPCapplet;

#include "gnome-wp-info.h"
#include "gnome-wp-item.h"
#include "gnome-wp-utils.h"
#include "gnome-wp-xml.h"

struct _GnomeWPCapplet {
  GtkWidget * window;

  /* The Tree View */
  GtkWidget * treeview;
  GtkTreeModel * model;

  /* Option Menu for Scaling Options */
  GtkWidget * wp_opts;
  GtkWidget * rm_button;

  /* Menu Items for Fill/Scale/Center/Tile Options */
  GtkWidget * fitem;
  GtkWidget * sitem;
  GtkWidget * citem;
  GtkWidget * witem;

  /* Widgets for Color Options */
  GtkWidget * color_opt;
  GtkWidget * pc_picker;
  GtkWidget * sc_picker;

  /* Menu Items for Color Style Options */
  GtkWidget * smenuitem;
  GtkWidget * hmenuitem;
  GtkWidget * vmenuitem;

  /* GConf Client */
  GConfClient * client;

  /* Thumbnailing and Icon Theme stuff */
  GnomeThumbnailFactory * thumbs;
  GnomeIconTheme * theme;

  /* Hash Table of Wallpapers */
  GHashTable * wphash;

  /* Keyboard Delay */
  gint delay;

  /* The Timeout ID for Setting the Wallpaper */
  gint idleid;

  /* File Chooser Dialog */
  GtkWidget * filesel;
};

typedef enum {
  GNOME_WP_SHADE_TYPE_SOLID,
  GNOME_WP_SHADE_TYPE_HORIZ,
  GNOME_WP_SHADE_TYPE_VERT
} GnomeWPShadeType;

typedef enum {
  GNOME_WP_SCALE_TYPE_CENTERED,
  GNOME_WP_SCALE_TYPE_STRETCHED,
  GNOME_WP_SCALE_TYPE_SCALED,
  GNOME_WP_SCALE_TYPE_TILED
} GnomeWPScaleType;

#define WP_PATH_KEY "/desktop/gnome/background"
#define WP_FILE_KEY WP_PATH_KEY "/picture_filename"
#define WP_OPTIONS_KEY WP_PATH_KEY "/picture_options"
#define WP_SHADING_KEY WP_PATH_KEY "/color_shading_type"
#define WP_PCOLOR_KEY WP_PATH_KEY "/primary_color"
#define WP_SCOLOR_KEY WP_PATH_KEY "/secondary_color"
#define WP_KEYBOARD_PATH "/desktop/gnome/peripherals/keyboard"
#define WP_DELAY_KEY WP_KEYBOARD_PATH "/delay"

void gnome_wp_main_quit (GnomeWPCapplet * capplet);

#endif

