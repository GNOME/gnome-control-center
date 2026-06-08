/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2014 Red Hat, Inc.
 */

#include "ui-helpers.h"
#include "config.h"

#include <NetworkManager.h>
#include <glib/gi18n.h>

void
widget_set_error (GtkWidget *widget)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));

    gtk_widget_add_css_class (widget, "error");
    gtk_accessible_update_state (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                 -1);
    if (GTK_IS_ENTRY (widget)) {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY, "dialog-error-symbolic");
        gtk_entry_set_icon_tooltip_text (GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY, _("Entry value is invalid"));
    }
}

void
widget_unset_error (GtkWidget *widget)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));

    gtk_widget_remove_css_class (widget, "error");
    gtk_accessible_reset_state (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_STATE_INVALID);
    if (GTK_IS_ENTRY (widget))
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (widget), GTK_ENTRY_ICON_SECONDARY, NULL);
}

gboolean
dns_entry_valid (GtkEntry *dns_entry, int family)
{
    g_auto(GStrv) dns_addresses = NULL;
    g_autofree gchar *dns_text = NULL;
    int i;

    dns_text = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (dns_entry))));

    if (dns_text[0] == '\0')
        return TRUE;

    dns_addresses = g_strsplit_set (dns_text, ", ", -1);

    for (i = 0; dns_addresses && dns_addresses[i]; i++) {
        const gchar *text = dns_addresses[i];

        if (!text || !*text)
            continue;

        if (!nm_utils_ipaddr_valid (family, text))
            return FALSE;
    }

    return TRUE;
}
