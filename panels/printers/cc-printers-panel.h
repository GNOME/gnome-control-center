/*
 * Copyright (C) 2010 Red Hat, Inc
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
 */


#ifndef _CC_PRINTERS_PANEL_H
#define _CC_PRINTERS_PANEL_H

#include <cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_PRINTERS_PANEL cc_printers_panel_get_type()

#define CC_PRINTERS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_PRINTERS_PANEL, CcPrintersPanel))

#define CC_PRINTERS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_PRINTERS_PANEL, CcPrintersPanelClass))

#define CC_IS_PRINTERS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_PRINTERS_PANEL))

#define CC_IS_PRINTERS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_PRINTERS_PANEL))

#define CC_PRINTERS_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_PRINTERS_PANEL, CcPrintersPanelClass))

typedef struct _CcPrintersPanel CcPrintersPanel;
typedef struct _CcPrintersPanelClass CcPrintersPanelClass;
typedef struct _CcPrintersPanelPrivate CcPrintersPanelPrivate;

struct _CcPrintersPanel
{
  CcPanel parent;

  CcPrintersPanelPrivate *priv;
};

struct _CcPrintersPanelClass
{
  CcPanelClass parent_class;
};

GType cc_printers_panel_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CC_PRINTERS_PANEL_H */
