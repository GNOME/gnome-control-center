/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* metacity-window-manager.c
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

#include <config.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>

#include "metacity-window-manager.h"

#define METACITY_THEME_KEY "/apps/metacity/general/theme"
#define METACITY_FONT_KEY  "/apps/metacity/general/titlebar_font"
#define METACITY_FOCUS_KEY "/apps/metacity/general/focus_mode"
#define METACITY_USE_SYSTEM_FONT_KEY "/apps/metacity/general/titlebar_uses_system_font"
#define METACITY_AUTORAISE_KEY "/apps/metacity/general/auto_raise"
#define METACITY_AUTORAISE_DELAY_KEY "/apps/metacity/general/auto_raise_delay"
#define METACITY_MOUSE_MODIFIER_KEY "/apps/metacity/general/mouse_button_modifier"
#define METACITY_DOUBLE_CLICK_TITLEBAR_KEY "/apps/metacity/general/action_double_click_titlebar"

enum
{
        DOUBLE_CLICK_MAXIMIZE,
        DOUBLE_CLICK_SHADE
};

static GnomeWindowManagerClass *parent_class;

struct _MetacityWindowManagerPrivate {
        GConfClient *gconf;
        char *font;
        char *theme;
        char *mouse_modifier;
};

static void
value_changed (GConfClient *client,
               const gchar *key,
               GConfValue  *value,
               void        *data)
{
        MetacityWindowManager *meta_wm;

        meta_wm = METACITY_WINDOW_MANAGER (data);

        gnome_window_manager_settings_changed (GNOME_WINDOW_MANAGER (meta_wm));
}

/* this function is called when the shared lib is loaded */
GObject *
window_manager_new (int expected_interface_version)
{
        GObject *wm;

        if (expected_interface_version != GNOME_WINDOW_MANAGER_INTERFACE_VERSION) {
                g_warning ("Metacity window manager module wasn't compiled with the current version of gnome-control-center");
                return NULL;
        }
  
        wm = g_object_new (metacity_window_manager_get_type (), NULL);

        return wm;
}

static GList *
add_themes_from_dir (GList *current_list, const char *path)
{
        DIR *theme_dir;
        struct dirent *entry;
        char *theme_file_path;
        GList *node;
        gboolean found = FALSE;

        if (!(g_file_test (path, G_FILE_TEST_EXISTS) && g_file_test (path, G_FILE_TEST_IS_DIR))) {
                return current_list;
        }

        theme_dir = opendir (path);

        for (entry = readdir (theme_dir); entry != NULL; entry = readdir (theme_dir)) {
                theme_file_path = g_build_filename (path, entry->d_name, "metacity-theme-1.xml", NULL);

                if (g_file_test (theme_file_path, G_FILE_TEST_EXISTS)) {

                        for (node = current_list; (node != NULL) && (!found); node = node->next) {
                                found = (strcmp (node->data, entry->d_name) == 0);
                        }
      
                        if (!found) {
                                current_list = g_list_prepend (current_list, g_strdup (entry->d_name));
                        }
                }

                /*g_free (entry);*/
                g_free (theme_file_path);
        }
   
        closedir (theme_dir);

        return current_list;
}

static GList *  
metacity_get_theme_list (GnomeWindowManager *wm)
{
        GList *themes = NULL;
        char *home_dir_themes;

        home_dir_themes = g_build_filename (g_get_home_dir (), ".metacity/themes", NULL);

        themes = add_themes_from_dir (themes, METACITY_THEME_DIR);
        themes = add_themes_from_dir (themes, "/usr/share/metacity/themes");
        themes = add_themes_from_dir (themes, home_dir_themes);

        g_free (home_dir_themes);

        return themes;
}

static char *
metacity_get_user_theme_folder (GnomeWindowManager *wm)
{
        return g_build_filename (g_get_home_dir (), ".themes", NULL);
}

static void
metacity_change_settings (GnomeWindowManager    *wm,
                          const GnomeWMSettings *settings)
{
        MetacityWindowManager *meta_wm;

        meta_wm = METACITY_WINDOW_MANAGER (wm);
        
        if (settings->flags & GNOME_WM_SETTING_MOUSE_FOCUS)
                gconf_client_set_string (meta_wm->p->gconf,
                                         METACITY_FOCUS_KEY,
                                         settings->focus_follows_mouse ?
                                         "sloppy" : "click", NULL);

        if (settings->flags & GNOME_WM_SETTING_AUTORAISE)
                gconf_client_set_bool (meta_wm->p->gconf,
                                       METACITY_AUTORAISE_KEY,
                                       settings->autoraise, NULL);
        
        if (settings->flags & GNOME_WM_SETTING_AUTORAISE_DELAY)
                gconf_client_set_int (meta_wm->p->gconf,
                                      METACITY_AUTORAISE_DELAY_KEY,
                                      settings->autoraise_delay, NULL);

        if (settings->flags & GNOME_WM_SETTING_FONT) {
                gconf_client_set_string (meta_wm->p->gconf,
                                         METACITY_FONT_KEY,
                                         settings->font, NULL);
        }
        
        if (settings->flags & GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER) {
                char *value;

                value = g_strdup_printf ("<%s>", settings->mouse_move_modifier);
                gconf_client_set_string (meta_wm->p->gconf,
                                         METACITY_MOUSE_MODIFIER_KEY,
                                         value, NULL);
                g_free (value);
        }

        if (settings->flags & GNOME_WM_SETTING_THEME) {
                gconf_client_set_string (meta_wm->p->gconf,
                                         METACITY_THEME_KEY,
                                         settings->theme, NULL);
        }

        if (settings->flags & GNOME_WM_SETTING_DOUBLE_CLICK_ACTION) {
                const char *action;

                action = NULL;
                
                switch (settings->double_click_action) {
                case DOUBLE_CLICK_SHADE:
                        action = "toggle_shade";
                        break;
                case DOUBLE_CLICK_MAXIMIZE:
                        action = "toggle_maximize";
                        break;
                }

                if (action != NULL) {
                        gconf_client_set_string (meta_wm->p->gconf,
                                                 METACITY_DOUBLE_CLICK_TITLEBAR_KEY,
                                                 action, NULL);
                }
        }
}

static void
metacity_get_settings (GnomeWindowManager *wm,
                       GnomeWMSettings    *settings)
{
        int to_get;
        MetacityWindowManager *meta_wm;

        meta_wm = METACITY_WINDOW_MANAGER (wm);
        
        to_get = settings->flags;
        settings->flags = 0;
        
        if (to_get & GNOME_WM_SETTING_MOUSE_FOCUS) {
                char *str;

                str = gconf_client_get_string (meta_wm->p->gconf,
                                               METACITY_FOCUS_KEY, NULL);
                settings->focus_follows_mouse = FALSE;
                if (str && (strcmp (str, "sloppy") == 0 ||
                            strcmp (str, "mouse") == 0))
                        settings->focus_follows_mouse = TRUE;

                g_free (str);
                
                settings->flags |= GNOME_WM_SETTING_MOUSE_FOCUS;
        }
        
        if (to_get & GNOME_WM_SETTING_AUTORAISE) {
                settings->autoraise = gconf_client_get_bool (meta_wm->p->gconf,
                                                             METACITY_AUTORAISE_KEY,
                                                             NULL);
                settings->flags |= GNOME_WM_SETTING_AUTORAISE;
        }
        
        if (to_get & GNOME_WM_SETTING_AUTORAISE_DELAY) {
                settings->autoraise_delay =
                        gconf_client_get_int (meta_wm->p->gconf,
                                              METACITY_AUTORAISE_DELAY_KEY,
                                              NULL);
                settings->flags |= GNOME_WM_SETTING_AUTORAISE_DELAY;
        }

        if (to_get & GNOME_WM_SETTING_FONT) {
                char *str;

                str = gconf_client_get_string (meta_wm->p->gconf,
                                               METACITY_FONT_KEY,
                                               NULL);

                if (str == NULL)
                        str = g_strdup ("Sans Bold 12");

                if (meta_wm->p->font &&
                    strcmp (meta_wm->p->font, str) == 0) {
                        g_free (str);
                } else {
                        g_free (meta_wm->p->font);
                        meta_wm->p->font = str;
                }
                
                settings->font = meta_wm->p->font;

                settings->flags |= GNOME_WM_SETTING_FONT;
        }
        
        if (to_get & GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER) {
                char *str;
                const char *new;

                str = gconf_client_get_string (meta_wm->p->gconf,
                                               METACITY_MOUSE_MODIFIER_KEY,
                                               NULL);

                if (str == NULL)
                        str = g_strdup ("<Super>");

                if (strcmp (str, "<Super>") == 0)
                        new = "Super";
                else if (strcmp (str, "<Alt>") == 0)
                        new = "Alt";
                else if (strcmp (str, "<Meta>") == 0)
                        new = "Meta";
                else if (strcmp (str, "<Hyper>") == 0)
                        new = "Hyper";
                else if (strcmp (str, "<Control>") == 0)
                        new = "Control";
                else
                        new = NULL;

                if (new && meta_wm->p->mouse_modifier &&
                    strcmp (new, meta_wm->p->mouse_modifier) == 0) {
                        /* unchanged */;
                } else {
                        g_free (meta_wm->p->mouse_modifier);
                        meta_wm->p->mouse_modifier = g_strdup (new);
                }

                g_free (str);

                settings->mouse_move_modifier = meta_wm->p->mouse_modifier;
                
                settings->flags |= GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER;
        }

        if (to_get & GNOME_WM_SETTING_THEME) {
                char *str;

                str = gconf_client_get_string (meta_wm->p->gconf,
                                               METACITY_THEME_KEY,
                                               NULL);

                if (str == NULL)
                        str = g_strdup ("Atlanta");

                if (meta_wm->p->theme &&
                    strcmp (meta_wm->p->theme, str) == 0) {
                        g_free (str);
                } else {
                        g_free (meta_wm->p->theme);
                        meta_wm->p->font = str;
                }
                
                settings->theme = meta_wm->p->theme;

                settings->flags |= GNOME_WM_SETTING_THEME;
        }

        if (to_get & GNOME_WM_SETTING_DOUBLE_CLICK_ACTION) {
                char *str;

                str = gconf_client_get_string (meta_wm->p->gconf,
                                               METACITY_DOUBLE_CLICK_TITLEBAR_KEY,
                                               NULL);
                
                if (str == NULL)
                        str = g_strdup ("toggle_shade");
                
                if (strcmp (str, "toggle_shade") == 0)
                        settings->double_click_action = DOUBLE_CLICK_SHADE;
                else if (strcmp (str, "toggle_maximize") == 0)
                        settings->double_click_action = DOUBLE_CLICK_MAXIMIZE;
                else
                        settings->double_click_action = DOUBLE_CLICK_SHADE;
                
                g_free (str);
                
                settings->flags |= GNOME_WM_SETTING_DOUBLE_CLICK_ACTION;             
        }
}

static int
metacity_get_settings_mask (GnomeWindowManager *wm)
{
        return GNOME_WM_SETTING_MASK;
}

static void
metacity_get_double_click_actions (GnomeWindowManager              *wm,
                                   const GnomeWMDoubleClickAction **actions_p,
                                   int                             *n_actions_p)
{
        static GnomeWMDoubleClickAction actions[] = {
                { DOUBLE_CLICK_MAXIMIZE, N_("Maximize") },
                { DOUBLE_CLICK_SHADE, N_("Roll up") }
        };
        static gboolean initialized = FALSE;        

        if (!initialized) {
                int i;
                
                initialized = TRUE;
                i = 0;
                while (i < (int) G_N_ELEMENTS (actions)) {
                        g_assert (actions[i].number == i);
                        actions[i].human_readable_name = _(actions[i].human_readable_name);
                        
                        ++i;
                }
        }

        *actions_p = actions;
        *n_actions_p = (int) G_N_ELEMENTS (actions);        
}

static void
metacity_window_manager_init (MetacityWindowManager *metacity_window_manager,
                              MetacityWindowManagerClass *class)
{
        metacity_window_manager->p = g_new0 (MetacityWindowManagerPrivate, 1);
        metacity_window_manager->p->gconf = gconf_client_get_default ();
        metacity_window_manager->p->font = NULL;
        metacity_window_manager->p->theme = NULL;
        metacity_window_manager->p->mouse_modifier = NULL;
        
        gconf_client_add_dir (metacity_window_manager->p->gconf,
                              "/apps/metacity/general",
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);

        g_signal_connect (G_OBJECT (metacity_window_manager->p->gconf),
                          "value_changed",
                          G_CALLBACK (value_changed), metacity_window_manager);
}

static void
metacity_window_manager_finalize (GObject *object) 
{
        MetacityWindowManager *metacity_window_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_METACITY_WINDOW_MANAGER (object));

        metacity_window_manager = METACITY_WINDOW_MANAGER (object);
        
        g_signal_handlers_disconnect_by_func (G_OBJECT (metacity_window_manager->p->gconf),
                                              G_CALLBACK (value_changed),
                                              metacity_window_manager);
        
        g_object_unref (G_OBJECT (metacity_window_manager->p->gconf));
        g_free (metacity_window_manager->p);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
metacity_window_manager_class_init (MetacityWindowManagerClass *class) 
{
        GObjectClass *object_class;
        GnomeWindowManagerClass *wm_class;

        object_class = G_OBJECT_CLASS (class);
        wm_class = GNOME_WINDOW_MANAGER_CLASS (class);

        object_class->finalize = metacity_window_manager_finalize;

        wm_class->change_settings          = metacity_change_settings;
        wm_class->get_settings             = metacity_get_settings;
        wm_class->get_settings_mask        = metacity_get_settings_mask;
        wm_class->get_user_theme_folder    = metacity_get_user_theme_folder;
        wm_class->get_theme_list           = metacity_get_theme_list;
        wm_class->get_double_click_actions = metacity_get_double_click_actions;
        
        parent_class = g_type_class_peek_parent (class);
}

GType
metacity_window_manager_get_type (void)
{
        static GType metacity_window_manager_type = 0;

        if (!metacity_window_manager_type) {
                static GTypeInfo metacity_window_manager_info = {
                        sizeof (MetacityWindowManagerClass),
                        NULL, /* GBaseInitFunc */
                        NULL, /* GBaseFinalizeFunc */
                        (GClassInitFunc) metacity_window_manager_class_init,
                        NULL, /* GClassFinalizeFunc */
                        NULL, /* user-supplied data */
                        sizeof (MetacityWindowManager),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) metacity_window_manager_init,
                        NULL
                };

                metacity_window_manager_type = 
                        g_type_register_static (gnome_window_manager_get_type (), 
                                                "MetacityWindowManager",
                                                &metacity_window_manager_info, 0);
        }

        return metacity_window_manager_type;
}


