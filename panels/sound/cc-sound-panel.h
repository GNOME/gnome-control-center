/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _CC_SOUND_PANEL_H
#define _CC_SOUND_PANEL_H

#include <shell/cc-panel.h>
#include "gvc-mixer-control.h"
#include "gvc-mixer-dialog.h"

G_BEGIN_DECLS

#define CC_TYPE_SOUND_PANEL cc_sound_panel_get_type()
#define CC_SOUND_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_SOUND_PANEL, CcSoundPanel))
#define CC_SOUND_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_SOUND_PANEL, CcSoundPanelClass))
#define CC_IS_SOUND_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_SOUND_PANEL))
#define CC_IS_SOUND_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_SOUND_PANEL))
#define CC_SOUND_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_SOUND_PANEL, CcSoundPanelClass))

typedef struct _CcSoundPanel CcSoundPanel;
typedef struct _CcSoundPanelClass CcSoundPanelClass;
typedef struct _CcSoundPanelPrivate CcSoundPanelPrivate;

struct _CcSoundPanel {
	CcPanel parent;

	GvcMixerControl *control;
	GvcMixerDialog  *dialog;
	GtkWidget       *connecting_label;
};

struct _CcSoundPanelClass {
	CcPanelClass parent_class;
};

GType cc_sound_panel_get_type (void) G_GNUC_CONST;

void  cc_sound_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _CC_SOUND_PANEL_H */

