/* gnome-theme-info.h - GNOME Theme information

   Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#ifndef GNOME_THEME_INFO_H
#define GNOME_THEME_INFO_H

#include <glib.h>


typedef enum {
  GNOME_THEME_METACITY = 1 << 0,
  GNOME_THEME_GTK_2 = 1 << 1,
  GNOME_THEME_GTK_2_KEYBINDING = 1 << 2,
} GnomeThemeElement;

typedef struct _GnomeThemeInfo GnomeThemeInfo;
struct _GnomeThemeInfo
{
  gchar *path;
  gchar *name;
  guint has_gtk : 1;
  guint has_keybinding : 1;
  guint has_metacity : 1;
  guint user_writable : 1;
};

typedef struct _GnomeThemeIconInfo GnomeThemeIconInfo;
struct _GnomeThemeIconInfo
{
  gchar *path;
  gchar *name;
};

typedef struct _GnomeThemeMetaInfo GnomeThemeMetaInfo;
struct _GnomeThemeMetaInfo
{
  gchar *path;
  gchar *name;
  gchar *font;
  gchar *comment;
  gchar *icon_file;
  gchar *background;
  gchar *gtk_theme_name;
  gchar *icon_theme_name;
  gchar *metacity_theme_name;
  GnomeThemeInfo *gtk_theme;
  GnomeThemeInfo *icon_theme;
  GnomeThemeInfo *metacity_theme;
};


/* Generic Themes */
GnomeThemeInfo     *gnome_theme_info_new                   (void);
void                gnome_theme_info_free                  (GnomeThemeInfo     *theme_info);
GnomeThemeInfo     *gnome_theme_info_find                  (const gchar        *theme_name);
GList              *gnome_theme_info_find_by_type          (guint               elements);
GnomeThemeInfo     *gnome_theme_info_find_by_dir           (const gchar        *theme_dir);



/* Icon Themes */
GnomeThemeIconInfo *gnome_theme_icon_info_new              (void);
void                gnome_theme_icon_info_free             (GnomeThemeIconInfo *icon_theme_info);
GnomeThemeInfo     *gnome_theme_icon_info_find             (const gchar        *icon_theme_name);
GList              *gnome_theme_icon_info_find_all         (void);

 
/* Meta themes*/
GnomeThemeMetaInfo *gnome_theme_meta_info_new              (void);
void                gnome_theme_meta_info_free             (GnomeThemeMetaInfo *meta_theme_info);
GnomeThemeMetaInfo *gnome_theme_meta_info_find             (const char         *meta_theme_name);
GList              *gnome_theme_meta_info_find_all         (void);



/* Theme monitoring */
void                gnome_theme_info_register_theme_change (GFunc               func,
							    gpointer            data);


#endif /* GNOME_THEME_INFO_H */
