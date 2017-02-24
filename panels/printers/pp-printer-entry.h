/*
 * Copyright 2017 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#ifndef PP_PRINTER_ENTRY_H
#define PP_PRINTER_ENTRY_H

#include <gtk/gtk.h>
#include <cups/cups.h>

#define PP_PRINTER_ENTRY_TYPE (pp_printer_entry_get_type ())
#define PP_PRINTER_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PP_PRINTER_ENTRY_TYPE, PpPrinterEntry))

typedef struct _PpPrinterEntry      PpPrinterEntry;
typedef struct _PpPrinterEntryClass PpPrinterEntryClass;

GType       pp_printer_entry_get_type (void);

PpPrinterEntry *pp_printer_entry_new  (cups_dest_t printer,
                                       gboolean    is_authorized);

void            pp_printer_entry_update_jobs_count (PpPrinterEntry *self);

#endif /* PP_PRINTER_ENTRY_H */
