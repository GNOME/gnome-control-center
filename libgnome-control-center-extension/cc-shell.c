/*
 * Copyright (c) 2010 Intel, Inc.
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
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "cc-shell.h"
#include "cc-panel.h"

G_DEFINE_TYPE (CcShell, cc_shell, GTK_TYPE_BUILDER)

#define SHELL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SHELL, CcShellPrivate))

struct _CcShellPrivate
{
  gchar *current_title;
  CcPanel *current_panel;
  GHashTable *panels;
};


static void
cc_shell_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_shell_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_shell_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_shell_parent_class)->dispose (object);
}

static void
cc_shell_finalize (GObject *object)
{
  CcShellPrivate *priv = CC_SHELL (object)->priv;

  if (priv->panels)
    {
      g_hash_table_destroy (priv->panels);
      priv->panels = NULL;
    }

  G_OBJECT_CLASS (cc_shell_parent_class)->finalize (object);
}

static GObject*
cc_shell_constructor (GType                  type,
                      guint                  n_construct_properties,
                      GObjectConstructParam *construct_properties)
{
  GError *err = NULL;
  GObject *object;

  object =
    G_OBJECT_CLASS (cc_shell_parent_class)->constructor (type,
                                                         n_construct_properties,
                                                         construct_properties);

  gtk_builder_add_from_file (GTK_BUILDER (object),
                             UIDIR "/shell.ui",
                             &err);

  if (err)
    {
      g_warning ("Could not load UI file: %s", err->message);

      g_error_free (err);
      g_object_unref (object);

      return NULL;
    }

  return object;
}

static void
cc_shell_class_init (CcShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcShellPrivate));

  object_class->get_property = cc_shell_get_property;
  object_class->set_property = cc_shell_set_property;
  object_class->dispose = cc_shell_dispose;
  object_class->finalize = cc_shell_finalize;
  object_class->constructor = cc_shell_constructor;
}


static void
load_panel_plugins (CcShell *shell)
{
  CcShellPrivate *priv = shell->priv;
  static volatile GType panel_type = G_TYPE_INVALID;
  static GIOExtensionPoint *ep = NULL;
  GList *modules;
  GList *panel_implementations;
  GList *l;
  GTimer *timer;

  /* make sure base type is registered */
  if (panel_type == G_TYPE_INVALID)
    {
      panel_type = g_type_from_name ("CcPanel");
    }

  priv->panels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        g_object_unref);

  if (ep == NULL)
    {
      g_debug ("Registering extension point");
      ep = g_io_extension_point_register (CC_PANEL_EXTENSION_POINT_NAME);
    }

  /* load all modules */
  g_debug ("Loading all modules in %s", EXTENSIONSDIR);
  modules = g_io_modules_load_all_in_directory (EXTENSIONSDIR);

  g_debug ("Loaded %d modules", g_list_length (modules));

#ifdef RUN_IN_SOURCE_TREE
  if (g_list_length (modules) == 0)
    modules = load_panel_plugins_from_source ();
#endif

  /* find all extensions */
  timer = g_timer_new ();
  panel_implementations = g_io_extension_point_get_extensions (ep);
  for (l = panel_implementations; l != NULL; l = l->next)
    {
      GIOExtension *extension;
      CcPanel *panel;
      char *id;

      extension = l->data;

      g_debug ("Found extension: %s %d", g_io_extension_get_name (extension),
               g_io_extension_get_priority (extension));
      panel = g_object_new (g_io_extension_get_type (extension),
                            "shell", shell,
                            NULL);
      g_object_get (panel, "id", &id, NULL);
      g_hash_table_insert (priv->panels, g_strdup (id), g_object_ref (panel));
      g_debug ("id: '%s', loaded in %fsec", id, g_timer_elapsed (timer, NULL));
      g_free (id);
      g_timer_start (timer);
    }
  g_timer_destroy (timer);
  timer = NULL;

  /* unload all modules; the module our instantiated authority is in won't be unloaded because
   * we've instantiated a reference to a type in this module
   */
  g_list_foreach (modules, (GFunc) g_type_module_unuse, NULL);
  g_list_free (modules);
}

static void
cc_shell_init (CcShell *self)
{
  self->priv = SHELL_PRIVATE (self);

  load_panel_plugins (self);
}

CcShell *
cc_shell_new (void)
{
  return g_object_new (CC_TYPE_SHELL, NULL);
}

gboolean
cc_shell_set_panel (CcShell     *shell,
                    const gchar *id)
{
  CcPanel *panel;
  CcShellPrivate *priv = shell->priv;
  GtkBuilder *builder = GTK_BUILDER (shell);
  GtkWidget *notebook;

  notebook =
    (GtkWidget*) gtk_builder_get_object (GTK_BUILDER (shell), "notebook");


  if (!id)
    {
      if (priv->current_panel != NULL)
        cc_panel_set_active (priv->current_panel, FALSE);

      priv->current_panel = NULL;

      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                     OVERVIEW_PAGE);

      return TRUE;
    }

  /* first look for a panel module */
  panel = g_hash_table_lookup (priv->panels, id);
  if (panel != NULL)
    {
      priv->current_panel = panel;
      gtk_container_set_border_width (GTK_CONTAINER (panel), 12);
      gtk_widget_show_all (GTK_WIDGET (panel));
      cc_panel_set_active (panel, TRUE);

      gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
                                GTK_WIDGET (panel), NULL, CAPPLET_PAGE);

      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                     CAPPLET_PAGE);

      gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder,
                                                             "label-title")),
                          priv->current_title);

      gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (builder,
                                                           "title-alignment")));
      return TRUE;
    }
  else
    {
      return FALSE;
    }

}

void
cc_shell_set_title (CcShell     *shell,
                    const gchar *title)
{
  CcShellPrivate *priv = shell->priv;

  g_free (priv->current_title);
  priv->current_title = g_strdup (title);
}
