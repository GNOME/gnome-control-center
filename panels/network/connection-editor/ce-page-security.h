/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more security.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __CE_PAGE_SECURITY_H
#define __CE_PAGE_SECURITY_H

#include <glib-object.h>

#include <gtk/gtk.h>
#include "ce-page.h"

G_BEGIN_DECLS

#define CE_TYPE_PAGE_SECURITY          (ce_page_security_get_type ())
#define CE_PAGE_SECURITY(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), CE_TYPE_PAGE_SECURITY, CEPageSecurity))
#define CE_PAGE_SECURITY_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), CE_TYPE_PAGE_SECURITY, CEPageSecurityClass))
#define CE_IS_PAGE_SECURITY(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), CE_TYPE_PAGE_SECURITY))
#define CE_IS_PAGE_SECURITY_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), CE_TYPE_PAGE_SECURITY))
#define CE_PAGE_SECURITY_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), CE_TYPE_PAGE_SECURITY, CEPageSecurityClass))

typedef struct _CEPageSecurity          CEPageSecurity;
typedef struct _CEPageSecurityClass     CEPageSecurityClass;

struct _CEPageSecurity
{
        CEPage parent;

        GtkComboBox *security_combo;
        GtkWidget   *security_heading;
        GtkSizeGroup *group;
        gboolean     adhoc;
};

struct _CEPageSecurityClass
{
        CEPageClass parent_class;
};

GType   ce_page_security_get_type (void);

CEPage *ce_page_security_new      (NMConnection     *connection,
                                   NMClient         *client);

G_END_DECLS

#endif /* __CE_PAGE_SECURITY_H */

