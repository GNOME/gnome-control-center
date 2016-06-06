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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 */


#ifndef _CC_WACOM_STYLUS_PAGE_H
#define _CC_WACOM_STYLUS_PAGE_H

#include <gtk/gtk.h>
#include "cc-wacom-tool.h"

G_BEGIN_DECLS

#define CC_TYPE_WACOM_STYLUS_PAGE cc_wacom_stylus_page_get_type()

#define CC_WACOM_STYLUS_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_WACOM_STYLUS_PAGE, CcWacomStylusPage))

#define CC_WACOM_STYLUS_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_WACOM_STYLUS_PAGE, CcWacomStylusPageClass))

#define CC_IS_WACOM_STYLUS_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_WACOM_STYLUS_PAGE))

#define CC_IS_WACOM_STYLUS_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_WACOM_STYLUS_PAGE))

#define CC_WACOM_STYLUS_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_WACOM_STYLUS_PAGE, CcWacomStylusPageClass))

typedef struct _CcWacomStylusPage CcWacomStylusPage;
typedef struct _CcWacomStylusPageClass CcWacomStylusPageClass;
typedef struct _CcWacomStylusPagePrivate CcWacomStylusPagePrivate;

struct _CcWacomStylusPage
{
  GtkBox parent;

  CcWacomStylusPagePrivate *priv;
};

struct _CcWacomStylusPageClass
{
  GtkBoxClass parent_class;
};

GType cc_wacom_stylus_page_get_type (void) G_GNUC_CONST;

GtkWidget * cc_wacom_stylus_page_new (CcWacomTool *stylus);

CcWacomTool * cc_wacom_stylus_page_get_tool (CcWacomStylusPage *page);

void cc_wacom_stylus_page_set_navigation (CcWacomStylusPage *page,
					  GtkNotebook *notebook);

G_END_DECLS

#endif /* _CC_WACOM_STYLUS_PAGE_H */
