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

#include "gnome-window-manager.h"

#include <gmodule.h>

static GObjectClass *parent_class;

struct _GnomeWindowManagerPrivate {
        char *window_manager_name;
        GnomeDesktopItem *ditem;
};

GObject *
gnome_window_manager_new (GnomeDesktopItem *it)
{
        const char *settings_lib;
        char *module_name;
        GnomeWindowManagerNewFunc wm_new_func = NULL;
        GObject *wm;
        GModule *module;
        gboolean success;

        settings_lib = gnome_desktop_item_get_string (it, "X-GNOME-WMSettingsModule");

        module_name = g_module_build_path (GNOME_WINDOW_MANAGER_MODULE_PATH,
                                           settings_lib);

        module = g_module_open (module_name, G_MODULE_BIND_LAZY);
        if (module == NULL) {
                g_warning ("Couldn't load window manager settings module `%s' (%s)", module_name, g_module_error ());
                return NULL;
        }

        success = g_module_symbol (module, "window_manager_new",
                                   (gpointer *) &wm_new_func);  
  
        if ((!success) || wm_new_func == NULL) {
                g_warning ("Couldn't load window manager settings module `%s`, couldn't find symbol \'window_manager_new\'", module_name);
                return NULL;
        }

        wm = (* wm_new_func) (GNOME_WINDOW_MANAGER_INTERFACE_VERSION);

        if (wm == NULL)
                return NULL;
        
        (GNOME_WINDOW_MANAGER (wm))->p->window_manager_name = g_strdup (gnome_desktop_item_get_string (it, GNOME_DESKTOP_ITEM_NAME));
        (GNOME_WINDOW_MANAGER (wm))->p->ditem = gnome_desktop_item_ref (it);
  
        return wm;
}

const char * 
gnome_window_manager_get_name (GnomeWindowManager *wm)
{
        return wm->p->window_manager_name;
}

GnomeDesktopItem *
gnome_window_manager_get_ditem (GnomeWindowManager *wm)
{
        return gnome_desktop_item_ref (wm->p->ditem);
}

GList *
gnome_window_manager_get_theme_list (GnomeWindowManager *wm)
{
        GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
        if (klass->get_theme_list)
                return klass->get_theme_list (wm);
        else
                return NULL;
}

char *
gnome_window_manager_get_user_theme_folder (GnomeWindowManager *wm)
{
        GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
        if (klass->get_user_theme_folder)
                return klass->get_user_theme_folder (wm);
        else
                return NULL;
}

void
gnome_window_manager_get_double_click_actions (GnomeWindowManager              *wm,
                                               const GnomeWMDoubleClickAction **actions,
                                               int                             *n_actions)
{
        GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);

        *actions = NULL;
        *n_actions = 0;
        
        if (klass->get_double_click_actions)
                return klass->get_double_click_actions (wm, actions, n_actions);
}

void
gnome_window_manager_change_settings  (GnomeWindowManager    *wm,
                                       const GnomeWMSettings *settings)
{
        GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
        
        (* klass->change_settings) (wm, settings);
}

void
gnome_window_manager_get_settings (GnomeWindowManager *wm,
                                   GnomeWMSettings    *settings)
{
        GnomeWindowManagerClass *klass = GNOME_WINDOW_MANAGER_GET_CLASS (wm);
        int mask;

        mask = (* klass->get_settings_mask) (wm);
        settings->flags &= mask; /* avoid back compat issues by not returning
                                  * fields to the caller that the WM module
                                  * doesn't know about
                                  */
        
        (* klass->get_settings) (wm, settings);
}

static void
gnome_window_manager_init (GnomeWindowManager *gnome_window_manager, GnomeWindowManagerClass *class)
{
	gnome_window_manager->p = g_new0 (GnomeWindowManagerPrivate, 1);
}

static void
gnome_window_manager_finalize (GObject *object) 
{
	GnomeWindowManager *gnome_window_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_GNOME_WINDOW_MANAGER (object));

	gnome_window_manager = GNOME_WINDOW_MANAGER (object);

	g_free (gnome_window_manager->p);

	parent_class->finalize (object);
}

enum {
  SETTINGS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gnome_window_manager_class_init (GnomeWindowManagerClass *class) 
{
	GObjectClass *object_class;
	GnomeWindowManagerClass *wm_class;

	object_class = G_OBJECT_CLASS (class);
	wm_class = GNOME_WINDOW_MANAGER_CLASS (class);

	object_class->finalize = gnome_window_manager_finalize;
        
	parent_class = g_type_class_peek_parent (class);

        
        signals[SETTINGS_CHANGED] =
                g_signal_new ("settings_changed",
                              G_OBJECT_CLASS_TYPE (class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                              G_STRUCT_OFFSET (GnomeWindowManagerClass, settings_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}


GType
gnome_window_manager_get_type (void)
{
	static GType gnome_window_manager_type = 0;

	if (!gnome_window_manager_type) {
		static GTypeInfo gnome_window_manager_info = {
			sizeof (GnomeWindowManagerClass),
			NULL, /* GBaseInitFunc */
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) gnome_window_manager_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (GnomeWindowManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) gnome_window_manager_init,
			NULL
		};

		gnome_window_manager_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"GnomeWindowManager",
						&gnome_window_manager_info, 0);                
	}

	return gnome_window_manager_type;
}


void
gnome_window_manager_settings_changed (GnomeWindowManager *wm)
{
        g_signal_emit (wm, signals[SETTINGS_CHANGED], 0);
}
