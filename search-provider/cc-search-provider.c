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

#include <config.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <string.h>

#include <shell/cc-panel.h>
#include <shell/cc-shell-model.h>
#include <shell/cc-panel-loader.h>

#include "cc-util.h"

#include "control-center-search-provider.h"
#include "cc-search-provider.h"

struct _CcSearchProvider
{
  GObject parent;

  CcShellSearchProvider2 *skeleton;
};

struct _CcSearchProviderClass
{
  GObjectClass parent_class;
};

typedef enum {
  MATCH_NONE,
  MATCH_PREFIX,
  MATCH_SUBSTRING
} PanelSearchMatch;

G_DEFINE_TYPE (CcSearchProvider, cc_search_provider, G_TYPE_OBJECT)

static char **
get_casefolded_terms (char **terms)
{
  char **casefolded_terms;
  int i, n;

  n = g_strv_length ((char**) terms);
  casefolded_terms = g_new (char*, n + 1);

  for (i = 0; i < n; i++)
    casefolded_terms[i] = cc_util_normalize_casefold_and_unaccent (terms[i]);
  casefolded_terms[n] = NULL;

  return casefolded_terms;
}

static gboolean
matches_multiple_terms (GtkTreeModel  *model,
                        GtkTreeIter   *iter,
                        char         **terms)
{
  int i;

  for (i = 0; terms[i]; i++)
    {
      if (cc_shell_model_iter_matches_search (CC_SHELL_MODEL (model),
                                              iter,
                                              terms[i]))
        return TRUE;
    }

  return FALSE;
}

static GtkTreeModel *
get_model (void)
{
  CcSearchProviderApp *app;

  app = cc_search_provider_app_get ();
  return GTK_TREE_MODEL (cc_search_provider_app_get_model (app));
}

static gboolean
handle_get_initial_result_set (CcShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation   *invocation,
                               char                   **terms,
                               CcSearchProvider        *self)
{
  GtkTreeModel *model = get_model ();
  GtkTreeIter iter;
  GPtrArray *results;
  gboolean ok;
  char **casefolded_terms;
  char **results_as_strings;

  casefolded_terms = get_casefolded_terms (terms);
  results = g_ptr_array_new ();

  ok = gtk_tree_model_get_iter_first (model, &iter);
  while (ok)
    {
      if (matches_multiple_terms (model, &iter,
                                  casefolded_terms))
        {
          g_ptr_array_add (results,
                           gtk_tree_model_get_string_from_iter (model,
                                                                &iter));
        }

      ok = gtk_tree_model_iter_next (model, &iter);
    }

  g_ptr_array_add (results, NULL);
  results_as_strings = (char**) g_ptr_array_free (results, FALSE);
  cc_shell_search_provider2_complete_get_initial_result_set (skeleton,
                                                             invocation,
                                                             (const char* const*) results_as_strings);

  g_strfreev (casefolded_terms);
  g_strfreev (results_as_strings);
  return TRUE;
}

static gboolean
handle_get_subsearch_result_set (CcShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation   *invocation,
                                 char                   **previous_results,
                                 char                   **terms,
                                 CcSearchProvider        *self)
{
  GtkTreeModel *model = get_model ();
  GtkTreeIter iter;
  GPtrArray *results;
  char **casefolded_terms;
  char **results_as_strings;
  int i;

  casefolded_terms = get_casefolded_terms (terms);
  results = g_ptr_array_new ();

  for (i = 0; previous_results[i]; i++)
    {
      if (gtk_tree_model_get_iter_from_string (model, &iter,
                                               previous_results[i]) &&
          matches_multiple_terms (model, &iter, casefolded_terms))
        {
          g_ptr_array_add (results, g_strdup (previous_results[i]));
        }
    }

  g_ptr_array_add (results, NULL);
  results_as_strings = (char**) g_ptr_array_free (results, FALSE);
  cc_shell_search_provider2_complete_get_subsearch_result_set (skeleton,
                                                               invocation,
                                                               (const char* const*) results_as_strings);

  g_strfreev (casefolded_terms);
  g_strfreev (results_as_strings);
  return TRUE;
}

static gboolean
handle_get_result_metas (CcShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation   *invocation,
                         char                   **results,
                         CcSearchProvider        *self)
{
  GtkTreeModel *model = get_model ();
  GtkTreeIter iter;
  int i;
  GVariantBuilder builder;
  GAppInfo *app;
  const char *id;
  char *name, *description, *escaped_description, *icon_string;
  GIcon *icon;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; results[i]; i++)
    {
      if (!gtk_tree_model_get_iter_from_string (model, &iter, results[i]))
        continue;

      gtk_tree_model_get (model, &iter,
                          COL_APP, &app,
                          COL_NAME, &name,
                          COL_GICON, &icon,
                          COL_DESCRIPTION, &description,
                          -1);
      id = g_app_info_get_id (app);
      icon_string = g_icon_to_string (icon);
      escaped_description = g_markup_escape_text (description, -1);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}",
                             "id", g_variant_new_string (id));
      g_variant_builder_add (&builder, "{sv}",
                             "name", g_variant_new_string (name));
      g_variant_builder_add (&builder, "{sv}",
                             "gicon", g_variant_new_string (icon_string));
      g_variant_builder_add (&builder, "{sv}",
                             "description", g_variant_new_string (escaped_description));
      g_variant_builder_close (&builder);

      g_free (name);
      g_free (description);
      g_free (escaped_description);
      g_free (icon_string);
      g_object_unref (app);
      g_object_unref (icon);
    }

  cc_shell_search_provider2_complete_get_result_metas (skeleton,
                                                       invocation,
                                                       g_variant_builder_end (&builder));
  return TRUE;
}

static gboolean
handle_activate_result (CcShellSearchProvider2  *skeleton,
                        GDBusMethodInvocation   *invocation,
                        char                    *identifier,
                        char                   **results,
                        guint                    timestamp,
                        CcSearchProvider        *self)
{
  GdkAppLaunchContext *launch_context;
  GAppInfo *app;
  GError *error;

  launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
  gdk_app_launch_context_set_timestamp (launch_context, timestamp);

  app = G_APP_INFO (g_desktop_app_info_new (identifier));

  error = NULL;
  if (!g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (launch_context), &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    }
  else
    {
      cc_shell_search_provider2_complete_activate_result (skeleton, invocation);
    }
  return TRUE;
}

static gboolean
handle_launch_search (CcShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation   *invocation,
                      char                   **terms,
                      guint                    timestamp,
                      CcSearchProvider        *self)
{
  GdkAppLaunchContext *launch_context;
  char *joined_terms, *command_line;
  GAppInfo *app;
  GError *error;

  launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
  gdk_app_launch_context_set_timestamp (launch_context, timestamp);

  joined_terms = g_strjoinv (" ", terms);
  command_line = g_strdup_printf ("gnome-control-center -s '%s'", joined_terms);

  error = NULL;
  app = g_app_info_create_from_commandline (command_line,
                                            "gnome-control-center.desktop",
                                            G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                            &error);
  if (!app) {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
      return TRUE;
  }

  if (!g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (launch_context), &error))
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    }
  else
    {
      cc_shell_search_provider2_complete_launch_search (skeleton, invocation);
    }
  return TRUE;
}

static void
cc_search_provider_init (CcSearchProvider *self)
{
  self->skeleton = cc_shell_search_provider2_skeleton_new ();

  g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);
  g_signal_connect (self->skeleton, "handle-launch-search",
                    G_CALLBACK (handle_launch_search), self);
}

gboolean
cc_search_provider_dbus_register (CcSearchProvider  *self,
                                  GDBusConnection   *connection,
                                  const gchar       *object_path,
                                  GError           **error)
{
  GDBusInterfaceSkeleton *skeleton;

  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);

  return g_dbus_interface_skeleton_export (skeleton, connection, object_path, error);
}

void
cc_search_provider_dbus_unregister (CcSearchProvider *self,
                                    GDBusConnection  *connection,
                                    const gchar      *object_path)
{
  GDBusInterfaceSkeleton *skeleton;

  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);

  if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
      g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);
}

static void
cc_search_provider_dispose (GObject *object)
{
  CcSearchProvider *self;

  self = CC_SEARCH_PROVIDER (object);

  g_clear_object (&self->skeleton);

  G_OBJECT_CLASS (cc_search_provider_parent_class)->dispose (object);
}

static void
cc_search_provider_class_init (CcSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_search_provider_dispose;
}

CcSearchProvider *
cc_search_provider_new (void)
{
  return g_object_new (CC_TYPE_SEARCH_PROVIDER, NULL);
}

