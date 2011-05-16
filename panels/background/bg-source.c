/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "bg-source.h"
#include "cc-background-item.h"

G_DEFINE_ABSTRACT_TYPE (BgSource, bg_source, G_TYPE_OBJECT)

#define SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_SOURCE, BgSourcePrivate))

struct _BgSourcePrivate
{
  GtkListStore *store;
};

enum
{
  PROP_LISTSTORE = 1
};


static void
bg_source_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BgSource *source = BG_SOURCE (object);

  switch (property_id)
    {
    case PROP_LISTSTORE:
      g_value_set_object (value, bg_source_get_liststore (source));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bg_source_set_property (GObject      *object,
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
bg_source_dispose (GObject *object)
{
  BgSourcePrivate *priv = BG_SOURCE (object)->priv;

  if (priv->store)
    {
      g_object_unref (priv->store);
      priv->store = NULL;
    }

  G_OBJECT_CLASS (bg_source_parent_class)->dispose (object);
}

static void
bg_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (bg_source_parent_class)->finalize (object);
}

static void
bg_source_class_init (BgSourceClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgSourcePrivate));

  object_class->get_property = bg_source_get_property;
  object_class->set_property = bg_source_set_property;
  object_class->dispose = bg_source_dispose;
  object_class->finalize = bg_source_finalize;

  pspec = g_param_spec_object ("liststore",
                               "Liststore",
                               "Liststore used in the source",
                               GTK_TYPE_LIST_STORE,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LISTSTORE, pspec);
}

static void
bg_source_init (BgSource *self)
{
  BgSourcePrivate *priv;

  priv = self->priv = SOURCE_PRIVATE (self);

  priv->store = gtk_list_store_new (3, G_TYPE_ICON, G_TYPE_OBJECT, G_TYPE_STRING);
}

GtkListStore*
bg_source_get_liststore (BgSource *source)
{
  g_return_val_if_fail (BG_IS_SOURCE (source), NULL);

  return source->priv->store;
}
