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
  GnomeThumbnailFactory *thumb_factory;

  /* desktop */
  GHashTable *wp_hash;
  gboolean wp_update_gconf;
  GtkIconView *wp_view;
  GtkTreeModel *wp_model;
  GtkWidget *wp_scpicker;
  GtkWidget *wp_pcpicker;
  GtkWidget *wp_style_menu;
  GtkWidget *wp_color_menu;
  GtkWidget *wp_rem_button;
  GtkFileChooser *wp_filesel;
  GtkWidget *wp_image;
  GSList *wp_uris;

  /* desktop effects */
  GtkWidget *enable_effects_button;
  GtkWidget *customize_effects_button;

  /* font */
  GtkWidget *font_details;
  GSList *font_groups;

  /* themes */
  GtkListStore *theme_store;
  GnomeThemeMetaInfo *theme_custom;
  GdkPixbuf *theme_icon;
  GtkWidget *theme_save_dialog;
  GtkWidget *theme_message_area;
  GtkWidget *theme_message_label;
  GtkWidget *apply_background_button;
  GtkWidget *revert_font_button;
  GtkWidget *apply_font_button;
  gchar *revert_application_font;
  gchar *revert_documents_font;
  gchar *revert_desktop_font;
  gchar *revert_windowtitle_font;
  gchar *revert_monospace_font;

  /* style */
  GdkPixbuf *gtk_theme_icon;
  GdkPixbuf *window_theme_icon;
  GdkPixbuf *icon_theme_icon;
} AppearanceData;
