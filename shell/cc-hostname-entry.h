/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Intel, Inc
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

#ifndef __CC_HOSTNAME_ENTRY_H__
#define __CC_HOSTNAME_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_HOSTNAME_ENTRY cc_hostname_entry_get_type()

#define CC_HOSTNAME_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_HOSTNAME_ENTRY, CcHostnameEntry))

#define CC_HOSTNAME_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_HOSTNAME_ENTRY, CcHostnameEntryClass))

#define CC_IS_HOSTNAME_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_HOSTNAME_ENTRY))

#define CC_IS_HOSTNAME_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_HOSTNAME_ENTRY))

#define CC_HOSTNAME_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_HOSTNAME_ENTRY, CcHostnameEntryClass))

typedef struct _CcHostnameEntry CcHostnameEntry;
typedef struct _CcHostnameEntryClass CcHostnameEntryClass;
typedef struct _CcHostnameEntryPrivate CcHostnameEntryPrivate;

struct _CcHostnameEntry
{
  GtkEntry parent;

  CcHostnameEntryPrivate *priv;
};

struct _CcHostnameEntryClass
{
  GtkEntryClass parent_class;
};

GType cc_hostname_entry_get_type (void) G_GNUC_CONST;

CcHostnameEntry *cc_hostname_entry_new (void);
gchar* cc_hostname_entry_get_hostname (CcHostnameEntry *entry);

G_END_DECLS

#endif /* __CC_HOSTNAME_ENTRY_H__ */
