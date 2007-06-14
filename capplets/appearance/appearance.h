/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-thumbnail.h>

#include "gnome-theme-info.h"

typedef struct {
  GConfClient *client;
  GladeXML *xml;

  /* desktop */
  GHashTable *wp_hash;
  GnomeThumbnailFactory *wp_thumbs;
  gboolean wp_update_gconf;
  GtkIconView *wp_view;
  GtkTreeModel *wp_model;
  GtkCellRenderer *wp_cell;
  GtkWidget *wp_scpicker;
  GtkWidget *wp_pcpicker;
  GtkWidget *wp_style_menu;
  GtkWidget *wp_color_menu;
  GtkWidget *wp_rem_button;
  GtkWidget *wp_filesel;
  GtkWidget *wp_image;

  /* font */
  GtkWidget *font_details;

  /* themes */
  GtkListStore *theme_store;
  GSList *theme_queue;
  GnomeThemeMetaInfo *theme_custom;
  GdkPixbuf *theme_icon;
} AppearanceData;
