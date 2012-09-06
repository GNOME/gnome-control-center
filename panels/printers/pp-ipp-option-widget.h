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

#ifndef __PP_IPP_OPTION_WIDGET_H__
#define __PP_IPP_OPTION_WIDGET_H__

#include <gtk/gtk.h>
#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_IPP_OPTION_WIDGET                  (pp_ipp_option_widget_get_type ())
#define PP_IPP_OPTION_WIDGET(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PP_TYPE_IPP_OPTION_WIDGET, PpIPPOptionWidget))
#define PP_IPP_OPTION_WIDGET_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass),  PP_TYPE_IPP_OPTION_WIDGET, PpIPPOptionWidgetClass))
#define PP_IS_IPP_OPTION_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PP_TYPE_IPP_OPTION_WIDGET))
#define PP_IS_IPP_OPTION_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass),  PP_TYPE_IPP_OPTION_WIDGET))
#define PP_IPP_OPTION_WIDGET_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj),  PP_TYPE_IPP_OPTION_WIDGET, PpIPPOptionWidgetClass))

typedef struct _PpIPPOptionWidget         PpIPPOptionWidget;
typedef struct _PpIPPOptionWidgetClass    PpIPPOptionWidgetClass;
typedef struct PpIPPOptionWidgetPrivate   PpIPPOptionWidgetPrivate;

struct _PpIPPOptionWidget
{
  GtkHBox parent_instance;

  PpIPPOptionWidgetPrivate *priv;
};

struct _PpIPPOptionWidgetClass
{
  GtkHBoxClass parent_class;

  void (*changed) (PpIPPOptionWidget *widget);
};

typedef void (*IPPOptionCallback) (GtkWidget *widget,
                                   gpointer   user_data);

GType	     pp_ipp_option_widget_get_type  (void) G_GNUC_CONST;

GtkWidget   *pp_ipp_option_widget_new (IPPAttribute *attr_supported,
                                       IPPAttribute *attr_default,
                                       const gchar  *option_name,
                                       const gchar  *printer);

G_END_DECLS

#endif /* __PP_IPP_OPTION_WIDGET_H__ */
