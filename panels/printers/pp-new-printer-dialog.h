/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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

#ifndef __PP_NEW_PRINTER_DIALOG_H__
#define __PP_NEW_PRINTER_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PP_TYPE_NEW_PRINTER_DIALOG            (pp_new_printer_dialog_get_type ())
#define PP_NEW_PRINTER_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), PP_TYPE_NEW_PRINTER_DIALOG, PpNewPrinterDialog))
#define PP_NEW_PRINTER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PP_TYPE_NEW_PRINTER_DIALOG, PpNewPrinterDialogClass))
#define PP_IS_NEW_PRINTER_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), PP_TYPE_NEW_PRINTER_DIALOG))
#define PP_IS_NEW_PRINTER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PP_TYPE_NEW_PRINTER_DIALOG))
#define PP_NEW_PRINTER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PP_TYPE_NEW_PRINTER_DIALOG, PpNewPrinterDialogClass))

typedef struct _PpNewPrinterDialog        PpNewPrinterDialog;
typedef struct _PpNewPrinterDialogClass   PpNewPrinterDialogClass;
typedef struct _PpNewPrinterDialogPrivate PpNewPrinterDialogPrivate;

struct _PpNewPrinterDialog
{
  GObject                    parent_instance;
  PpNewPrinterDialogPrivate *priv;
};

struct _PpNewPrinterDialogClass
{
  GObjectClass parent_class;

  void (*pre_response)  (PpNewPrinterDialog *dialog,
                         const gchar        *device_name,
                         const gchar        *device_location,
                         const gchar        *device_make_and_model,
                         gboolean            is_network_device);

  void (*response)      (PpNewPrinterDialog *dialog,
                         gint                response_id);
};

GType               pp_new_printer_dialog_get_type (void) G_GNUC_CONST;
PpNewPrinterDialog *pp_new_printer_dialog_new      (GtkWindow *parent);

G_END_DECLS

#endif /* __PP_NEW_PRINTER_DIALOG_H__ */
