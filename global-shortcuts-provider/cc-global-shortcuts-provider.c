/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 * Copyright © 2024 GNOME Foundation Inc.
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
 * Contributor: Dorota Czaplejewicz
 */

#include <config.h>

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "cc-keyboard-manager.h"

#include <shell/cc-panel-loader.h>
#include <shell/cc-panel.h>
#include <shell/cc-shell-model.h>

#include "cc-global-shortcut-dialog.h"
#include "cc-global-shortcuts-provider.h"
#include "cc-util.h"
#include "control-center-global-shortcuts-provider.h"

struct _CcGlobalShortcutsProvider
{
  GObject parent;

  CcSettingsGlobalShortcutsProvider *skeleton;

  GtkApplication *app;

  GHashTable *dialogs;
};

typedef enum
{
  MATCH_NONE,
  MATCH_PREFIX,
  MATCH_SUBSTRING
} PanelSearchMatch;

G_DEFINE_TYPE (CcGlobalShortcutsProvider,
               cc_global_shortcuts_provider,
               G_TYPE_OBJECT)

static void
handle_dialog_done (CcGlobalShortcutsProvider *self,
                    CcGlobalShortcutDialog    *shortcut_dialog,
                    GVariant                  *response)
{
  GDBusMethodInvocation *invocation;

  invocation = g_hash_table_lookup (self->dialogs, shortcut_dialog);

  if (response)
    {
      cc_settings_global_shortcuts_provider_complete_bind_shortcuts (self->skeleton,
                                                                     invocation,
                                                                     g_variant_ref_sink (response));
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Access denied");
    }
}

static void
on_dialog_done (CcGlobalShortcutDialog    *shortcut_dialog,
                GVariant                  *response,
                CcGlobalShortcutsProvider *self)
{
  handle_dialog_done (self, shortcut_dialog, response);
  g_hash_table_remove (self->dialogs, shortcut_dialog);
}

static gboolean
handle_bind_shortcuts (CcGlobalShortcutsProvider *self,
                       GDBusMethodInvocation     *invocation,
                       const char                *app_id,
                       const char                *parent_window,
                       GVariant                  *shortcuts)
{
  g_autoptr(CcGlobalShortcutDialog) shortcut_dialog = NULL;

  if (!g_application_id_is_valid (app_id))
    {
      g_warning ("Discarded shortcut bind request from application with an invalid app_id >%s<.", app_id);
      return G_DBUS_METHOD_INVOCATION_UNHANDLED;
    }

  shortcut_dialog =
    cc_global_shortcut_dialog_new (app_id,
                                   parent_window,
                                   shortcuts);

  g_signal_connect (shortcut_dialog, "done",
                    G_CALLBACK (on_dialog_done),
                    self);

  gtk_application_add_window (self->app, GTK_WINDOW (shortcut_dialog));

  g_hash_table_insert (self->dialogs,
                       g_object_ref (shortcut_dialog),
                       g_object_ref (invocation));

  cc_global_shortcut_dialog_present (shortcut_dialog);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
cc_global_shortcuts_provider_init (CcGlobalShortcutsProvider *self)
{
  self->dialogs = g_hash_table_new_full (NULL, NULL,
                                         (GDestroyNotify) gtk_window_destroy,
                                         (GDestroyNotify) g_object_unref);
  self->skeleton = cc_settings_global_shortcuts_provider_skeleton_new ();

  g_signal_connect_swapped (self->skeleton, "handle-bind-shortcuts",
                            G_CALLBACK (handle_bind_shortcuts), self);
}

gboolean
cc_global_shortcuts_provider_dbus_register (CcGlobalShortcutsProvider  *self,
                                            GDBusConnection            *connection,
                                            const char                 *object_path,
                                            GError                    **error)
{
  GDBusInterfaceSkeleton *skeleton =
    G_DBUS_INTERFACE_SKELETON (self->skeleton);

  return g_dbus_interface_skeleton_export (skeleton, connection, object_path, error);
}

void
cc_global_shortcuts_provider_dbus_unregister (CcGlobalShortcutsProvider *self,
                                              GDBusConnection           *connection,
                                              const char                *object_path)
{
  GDBusInterfaceSkeleton *skeleton =
    G_DBUS_INTERFACE_SKELETON (self->skeleton);

  if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
    g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);
}

static void
cc_global_shortcuts_provider_dispose (GObject *object)
{
  CcGlobalShortcutsProvider *self =
    CC_GLOBAL_SHORTCUTS_PROVIDER (object);

  g_clear_object (&self->skeleton);
  g_clear_pointer (&self->dialogs, g_hash_table_unref);

  G_OBJECT_CLASS (cc_global_shortcuts_provider_parent_class)->dispose (object);
}

static void
cc_global_shortcuts_provider_class_init (CcGlobalShortcutsProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_global_shortcuts_provider_dispose;
}

CcGlobalShortcutsProvider *
cc_global_shortcuts_provider_new (GtkApplication *app)
{
  CcGlobalShortcutsProvider *self;

  self = g_object_new (CC_TYPE_GLOBAL_SHORTCUTS_PROVIDER, NULL);
  self->app = app;

  return self;
}
