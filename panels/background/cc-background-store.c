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

struct _CcBackgroundStore
{
  GObject parent_instance;

  /* Other members, including private data. */
};

/* Private structure definition. */
typedef struct _CcBackgroundStorePrivate CcBackgroundStorePrivate;

struct _CcBackgroundStorePrivate
{
  GListStore * model;
};

G_DEFINE_TYPE_WITH_PRIVATE (CcBackgroundStore, cc_background_store, G_TYPE_OBJECT)


static void
cc_background_store_dispose (GObject *gobject)
{
  CcBackgroundStorePrivate *priv = cc_background_store_get_instance_private (CC_BACKGROUND_STORE (gobject));

  /* In dispose(), you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */

  /* dispose() might be called multiple times, so we must guard against
   * calling g_object_unref() on an invalid GObject by setting the member
   * NULL; g_clear_object() does this for us.
   */
  g_clear_object (&priv->model);

  /* Always chain up to the parent class; there is no need to check if
   * the parent class implements the dispose() virtual function: it is
   * always guaranteed to do so
   */
  G_OBJECT_CLASS (cc_background_store_parent_class)->dispose (gobject);
}

static void
cc_background_store_finalize (GObject *gobject)
{
  //CcBackgroundStorePrivate *priv = cc_background_store_get_instance_private (CC_BACKGROUND_STORE (gobject));

  //g_free (priv->filename);

  /* Always chain up to the parent class; as with dispose(), finalize()
   * is guaranteed to exist on the parent's class virtual function table
   */
  G_OBJECT_CLASS (cc_background_store_parent_class)->finalize (gobject);
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
  CcBackgroundStorePrivate *priv = cc_background_store_get_instance_private (self);
  priv->model = g_list_store_new (cc_background_item_get_type());
}
