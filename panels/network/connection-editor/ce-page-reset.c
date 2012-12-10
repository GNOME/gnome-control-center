/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include "net-connection-editor.h"
#include "ce-page-reset.h"

G_DEFINE_TYPE (CEPageReset, ce_page_reset, CE_TYPE_PAGE)


static void
forget_cb (GtkButton *button, CEPageReset *page)
{
        net_connection_editor_forget (page->editor);
}

static void
reset_cb (GtkButton *button, CEPageReset *page)
{
        net_connection_editor_reset (page->editor);
}

static void
connect_reset_page (CEPageReset *page)
{
        GtkWidget *widget;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "button_forget"));
        g_signal_connect (widget, "clicked", G_CALLBACK (forget_cb), page);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "button_reset"));
        g_signal_connect (widget, "clicked", G_CALLBACK (reset_cb), page);
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        return TRUE;
}

static void
ce_page_reset_init (CEPageReset *page)
{
}

static void
ce_page_reset_class_init (CEPageResetClass *class)
{
        CEPageClass *page_class= CE_PAGE_CLASS (class);

        page_class->validate = validate;
}

CEPage *
ce_page_reset_new (NMConnection        *connection,
                   NMClient            *client,
                   NMRemoteSettings    *settings,
                   NetConnectionEditor *editor)
{
        CEPageReset *page;

        page = CE_PAGE_RESET (ce_page_new (CE_TYPE_PAGE_RESET,
                                           connection,
                                           client,
                                           settings,
                                           "/org/gnome/control-center/network/reset-page.ui",
                                           _("Reset")));
        page->editor = editor;

        connect_reset_page (page);

        return CE_PAGE (page);
}
