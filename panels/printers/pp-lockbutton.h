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

#ifndef __PP_LOCK_BUTTON_H__
#define __PP_LOCK_BUTTON_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PP_TYPE_LOCK_BUTTON         (pp_lock_button_get_type ())
#define PP_LOCK_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PP_TYPE_LOCK_BUTTON, PpLockButton))
#define PP_LOCK_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), PP_LOCK_BUTTON,  PpLockButtonClass))
#define PP_IS_LOCK_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PP_TYPE_LOCK_BUTTON))
#define PP_IS_LOCK_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PP_TYPE_LOCK_BUTTON))
#define PP_LOCK_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PP_TYPE_LOCK_BUTTON, PpLockButtonClass))

typedef struct _PpLockButton        PpLockButton;
typedef struct _PpLockButtonClass   PpLockButtonClass;
typedef struct _PpLockButtonPrivate PpLockButtonPrivate;

struct _PpLockButton
{
  GtkBin parent;

  PpLockButtonPrivate *priv;
};

struct _PpLockButtonClass
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

GType        pp_lock_button_get_type       (void) G_GNUC_CONST;
GtkWidget   *pp_lock_button_new            (GPermission   *permission);
GPermission *pp_lock_button_get_permission (PpLockButton *button);


G_END_DECLS

#endif  /* __PP_LOCK_BUTTON_H__ */
