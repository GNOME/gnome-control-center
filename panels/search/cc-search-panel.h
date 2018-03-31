/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef _CC_SEARCH_PANEL_H
#define _CC_SEARCH_PANEL_H

#include <cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_SEARCH_PANEL cc_search_panel_get_type()

#define CC_SEARCH_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SEARCH_PANEL, CcSearchPanel))

#define CC_SEARCH_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SEARCH_PANEL, CcSearchPanelClass))

#define CC_IS_SEARCH_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SEARCH_PANEL))

#define CC_IS_SEARCH_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SEARCH_PANEL))

#define CC_SEARCH_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SEARCH_PANEL, CcSearchPanelClass))

typedef struct _CcSearchPanel CcSearchPanel;
typedef struct _CcSearchPanelClass CcSearchPanelClass;
typedef struct _CcSearchPanelPrivate CcSearchPanelPrivate;

struct _CcSearchPanel
{
  CcPanel parent;

  CcSearchPanelPrivate *priv;
};

struct _CcSearchPanelClass
{
  CcPanelClass parent_class;
};

GType cc_search_panel_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CC_SEARCH_PANEL_H */
