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

#ifndef _UM_EDITABLE_ENTRY_H_
#define _UM_EDITABLE_ENTRY_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_EDITABLE_ENTRY  um_editable_entry_get_type()

#define UM_EDITABLE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_EDITABLE_ENTRY, UmEditableEntry))
#define UM_EDITABLE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_EDITABLE_ENTRY, UmEditableEntryClass))
#define UM_IS_EDITABLE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_EDITABLE_ENTRY))
#define UM_IS_EDITABLE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_EDITABLE_ENTRY))
#define UM_EDITABLE_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_EDITABLE_ENTRY, UmEditableEntryClass))

typedef struct _UmEditableEntry UmEditableEntry;
typedef struct _UmEditableEntryClass UmEditableEntryClass;
typedef struct _UmEditableEntryPrivate UmEditableEntryPrivate;

struct _UmEditableEntry
{
  GtkAlignment parent;

  UmEditableEntryPrivate *priv;
};

struct _UmEditableEntryClass
{
  GtkAlignmentClass parent_class;

  void (* editing_done) (UmEditableEntry *entry);
};

GType        um_editable_entry_get_type     (void) G_GNUC_CONST;
GtkWidget   *um_editable_entry_new          (void);
void         um_editable_entry_set_text     (UmEditableEntry *entry,
                                             const gchar     *text);
const gchar *um_editable_entry_get_text     (UmEditableEntry *entry);
void         um_editable_entry_set_editable (UmEditableEntry *entry,
                                             gboolean         editable);
gboolean     um_editable_entry_get_editable (UmEditableEntry *entry);
void         um_editable_entry_set_weight   (UmEditableEntry *entry,
                                             gint             weight);
gint         um_editable_entry_get_weight   (UmEditableEntry *entry);
void         um_editable_entry_set_scale    (UmEditableEntry *entry,
                                             gdouble          scale);
gdouble      um_editable_entry_get_scale    (UmEditableEntry *entry);

G_END_DECLS

#endif /* _UM_EDITABLE_ENTRY_H_ */
