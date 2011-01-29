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

#ifndef __DT_LOCK_BUTTON_H__
#define __DT_LOCK_BUTTON_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define DT_TYPE_LOCK_BUTTON         (dt_lock_button_get_type ())
#define DT_LOCK_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DT_TYPE_LOCK_BUTTON, DtLockButton))
#define DT_LOCK_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), DT_LOCK_BUTTON,  DtLockButtonClass))
#define DT_IS_LOCK_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DT_TYPE_LOCK_BUTTON))
#define DT_IS_LOCK_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DT_TYPE_LOCK_BUTTON))
#define DT_LOCK_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DT_TYPE_LOCK_BUTTON, DtLockButtonClass))

typedef struct _DtLockButton        DtLockButton;
typedef struct _DtLockButtonClass   DtLockButtonClass;
typedef struct _DtLockButtonPrivate DtLockButtonPrivate;

struct _DtLockButton
{
  GtkBin parent;

  DtLockButtonPrivate *priv;
};

struct _DtLockButtonClass
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

GType        dt_lock_button_get_type       (void) G_GNUC_CONST;
GtkWidget   *dt_lock_button_new            (GPermission   *permission);
GPermission *dt_lock_button_get_permission (DtLockButton *button);


G_END_DECLS

#endif  /* __DT_LOCK_BUTTON_H__ */
