/*
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CC_LOCK_BUTTON_H__
#define __CC_LOCK_BUTTON_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define UM_TYPE_LOCK_BUTTON         (cc_lock_button_get_type ())
#define CC_LOCK_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UM_TYPE_LOCK_BUTTON, CcLockButton))
#define CC_LOCK_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), CC_LOCK_BUTTON,  CcLockButtonClass))
#define UM_IS_LOCK_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UM_TYPE_LOCK_BUTTON))
#define UM_IS_LOCK_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), UM_TYPE_LOCK_BUTTON))
#define CC_LOCK_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), UM_TYPE_LOCK_BUTTON, CcLockButtonClass))

typedef struct _CcLockButton        CcLockButton;
typedef struct _CcLockButtonClass   CcLockButtonClass;
typedef struct _CcLockButtonPrivate CcLockButtonPrivate;

struct _CcLockButton
{
  GtkBin parent;

  CcLockButtonPrivate *priv;
};

struct _CcLockButtonClass
{
  GtkBinClass parent_class;

  void (*reserved0) (void);
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
  void (*reserved6) (void);
  void (*reserved7) (void);
};

GType        cc_lock_button_get_type       (void) G_GNUC_CONST;
GtkWidget   *cc_lock_button_new            (GPermission   *permission);
GPermission *cc_lock_button_get_permission (CcLockButton *button);


G_END_DECLS

#endif  /* __CC_LOCK_BUTTON_H__ */
