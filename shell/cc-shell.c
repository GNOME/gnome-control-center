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

G_DEFINE_TYPE (CcShell, cc_shell, GTK_TYPE_BUILDER)

#define SHELL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SHELL, CcShellPrivate))

struct _CcShellPrivate
{
  gpointer *dummy;
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
cc_shell_init (CcShell *self)
{
  self->priv = SHELL_PRIVATE (self);
}

CcShell *
cc_shell_new (void)
{
  return g_object_new (CC_TYPE_SHELL, NULL);
}
