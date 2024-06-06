/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gxdp.h>
#include <stdlib.h>

#include "control-center-global-shortcuts-provider.h"
#include <shell/cc-panel-loader.h>
#include <shell/cc-shell-model.h>

struct _CcGlobalShortcutsProviderApp
{
  AdwApplication parent;

  CcGlobalShortcutsProvider *global_shortcuts_provider;
};

G_DEFINE_TYPE (CcGlobalShortcutsProviderApp,
               cc_global_shortcuts_provider_app,
               ADW_TYPE_APPLICATION)

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in millisends */

static gboolean
cc_global_shortcuts_provider_app_dbus_register (GApplication     *application,
                                                GDBusConnection  *connection,
                                                const char       *object_path,
                                                GError          **error)
{
  CcGlobalShortcutsProviderApp *self =
    CC_GLOBAL_SHORTCUTS_PROVIDER_APP (application);
  GApplicationClass *parent_class =
    G_APPLICATION_CLASS (cc_global_shortcuts_provider_app_parent_class);

  if (!parent_class->dbus_register (application,
                                    connection,
                                    object_path,
                                    error))
    return FALSE;


  return cc_global_shortcuts_provider_dbus_register (self->global_shortcuts_provider,
                                                     connection,
                                                     object_path, error);
}

static void
cc_global_shortcuts_provider_app_dbus_unregister (GApplication    *application,
                                                  GDBusConnection *connection,
                                                  const char      *object_path)
{
  CcGlobalShortcutsProviderApp *self =
    CC_GLOBAL_SHORTCUTS_PROVIDER_APP (application);
  GApplicationClass *parent_class =
    G_APPLICATION_CLASS (cc_global_shortcuts_provider_app_parent_class);

  if (self->global_shortcuts_provider)
    {
      cc_global_shortcuts_provider_dbus_unregister (self->global_shortcuts_provider,
                                                   connection,
                                                   object_path);
    }

  parent_class->dbus_unregister (application, connection, object_path);
}

static void
cc_global_shortcuts_provider_app_dispose (GObject *object)
{
  CcGlobalShortcutsProviderApp *self =
    CC_GLOBAL_SHORTCUTS_PROVIDER_APP (object);

  g_clear_object (&self->global_shortcuts_provider);

  G_OBJECT_CLASS (cc_global_shortcuts_provider_app_parent_class)->dispose (object);
}

static void
cc_global_shortcuts_provider_app_init (CcGlobalShortcutsProviderApp *self)
{
  self->global_shortcuts_provider =
    cc_global_shortcuts_provider_new (GTK_APPLICATION (self));

  g_application_set_inactivity_timeout (G_APPLICATION (self),
                                        INACTIVITY_TIMEOUT);

  /* HACK: get the updated inactivity timeout started */
  g_application_hold (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}

static void
cc_global_shortcuts_provider_app_startup (GApplication *application)
{
  G_APPLICATION_CLASS (cc_global_shortcuts_provider_app_parent_class)->startup (application);
}

static void
cc_global_shortcuts_provider_app_class_init (CcGlobalShortcutsProviderAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = cc_global_shortcuts_provider_app_dispose;

  app_class->dbus_register = cc_global_shortcuts_provider_app_dbus_register;
  app_class->dbus_unregister = cc_global_shortcuts_provider_app_dbus_unregister;
  app_class->startup = cc_global_shortcuts_provider_app_startup;
}

CcGlobalShortcutsProviderApp *
cc_global_shortcuts_provider_app_get ()
{
  static CcGlobalShortcutsProviderApp *singleton;

  if (singleton)
    return singleton;

  singleton = g_object_new (CC_TYPE_GLOBAL_SHORTCUTS_PROVIDER_APP,
                            "application-id", "org.gnome.Settings.GlobalShortcutsProvider",
                            "flags", G_APPLICATION_IS_SERVICE,
                            NULL);

  return singleton;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  g_autoptr (GError) error = NULL;

  if (!gxdp_init_gtk (GXDP_SERVICE_CLIENT_TYPE_GLOBAL_SHORTCUTS,
                      &error))
    {
      g_warning ("Failed to initialize windowing system connection: %s",
                 error->message);
      return EXIT_FAILURE;
    }

  app = G_APPLICATION (cc_global_shortcuts_provider_app_get ());
  return g_application_run (app, argc, argv);
}
