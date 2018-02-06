/*                                                                              
 * Copyright (C) 2017 jsparber
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.         
 *                                                                              
 * Author: Julian Sparber <julian@sparber.net>
 *                                                                              
 */   

#include <gio/gio.h>
#include "cc-background-store.h"
#include "cc-background-item.h"
#include "cc-background-xml.h"

struct _CcBackgroundStore
{
  GObject parent_instance;

  GListStore *model;
  CcBackgroundXml *xml;
};

G_DEFINE_TYPE (CcBackgroundStore, cc_background_store, G_TYPE_OBJECT)

static void
cc_background_store_dispose (GObject *gobject)
{
  CcBackgroundStore *self = CC_BACKGROUND_STORE (gobject);

  g_clear_object (&self->model);
  g_clear_object (&self->xml);

  /* Always chain up to the parent class; there is no need to check if
   * the parent class implements the dispose() virtual function: it is
   * always guaranteed to do so
   */
  G_OBJECT_CLASS (cc_background_store_parent_class)->dispose (gobject);
}

static void
cc_background_store_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (cc_background_store_parent_class)->finalize (gobject);
}

static void
item_added (CcBackgroundXml    *xml,
            CcBackgroundItem   *item,
            CcBackgroundStore  *self)
{
  g_list_store_append (self->model, item);
}

static void
list_load_cb (GObject           *source_object,
              GAsyncResult      *res,
              gpointer           user_data)
{
  cc_background_xml_load_list_finish (res);
}

static void
cc_background_store_class_init (CcBackgroundStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_background_store_dispose;
  object_class->finalize = cc_background_store_finalize;
}

static void
cc_background_store_init (CcBackgroundStore *self)
{
  self->model = g_list_store_new (cc_background_item_get_type());
  self->xml = cc_background_xml_new ();

  g_signal_connect (G_OBJECT (self->xml), "added",
                    G_CALLBACK (item_added), self);

  /* Try adding the default background first */
  //load_default_bg (self);

  cc_background_xml_load_list_async (self->xml, NULL, list_load_cb, self);
}

void 
cc_background_store_bind_flow_box (CcBackgroundStore            *self,
                                   gpointer                      panel,
                                   GtkWidget                    *widget,
                                   GtkFlowBoxCreateWidgetFunc    create_widget_fun)
{
  gtk_flow_box_bind_model (GTK_FLOW_BOX (widget),
                           G_LIST_MODEL (self->model),
                           create_widget_fun,
                           panel,
                           NULL);
}

CcBackgroundStore *
cc_background_store_new ()
{
  return g_object_new (CC_TYPE_BACKGROUND_STORE, NULL);
}
