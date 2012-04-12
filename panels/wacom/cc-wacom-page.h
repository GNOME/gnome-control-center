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
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 */


#ifndef _CC_WACOM_PAGE_H
#define _CC_WACOM_PAGE_H

#include <gtk/gtk.h>
#include "cc-wacom-panel.h"
#include "gsd-wacom-device.h"

G_BEGIN_DECLS

#define CC_TYPE_WACOM_PAGE cc_wacom_page_get_type()

#define CC_WACOM_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_WACOM_PAGE, CcWacomPage))

#define CC_WACOM_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_WACOM_PAGE, CcWacomPageClass))

#define CC_IS_WACOM_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_WACOM_PAGE))

#define CC_IS_WACOM_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_WACOM_PAGE))

#define CC_WACOM_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_WACOM_PAGE, CcWacomPageClass))

typedef struct _CcWacomPage CcWacomPage;
typedef struct _CcWacomPageClass CcWacomPageClass;
typedef struct _CcWacomPagePrivate CcWacomPagePrivate;

struct _CcWacomPage
{
  GtkBox parent;

  CcWacomPagePrivate *priv;
};

struct _CcWacomPageClass
{
  GtkBoxClass parent_class;
};

GType cc_wacom_page_get_type (void) G_GNUC_CONST;

GtkWidget * cc_wacom_page_new (CcWacomPanel   *panel,
			       GsdWacomDevice *stylus,
			       GsdWacomDevice *eraser,
			       GsdWacomDevice *pad);

gboolean cc_wacom_page_update_tools (CcWacomPage    *page,
				     GsdWacomDevice *stylus,
				     GsdWacomDevice *eraser,
				     GsdWacomDevice *pad);

void cc_wacom_page_set_navigation (CcWacomPage *page,
				   GtkNotebook *notebook,
				   gboolean     ignore_first_page);

G_END_DECLS

#endif /* _CC_WACOM_PAGE_H */
