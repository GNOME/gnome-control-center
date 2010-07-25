/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Jonathan Blandford <jrb@gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _CC_FONTS_PANEL_H
#define _CC_FONTS_PANEL_H

#include <libgnome-control-center/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_FONTS_PANEL cc_fonts_panel_get_type()

#define CC_FONTS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_FONTS_PANEL, CcFontsPanel))

#define CC_FONTS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_FONTS_PANEL, CcFontsPanelClass))

#define CC_IS_FONTS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_FONTS_PANEL))

#define CC_IS_FONTS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_FONTS_PANEL))

#define CC_FONTS_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_FONTS_PANEL, CcFontsPanelClass))

typedef struct _CcFontsPanel CcFontsPanel;
typedef struct _CcFontsPanelClass CcFontsPanelClass;
typedef struct _CcFontsPanelPrivate CcFontsPanelPrivate;

struct _CcFontsPanel
{
  CcPanel parent;

  CcFontsPanelPrivate *priv;
};

struct _CcFontsPanelClass
{
  CcPanelClass parent_class;
};

GType cc_fonts_panel_get_type (void) G_GNUC_CONST;

void  cc_fonts_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _CC_FONTS_PANEL_H */
