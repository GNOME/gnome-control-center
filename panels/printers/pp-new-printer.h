/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#ifndef __PP_NEW_PRINTER_H__
#define __PP_NEW_PRINTER_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PP_TYPE_NEW_PRINTER         (pp_new_printer_get_type ())
#define PP_NEW_PRINTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PP_TYPE_NEW_PRINTER, PpNewPrinter))
#define PP_NEW_PRINTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PP_TYPE_NEW_PRINTER, PpNewPrinterClass))
#define PP_IS_NEW_PRINTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PP_TYPE_NEW_PRINTER))
#define PP_IS_NEW_PRINTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PP_TYPE_NEW_PRINTER))
#define PP_NEW_PRINTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PP_TYPE_NEW_PRINTER, PpNewPrinterClass))

typedef struct _PpNewPrinter        PpNewPrinter;
typedef struct _PpNewPrinterClass   PpNewPrinterClass;
typedef struct _PpNewPrinterPrivate PpNewPrinterPrivate;

struct _PpNewPrinter
{
  GObject              parent_instance;
  PpNewPrinterPrivate *priv;
};

struct _PpNewPrinterClass
{
  GObjectClass parent_class;
};

GType         pp_new_printer_get_type   (void) G_GNUC_CONST;
PpNewPrinter *pp_new_printer_new        (void);

void          pp_new_printer_add_async  (PpNewPrinter         *host,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);

gboolean      pp_new_printer_add_finish (PpNewPrinter         *host,
                                         GAsyncResult         *result,
                                         GError              **error);

G_END_DECLS

#endif /* __PP_NEW_PRINTER_H__ */
