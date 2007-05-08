/*
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Authors:  Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GNOME_SETTINGS_MODULE_H__
#define __GNOME_SETTINGS_MODULE_H__

#include <glib-object.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

#define GNOME_SETTINGS_TYPE_MODULE         (gnome_settings_module_get_type ())
#define GNOME_SETTINGS_MODULE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_SETTINGS_TYPE_MODULE, GnomeSettingsModule))
#define GNOME_SETTINGS_MODULE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_SETTINGS_TYPE_MODULE, GnomeSettingsModuleClass))
#define GNOME_SETTINGS_IS_MODULE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_SETTINGS_TYPE_MODULE))
#define GNOME_SETTINGS_IS_MODULE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_SETTINGS_TYPE_MODULE))
#define GNOME_SETTINGS_MODULE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_SETTINGS_TYPE_MODULE, GnomeSettingsModuleClass))

typedef struct _GnomeSettingsModule        GnomeSettingsModule;
typedef struct _GnomeSettingsModuleClass   GnomeSettingsModuleClass;
typedef struct _GnomeSettingsModulePrivate GnomeSettingsModulePrivate;

typedef enum {
	GNOME_SETTINGS_MODULE_STATUS_NONE,
	GNOME_SETTINGS_MODULE_STATUS_INITIALIZED,
	GNOME_SETTINGS_MODULE_STATUS_STARTED,
	GNOME_SETTINGS_MODULE_STATUS_STOPPED
} GnomeSettingsModuleStatus;

typedef enum {
	GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS,
	GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS,
	GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES,
	GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES,
	GNOME_SETTINGS_MODULE_RUNLEVEL_NONE
} GnomeSettingsModuleRunlevel;

struct _GnomeSettingsModule {
	GObject parent;
	GnomeSettingsModulePrivate *priv;
};

struct _GnomeSettingsModuleClass {
	GObjectClass parent_class;

	/* virtual methods */
	GnomeSettingsModuleRunlevel (* get_runlevel) (GnomeSettingsModule *module);
	gboolean (* initialize) (GnomeSettingsModule *module, GConfClient *config_client);
	gboolean (* start) (GnomeSettingsModule *module);
	gboolean (* stop) (GnomeSettingsModule *module);
	gboolean (* reload_settings) (GnomeSettingsModule *module);
};

GType    gnome_settings_module_get_type (void);

gboolean gnome_settings_module_initialize (GnomeSettingsModule *module, GConfClient *client);
gboolean gnome_settings_module_start (GnomeSettingsModule *module);
gboolean gnome_settings_module_stop (GnomeSettingsModule *module);
gboolean gnome_settings_module_reload_settings (GnomeSettingsModule *module);

GnomeSettingsModuleRunlevel gnome_settings_module_get_runlevel (GnomeSettingsModule *module);
GConfClient                *gnome_settings_module_get_config_client (GnomeSettingsModule *module);
GnomeSettingsModuleStatus   gnome_settings_module_get_status (GnomeSettingsModule *module);

G_END_DECLS

#endif
