/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 */


#ifndef _CC_NETWORK_PANEL_H
#define _CC_NETWORK_PANEL_H

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_NETWORK_PANEL cc_network_panel_get_type()

#define CC_NETWORK_PANEL(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CC_TYPE_NETWORK_PANEL, CcNetworkPanel))

#define CC_NETWORK_PANEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        CC_TYPE_NETWORK_PANEL, CcNetworkPanelClass))

#define CC_IS_NETWORK_PANEL(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        CC_TYPE_NETWORK_PANEL))

#define CC_IS_NETWORK_PANEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), \
        CC_TYPE_NETWORK_PANEL))

#define CC_NETWORK_PANEL_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        CC_TYPE_NETWORK_PANEL, CcNetworkPanelClass))

typedef struct _CcNetworkPanel CcNetworkPanel;
typedef struct _CcNetworkPanelClass CcNetworkPanelClass;
typedef struct _CcNetworkPanelPrivate CcNetworkPanelPrivate;

struct _CcNetworkPanel
{
        CcPanel parent;
        CcNetworkPanelPrivate *priv;
};

struct _CcNetworkPanelClass
{
        CcPanelClass parent_class;
};

GType cc_network_panel_get_type (void) G_GNUC_CONST;

void  cc_network_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _CC_NETWORK_PANEL_H */
