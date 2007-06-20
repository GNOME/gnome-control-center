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

#include "gnome-settings-module.h"

#define CLASS(module) (GNOME_SETTINGS_MODULE_CLASS (G_OBJECT_GET_CLASS (module)))

struct _GnomeSettingsModulePrivate {
	GnomeSettingsModuleStatus status;
	GConfClient *config_client;
};

static GObjectClass *parent_class = NULL;

static void
gnome_settings_module_finalize (GObject *object)
{
	GnomeSettingsModule *module = GNOME_SETTINGS_MODULE (object);

	g_return_if_fail (GNOME_SETTINGS_IS_MODULE (module));

	if (module->priv) {
		if (module->priv->config_client) {
			g_object_unref (G_OBJECT (module->priv->config_client));
			module->priv->config_client = NULL;
		}

		g_free (module->priv);
		module->priv = NULL;
	}

	if (parent_class->finalize)
		parent_class->finalize (object);
}

static void
gnome_settings_module_init (GnomeSettingsModule *module, GnomeSettingsModuleClass *klass)
{
	module->priv = g_new0 (GnomeSettingsModulePrivate, 1);
	module->priv->status = GNOME_SETTINGS_MODULE_STATUS_NONE;
}

static void
gnome_settings_module_class_init (GnomeSettingsModuleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gnome_settings_module_finalize;

	klass->initialize = NULL;
	klass->start = NULL;
	klass->stop = NULL;
	klass->reload_settings = NULL;
}

GType
gnome_settings_module_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModule),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_init,
		};
      
		module_type = g_type_register_static (G_TYPE_OBJECT,
						      "GnomeSettingsModule",
						      &module_info, 0);
	}
  
	return module_type;
}

GnomeSettingsModuleRunlevel
gnome_settings_module_get_runlevel (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), GNOME_SETTINGS_MODULE_RUNLEVEL_NONE);
	g_return_val_if_fail (CLASS (module)->get_runlevel != NULL, GNOME_SETTINGS_MODULE_RUNLEVEL_NONE);

	return CLASS (module)->get_runlevel (module);
}

gboolean
gnome_settings_module_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), FALSE);
	g_return_val_if_fail (module->priv->status == GNOME_SETTINGS_MODULE_STATUS_NONE, FALSE);
	g_return_val_if_fail (CLASS (module)->initialize != NULL, FALSE);

	/* associate GConfClient with this module */
	if (module->priv->config_client)
		g_object_unref (module->priv->config_client);

	module->priv->config_client = g_object_ref (config_client);

	if (CLASS (module)->initialize (module, config_client)) {
		module->priv->status = GNOME_SETTINGS_MODULE_STATUS_INITIALIZED;
		return TRUE;
	}

	return FALSE;
}

gboolean
gnome_settings_module_start (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), FALSE);
	g_return_val_if_fail (module->priv->status == GNOME_SETTINGS_MODULE_STATUS_INITIALIZED, FALSE);

	if (!CLASS (module)->start)
		return TRUE;

	if (CLASS (module)->start (module)) {
		module->priv->status = GNOME_SETTINGS_MODULE_STATUS_STARTED;
		return TRUE;
	}

	return FALSE;
}

gboolean
gnome_settings_module_stop (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), FALSE);
	g_return_val_if_fail (module->priv->status == GNOME_SETTINGS_MODULE_STATUS_STARTED, FALSE);
	
	if (!CLASS (module)->stop)
		return TRUE;

	if (CLASS (module)->stop (module)) {
		module->priv->status = GNOME_SETTINGS_MODULE_STATUS_STOPPED;
		return TRUE;
	}

	return FALSE;
}

gboolean
gnome_settings_module_reload_settings (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), FALSE);
	g_return_val_if_fail (CLASS (module)->reload_settings != NULL, FALSE);

	return CLASS (module)->reload_settings (module);
}

GConfClient *
gnome_settings_module_get_config_client (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), NULL);

	return module->priv->config_client;
}

GnomeSettingsModuleStatus
gnome_settings_module_get_status (GnomeSettingsModule *module)
{
	g_return_val_if_fail (GNOME_SETTINGS_IS_MODULE (module), GNOME_SETTINGS_MODULE_STATUS_NONE);

	return module->priv->status;
}
