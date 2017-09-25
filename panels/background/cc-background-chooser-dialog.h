/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __CC_PANEL_CHOOSER_DIALOG_H__
#define __CC_PANEL_CHOOSER_DIALOG_H__

#include <gtk/gtk.h>

#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_CHOOSER_DIALOG (cc_background_chooser_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundChooserDialog, cc_background_chooser_dialog, CC, BACKGROUND_CHOOSER_DIALOG, GtkDialog)

GtkWidget *            cc_background_chooser_dialog_new                    (GtkWindow *transient_for);

CcBackgroundItem *     cc_background_chooser_dialog_get_item               (CcBackgroundChooserDialog *chooser);

G_END_DECLS

#endif /* __CC_BACKGROUND_CHOOSER_DIALOG_H__ */
