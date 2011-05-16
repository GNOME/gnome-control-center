/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef _UM_EDITABLE_BUTTON_H
#define _UM_EDITABLE_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_EDITABLE_BUTTON  um_editable_button_get_type()

#define UM_EDITABLE_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_EDITABLE_BUTTON, UmEditableButton))
#define UM_EDITABLE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_EDITABLE_BUTTON, UmEditableButtonClass))
#define UM_IS_EDITABLE_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_EDITABLE_BUTTON))
#define UM_IS_EDITABLE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_EDITABLE_BUTTON))
#define UM_EDITABLE_BUTTON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_EDITABLE_BUTTON, UmEditableButtonClass))

typedef struct _UmEditableButton UmEditableButton;
typedef struct _UmEditableButtonClass UmEditableButtonClass;
typedef struct _UmEditableButtonPrivate UmEditableButtonPrivate;

struct _UmEditableButton
{
  GtkAlignment parent;

  UmEditableButtonPrivate *priv;
};

struct _UmEditableButtonClass
{
  GtkAlignmentClass parent_class;

  void (* start_editing) (UmEditableButton *button);
  void (* activate)      (UmEditableButton *button);
};

GType        um_editable_button_get_type     (void) G_GNUC_CONST;
GtkWidget   *um_editable_button_new          (void);
void         um_editable_button_set_text     (UmEditableButton *button,
                                              const gchar      *text);
const gchar *um_editable_button_get_text     (UmEditableButton *button);
void         um_editable_button_set_editable (UmEditableButton *button,
                                              gboolean          editable);
gboolean     um_editable_button_get_editable (UmEditableButton *button);
void         um_editable_button_set_weight   (UmEditableButton *button,
                                              gint              weight);
gint         um_editable_button_get_weight   (UmEditableButton *button);
void         um_editable_button_set_scale    (UmEditableButton *button,
                                              gdouble           scale);
gdouble      um_editable_button_get_scale    (UmEditableButton *button);

G_END_DECLS

#endif /* _UM_EDITABLE_BUTTON_H_ */
