/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _CC_BACKGROUND_XML_H_
#define _CC_BACKGROUND_XML_H_

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_XML         (cc_background_xml_get_type ())
#define CC_BACKGROUND_XML(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_BACKGROUND_XML, CcBackgroundXml))
#define CC_BACKGROUND_XML_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_BACKGROUND_XML, CcBackgroundXmlClass))
#define CC_IS_BACKGROUND_XML(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_BACKGROUND_XML))
#define CC_IS_BACKGROUND_XML_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_BACKGROUND_XML))
#define CC_BACKGROUND_XML_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_BACKGROUND_XML, CcBackgroundXmlClass))

typedef struct CcBackgroundXmlPrivate CcBackgroundXmlPrivate;

typedef struct
{
  GObject parent;
  CcBackgroundXmlPrivate *priv;
} CcBackgroundXml;

typedef struct
{
  GObjectClass parent_class;
  void (*added) (CcBackgroundXml *xml, GObject *item);
} CcBackgroundXmlClass;

GType              cc_background_xml_get_type (void);

CcBackgroundXml *cc_background_xml_new (void);

void cc_background_xml_save                          (CcBackgroundItem *item,
						      const char       *filename);

CcBackgroundItem *cc_background_xml_get_item         (const char      *filename);
gboolean cc_background_xml_load_xml                  (CcBackgroundXml *data,
						      const char      *filename);
void cc_background_xml_load_list_async               (CcBackgroundXml *data,
						      GCancellable *cancellable,
						      GAsyncReadyCallback callback,
						      gpointer user_data);
const GHashTable *cc_background_xml_load_list_finish (GAsyncResult  *async_result);

G_END_DECLS

#endif

