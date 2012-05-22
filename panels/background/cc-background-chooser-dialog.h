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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CC_PANEL_CHOOSER_DIALOG_H__
#define __CC_PANEL_CHOOSER_DIALOG_H__

#include <gtk/gtk.h>

#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_CHOOSER_DIALOG            (cc_background_chooser_dialog_get_type ())
#define CC_BACKGROUND_CHOOSER_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), CC_TYPE_BACKGROUND_CHOOSER_DIALOG, CcBackgroundChooserDialog))
#define CC_BACKGROUND_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_BACKGROUND_CHOOSER_DIALOG, CcBackgroundChooserDialogClass))
#define CC_IS_BACKGROUND_CHOOSER_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), CC_TYPE_BACKGROUND_CHOOSER_DIALOG))
#define CC_IS_BACKGROUND_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_BACKGROUND_CHOOSER_DIALOG))
#define CC_BACKGROUND_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_BACKGROUND_CHOOSER_DIALOG, CcBackgroundChooserDialogClass))

typedef struct _CcBackgroundChooserDialog        CcBackgroundChooserDialog;
typedef struct _CcBackgroundChooserDialogClass   CcBackgroundChooserDialogClass;
typedef struct _CcBackgroundChooserDialogPrivate CcBackgroundChooserDialogPrivate;

struct _CcBackgroundChooserDialog
{
  GtkDialog parent_instance;
  CcBackgroundChooserDialogPrivate *priv;
};

struct _CcBackgroundChooserDialogClass
{
  GtkDialogClass parent_class;
};

GType                  cc_background_chooser_dialog_get_type               (void) G_GNUC_CONST;
GtkWidget *            cc_background_chooser_dialog_new                    (void);

CcBackgroundItem *     cc_background_chooser_dialog_get_item               (CcBackgroundChooserDialog *chooser);

G_END_DECLS

#endif /* __CC_BACKGROUND_CHOOSER_DIALOG_H__ */
