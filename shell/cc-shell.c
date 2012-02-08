/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

/**
 * SECTION:cc-shell
 * @short_description: Abstract class representing the Control Center shell
 *
 * CcShell is an abstract class that represents an instance of a control
 * center shell. It provides access to some of the properties of the shell
 * that panels will need to read or change. When a panel is created it has an
 * instance of CcShell available that represents the current shell.
 */


#include "cc-shell.h"
#include "cc-panel.h"

G_DEFINE_ABSTRACT_TYPE (CcShell, cc_shell, G_TYPE_OBJECT)

#define SHELL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SHELL, CcShellPrivate))

struct _CcShellPrivate
{
  CcPanel *active_panel;
};

enum
{
  PROP_ACTIVE_PANEL = 1
};


static void
cc_shell_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  CcShell *shell = CC_SHELL (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      g_value_set_object (value, shell->priv->active_panel);
      break;

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
  CcShell *shell = CC_SHELL (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      cc_shell_set_active_panel (shell, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_shell_dispose (GObject *object)
{
  /* remove and unref the active shell */
  cc_shell_set_active_panel (CC_SHELL (object), NULL);

  G_OBJECT_CLASS (cc_shell_parent_class)->dispose (object);
}

static void
cc_shell_class_init (CcShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (CcShellPrivate));

  object_class->get_property = cc_shell_get_property;
  object_class->set_property = cc_shell_set_property;
  object_class->dispose = cc_shell_dispose;

  pspec = g_param_spec_object ("active-panel",
                               "active panel",
                               "The currently active Panel",
                               CC_TYPE_PANEL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACTIVE_PANEL, pspec);
}

static void
cc_shell_init (CcShell *self)
{
  self->priv = SHELL_PRIVATE (self);
}

/**
 * cc_shell_get_active_panel:
 * @shell: A #CcShell
 *
 * Get the current active panel
 *
 * Returns: a #CcPanel or NULL if no panel is active
 */
CcPanel*
cc_shell_get_active_panel (CcShell *shell)
{
  g_return_val_if_fail (CC_IS_SHELL (shell), NULL);

  return shell->priv->active_panel;
}

/**
 * cc_shell_set_active_panel:
 * @shell: A #CcShell
 * @panel: A #CcPanel
 *
 * Set the current active panel. If @panel is NULL, then the shell is returned
 * to a state where no panel is being displayed (for example, the list of panels
 * may be shown instead).
 *
 */
void
cc_shell_set_active_panel (CcShell *shell,
                           CcPanel *panel)
{
  g_return_if_fail (CC_IS_SHELL (shell));
  g_return_if_fail (panel == NULL || CC_IS_PANEL (panel));

  if (panel != shell->priv->active_panel)
    {
      /* remove the old panel */
      g_object_unref (shell->priv->active_panel);
      shell->priv->active_panel = NULL;

      /* set the new panel */
      if (panel)
        {
          shell->priv->active_panel = g_object_ref (panel);
          g_object_set (G_OBJECT (panel), "shell", shell, NULL);
        }
      g_object_notify (G_OBJECT (shell), "active-panel");
    }
}

/**
 * cc_shell_set_active_panel_from_id:
 * @shell: A #CcShell
 * @id: the ID of the panel to set as active
 * @error: A #GError
 *
 * Find a panel corresponding to the specified id and set it as active.
 *
 * Returns: #TRUE if the panel was found and set as the active panel
 */
gboolean
cc_shell_set_active_panel_from_id (CcShell      *shell,
                                   const gchar  *id,
                                   const gchar **argv,
                                   GError      **error)
{
  CcShellClass *class;

  g_return_val_if_fail (CC_IS_SHELL (shell), FALSE);


  class = (CcShellClass *) G_OBJECT_GET_CLASS (shell);

  if (!class->set_active_panel_from_id)
    {
      g_warning ("Object of type \"%s\" does not implement required virtual"
                 " function \"set_active_panel_from_id\",",
                 G_OBJECT_TYPE_NAME (shell));
      return FALSE;
    }
  else
    {
      return class->set_active_panel_from_id (shell, id, argv, error);
    }
}

/**
 * cc_shell_get_toplevel:
 * @shell: A #CcShell
 *
 * Gets the toplevel window of the shell.
 *
 * Returns: The #GtkWidget of the shell window, or #NULL on error.
 */
GtkWidget *
cc_shell_get_toplevel (CcShell *shell)
{
  CcShellClass *klass;

  g_return_val_if_fail (CC_IS_SHELL (shell), NULL);

  klass = CC_SHELL_GET_CLASS (shell);

  if (klass->get_toplevel)
    {
        return klass->get_toplevel (shell);
    }

  g_warning ("Object of type \"%s\" does not implement required virtual"
             " function \"get_toplevel\",",
             G_OBJECT_TYPE_NAME (shell));

  return NULL;
}

void
cc_shell_embed_widget_in_header (CcShell *shell, GtkWidget *widget)
{
  CcShellClass *class;

  g_return_if_fail (CC_IS_SHELL (shell));

  class = (CcShellClass *) G_OBJECT_GET_CLASS (shell);

  if (!class->embed_widget_in_header)
    {
      g_warning ("Object of type \"%s\" does not implement required virtual"
                 " function \"embed_widget_in_header\",",
                 G_OBJECT_TYPE_NAME (shell));
    }
  else
    {
      class->embed_widget_in_header (shell, widget);
    }
}
