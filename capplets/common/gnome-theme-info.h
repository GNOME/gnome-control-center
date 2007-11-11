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

#ifndef GNOME_THEME_INFO_H
#define GNOME_THEME_INFO_H

#include <config.h>

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

typedef enum {
  GNOME_THEME_TYPE_METATHEME,
  GNOME_THEME_TYPE_ICON,
  GNOME_THEME_TYPE_CURSOR,
  GNOME_THEME_TYPE_REGULAR
} GnomeThemeType;

typedef enum {
  GNOME_THEME_CHANGE_CREATED,
  GNOME_THEME_CHANGE_DELETED,
  GNOME_THEME_CHANGE_CHANGED
} GnomeThemeChangeType;


typedef enum {
  GNOME_THEME_METACITY = 1 << 0,
  GNOME_THEME_GTK_2 = 1 << 1,
  GNOME_THEME_GTK_2_KEYBINDING = 1 << 2
} GnomeThemeElement;

typedef struct _GnomeThemeCommonInfo GnomeThemeCommonInfo;
typedef struct _GnomeThemeCommonInfo GnomeThemeIconInfo;
struct _GnomeThemeCommonInfo
{
  GnomeThemeType type;
  gchar *path;
  gchar *name;
  gchar *readable_name;
  gint priority;
};

typedef struct _GnomeThemeInfo GnomeThemeInfo;
struct _GnomeThemeInfo
{
  GnomeThemeType type;
  gchar *path;
  gchar *name;
  gchar *readable_name;
  gint priority;

  guint has_gtk : 1;
  guint has_keybinding : 1;
  guint has_metacity : 1;
};

typedef struct _GnomeThemeCursorInfo GnomeThemeCursorInfo;
struct _GnomeThemeCursorInfo {
  GnomeThemeType type;
  gchar *path;
  gchar *name;
  gchar *readable_name;
  gint priority;

  GArray *sizes;
  GdkPixbuf *thumbnail;
};

typedef struct _GnomeThemeMetaInfo GnomeThemeMetaInfo;
struct _GnomeThemeMetaInfo
{
  GnomeThemeType type;
  gchar *path;
  gchar *name;
  gchar *readable_name;
  gint priority;

  gchar *comment;
  gchar *icon_file;

  gchar *gtk_theme_name;
  gchar *gtk_color_scheme;
  gchar *metacity_theme_name;
  gchar *icon_theme_name;
  gchar *sound_theme_name;
  gchar *cursor_theme_name;
  guint cursor_size;

  gchar *application_font;
  gchar *desktop_font;
  gchar *monospace_font;
  gchar *background_image;
};

enum {
  COLOR_FG,
  COLOR_BG,
  COLOR_TEXT,
  COLOR_BASE,
  COLOR_SELECTED_FG,
  COLOR_SELECTED_BG,
  COLOR_TOOLTIP_FG,
  COLOR_TOOLTIP_BG,
  NUM_SYMBOLIC_COLORS
};

typedef void (* ThemeChangedCallback) (GnomeThemeCommonInfo *theme,
				       GnomeThemeChangeType  change_type,
				       gpointer              user_data);


/* GTK/Metacity/keybinding Themes */
GnomeThemeInfo     *gnome_theme_info_new                   (void);
void                gnome_theme_info_free                  (GnomeThemeInfo     *theme_info);
GnomeThemeInfo     *gnome_theme_info_find                  (const gchar        *theme_name);
GList              *gnome_theme_info_find_by_type          (guint               elements);


/* Icon Themes */
GnomeThemeIconInfo *gnome_theme_icon_info_new              (void);
void                gnome_theme_icon_info_free             (GnomeThemeIconInfo *icon_theme_info);
GnomeThemeIconInfo *gnome_theme_icon_info_find             (const gchar        *icon_theme_name);
GList              *gnome_theme_icon_info_find_all         (void);
gint                gnome_theme_icon_info_compare          (GnomeThemeIconInfo *a,
							    GnomeThemeIconInfo *b);

/* Cursor Themes */
GnomeThemeCursorInfo *gnome_theme_cursor_info_new	   (void);
void                  gnome_theme_cursor_info_free	   (GnomeThemeCursorInfo *info);
GnomeThemeCursorInfo *gnome_theme_cursor_info_find	   (const gchar          *name);
GList                *gnome_theme_cursor_info_find_all	   (void);
gint                  gnome_theme_cursor_info_compare      (GnomeThemeCursorInfo *a,
							    GnomeThemeCursorInfo *b);

/* Meta themes*/
GnomeThemeMetaInfo *gnome_theme_meta_info_new              (void);
void                gnome_theme_meta_info_free             (GnomeThemeMetaInfo *meta_theme_info);
void                gnome_theme_meta_info_print            (GnomeThemeMetaInfo *meta_theme_info);
GnomeThemeMetaInfo *gnome_theme_meta_info_find             (const gchar        *meta_theme_name);
GList              *gnome_theme_meta_info_find_all         (void);
gint                gnome_theme_meta_info_compare          (GnomeThemeMetaInfo *a,
							    GnomeThemeMetaInfo *b);
GnomeThemeMetaInfo *gnome_theme_read_meta_theme            (GnomeVFSURI        *meta_theme_uri);

/* Other */
void                gnome_theme_init                       (gboolean            *monitor_not_added);
void                gnome_theme_info_register_theme_change (ThemeChangedCallback func,
							    gpointer             data);

gboolean            gnome_theme_color_scheme_parse         (const gchar         *scheme,
							    GdkColor            *colors);
gboolean            gnome_theme_color_scheme_equal         (const gchar         *s1,
							    const gchar         *s2);

#endif /* GNOME_THEME_INFO_H */
