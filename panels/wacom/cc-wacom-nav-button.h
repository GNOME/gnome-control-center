/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Bastien Nocera <hadess@hadess.net>
 */


#ifndef _CC_WACOM_NAV_BUTTON_H
#define _CC_WACOM_NAV_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_WACOM_NAV_BUTTON cc_wacom_nav_button_get_type()

#define CC_WACOM_NAV_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_WACOM_NAV_BUTTON, CcWacomNavButton))

#define CC_WACOM_NAV_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_WACOM_NAV_BUTTON, CcWacomNavButtonClass))

#define CC_IS_WACOM_NAV_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_WACOM_NAV_BUTTON))

#define CC_IS_WACOM_NAV_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_WACOM_NAV_BUTTON))

#define CC_WACOM_NAV_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_WACOM_NAV_BUTTON, CcWacomNavButtonClass))

typedef struct _CcWacomNavButton CcWacomNavButton;
typedef struct _CcWacomNavButtonClass CcWacomNavButtonClass;
typedef struct _CcWacomNavButtonPrivate CcWacomNavButtonPrivate;

struct _CcWacomNavButton
{
  GtkBox parent;

  CcWacomNavButtonPrivate *priv;
};

struct _CcWacomNavButtonClass
{
  GtkBoxClass parent_class;
};

GType cc_wacom_nav_button_get_type (void) G_GNUC_CONST;

GtkWidget * cc_wacom_nav_button_new (void);

G_END_DECLS

#endif /* _CC_WACOM_NAV_BUTTON_H */
