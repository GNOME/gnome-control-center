/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* gnome-window-manager.h
 * Copyright (C) 2002 Seth Nickell
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Written by: Seth Nickell <snickell@stanford.edu>,
 *             Havoc Pennington <hp@redhat.com>
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

#ifndef GNOME_WINDOW_MANAGER_H
#define GNOME_WINDOW_MANAGER_H

#include <glib-object.h>

#include <libgnome/gnome-desktop-item.h>

/* Increment if backward-incompatible changes are made, so we get a clean
 * error. In principle the libtool versioning handles this, but
 * in combination with dlopen I don't quite trust that.
 */
#define GNOME_WINDOW_MANAGER_INTERFACE_VERSION 1

typedef GObject * (* GnomeWindowManagerNewFunc) (int expected_interface_version);

typedef enum
{
        GNOME_WM_SETTING_FONT                = 1 << 0,
        GNOME_WM_SETTING_MOUSE_FOCUS         = 1 << 1,
        GNOME_WM_SETTING_AUTORAISE           = 1 << 2,
        GNOME_WM_SETTING_AUTORAISE_DELAY     = 1 << 3,
        GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER = 1 << 4,
        GNOME_WM_SETTING_THEME               = 1 << 5,
        GNOME_WM_SETTING_DOUBLE_CLICK_ACTION = 1 << 6,
        GNOME_WM_SETTING_MASK                =
        GNOME_WM_SETTING_FONT                |
        GNOME_WM_SETTING_MOUSE_FOCUS         |
        GNOME_WM_SETTING_AUTORAISE           |
        GNOME_WM_SETTING_AUTORAISE_DELAY     |
        GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER |
        GNOME_WM_SETTING_THEME               |
        GNOME_WM_SETTING_DOUBLE_CLICK_ACTION
} GnomeWMSettingsFlags;

typedef struct
{
        int number;
        const char *human_readable_name;
} GnomeWMDoubleClickAction;

typedef struct
{
        GnomeWMSettingsFlags flags; /* this allows us to expand the struct
                                     * while remaining binary compatible
                                     */
        const char *font;
        int autoraise_delay;
        /* One of the strings "Alt", "Control", "Super", "Hyper", "Meta" */
        const char *mouse_move_modifier;
        const char *theme;
        int double_click_action;
        
        guint focus_follows_mouse : 1;
        guint autoraise : 1;

} GnomeWMSettings;

G_BEGIN_DECLS

#define GNOME_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gnome_window_manager_get_type (), GnomeWindowManager)
#define GNOME_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gnome_window_manager_get_type (), GnomeWindowManagerClass)
#define IS_GNOME_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gnome_window_manager_get_type ())
#define GNOME_WINDOW_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), gnome_window_manager_get_type, GnomeWindowManagerClass))

typedef struct _GnomeWindowManager GnomeWindowManager;
typedef struct _GnomeWindowManagerClass GnomeWindowManagerClass;

typedef struct _GnomeWindowManagerPrivate GnomeWindowManagerPrivate;

struct _GnomeWindowManager
{
        GObject parent;
        
        GnomeWindowManagerPrivate *p;
};

struct _GnomeWindowManagerClass
{
        GObjectClass klass;

        void         (* settings_changed)       (GnomeWindowManager    *wm);
        
        void         (* change_settings)        (GnomeWindowManager    *wm,
                                                 const GnomeWMSettings *settings);
        void         (* get_settings)           (GnomeWindowManager    *wm,
                                                 GnomeWMSettings       *settings);

        GList *      (* get_theme_list)         (GnomeWindowManager *wm);
        char *       (* get_user_theme_folder)  (GnomeWindowManager *wm);

        int          (* get_settings_mask)      (GnomeWindowManager *wm);

        void         (* get_double_click_actions) (GnomeWindowManager              *wm,
                                                   const GnomeWMDoubleClickAction **actions,
                                                   int                             *n_actions);
        
        void         (* padding_func_1)         (GnomeWindowManager *wm);
        void         (* padding_func_2)         (GnomeWindowManager *wm);
        void         (* padding_func_3)         (GnomeWindowManager *wm);
        void         (* padding_func_4)         (GnomeWindowManager *wm);
        void         (* padding_func_5)         (GnomeWindowManager *wm);
        void         (* padding_func_6)         (GnomeWindowManager *wm);
        void         (* padding_func_7)         (GnomeWindowManager *wm);
        void         (* padding_func_8)         (GnomeWindowManager *wm);
        void         (* padding_func_9)         (GnomeWindowManager *wm);
        void         (* padding_func_10)        (GnomeWindowManager *wm);
};

GObject *         gnome_window_manager_new                     (GnomeDesktopItem   *item);
GType             gnome_window_manager_get_type                (void);
const char *      gnome_window_manager_get_name                (GnomeWindowManager *wm);
GnomeDesktopItem *gnome_window_manager_get_ditem               (GnomeWindowManager *wm);

/* GList of char *'s */
GList *           gnome_window_manager_get_theme_list          (GnomeWindowManager *wm);
char *            gnome_window_manager_get_user_theme_folder   (GnomeWindowManager *wm);

/* only uses fields with their flags set */
void              gnome_window_manager_change_settings  (GnomeWindowManager    *wm,
                                                         const GnomeWMSettings *settings);
/* only gets fields with their flags set (and if it fails to get a field,
 * it unsets that flag, so flags should be checked on return)
 */
void              gnome_window_manager_get_settings     (GnomeWindowManager *wm,
                                                         GnomeWMSettings    *settings);

void              gnome_window_manager_settings_changed (GnomeWindowManager *wm);

void gnome_window_manager_get_double_click_actions (GnomeWindowManager              *wm,
                                                    const GnomeWMDoubleClickAction **actions,
                                                    int                             *n_actions);


G_END_DECLS

#endif /* GNOME_WINDOW_MANAGER_H */
