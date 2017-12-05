/* cc-background-grid-item.c
 *
 * Copyright (C) 2017 Julian Sparber <julian@sparber.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include "cc-background-grid-item.h"
#include "cc-background-item.h"

struct _CcBackgroundGridItem
{
  GtkFlowBoxChild            parent;

  GtkImage                  *image;

  /* data */
  CcBackgroundItem      *item;

};


G_DEFINE_TYPE (CcBackgroundGridItem, cc_background_grid_item, GTK_TYPE_FLOW_BOX_CHILD)

    enum {
      PROP_0,
      PROP_ITEM
    };

GtkWidget*
cc_background_grid_item_new (CcBackgroundItem *item)
{

  return g_object_new (CC_TYPE_BACKGROUND_GRID_ITEM,
                       "item", item,
                       NULL);
}

CcBackgroundItem * cc_background_grid_item_get_ref (CcBackgroundGridItem *self)
{
  return self->item;
}
void
cc_background_grid_item_set_ref (CcBackgroundGridItem *self, CcBackgroundItem *item)
{
  self->item = item;
}

static void
cc_background_grid_item_finalize (GObject *object)
{
  //CcBackgroundGridItem *self = CC_BACKGROUND_GRID_ITEM (object);

  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->finalize (object);

}

static void
cc_background_grid_item_dispose (GObject *object)
{
  //CcBackgroundGridItem *self = CC_BACKGROUND_GRID_ITEM (object);

  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->dispose (object);
}

static void
cc_background_grid_item_set_property (GObject *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) object;
  switch (prop_id)
    {

    case PROP_ITEM:
      self->item = g_value_dup_object (value);
      g_debug ("Every set %p -> %p", value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_grid_item_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) object;

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
cc_background_grid_item_class_init (CcBackgroundGridItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_background_grid_item_finalize;
  object_class->dispose = cc_background_grid_item_dispose;
  object_class->get_property = cc_background_grid_item_get_property;
  object_class->set_property = cc_background_grid_item_set_property;

  /*g_object_class_override_property (object_class,
                                    PROP_ITEM,
                                    "item");
                                    */
  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "Background item reference",
                                                        "The reference to this background item",
                                                        CC_TYPE_BACKGROUND_ITEM,
                                                        G_PARAM_READWRITE));

}

static void
cc_background_grid_item_init (CcBackgroundGridItem *self)
{
  g_debug ("Item ref: %p", self->item);
  //gtk_widget_init_template (GTK_WIDGET (self));
}
