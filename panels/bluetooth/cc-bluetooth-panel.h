/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2010  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _CC_BLUETOOTH_PANEL_H
#define _CC_BLUETOOTH_PANEL_H

#include <shell/cc-shell.h>

G_BEGIN_DECLS

#define CC_TYPE_BLUETOOTH_PANEL cc_bluetooth_panel_get_type()
#define CC_BLUETOOTH_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanel))
#define CC_BLUETOOTH_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanelClass))
#define CC_IS_BLUETOOTH_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_BLUETOOTH_PANEL))
#define CC_IS_BLUETOOTH_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_BLUETOOTH_PANEL))
#define CC_BLUETOOTH_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanelClass))

typedef struct CcBluetoothPanel CcBluetoothPanel;
typedef struct CcBluetoothPanelClass CcBluetoothPanelClass;
typedef struct CcBluetoothPanelPrivate CcBluetoothPanelPrivate;

struct CcBluetoothPanel {
	CcPanel parent;
	CcBluetoothPanelPrivate *priv;
};

struct CcBluetoothPanelClass {
	CcPanelClass parent_class;
};

GType cc_bluetooth_panel_get_type (void) G_GNUC_CONST;

void  cc_bluetooth_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _CC_BLUETOOTH_PANEL_H */

