/* -*- mode: C; c-basic-offset: 4 -*-
 * themus - utilities for GNOME themes
 * Copyright (C) 2000, 2001 Eazel Inc.
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* libmain.c - object activation infrastructure for shared library
   version of tree view. */

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "themus-properties-view.h"
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>

static GType tpp_type = 0;
static void   property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface);
static GList *themus_properties_get_pages (NautilusPropertyPageProvider *provider,
					   GList *files);

static void
themus_properties_plugin_register_type (GTypeModule *module)
{
    static const GTypeInfo info = {
        sizeof (GObjectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        NULL,
        NULL,
        sizeof (GObject),
        0,
        (GInstanceInitFunc) NULL
    };
    static const GInterfaceInfo property_page_provider_iface_info = {
        (GInterfaceInitFunc)property_page_provider_iface_init,
        NULL,
        NULL
    };

    tpp_type = g_type_module_register_type (module, G_TYPE_OBJECT,
					    "ThemusPropertiesPlugin",
					    &info, 0);
    g_type_module_add_interface (module,
				 tpp_type,
				 NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
				 &property_page_provider_iface_info);
}

static void
property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
    iface->get_pages = themus_properties_get_pages;
}

static GList *
themus_properties_get_pages (NautilusPropertyPageProvider *provider,
			     GList *files)
{
    GList *pages = NULL;
    NautilusFileInfo *file;
    char *uri = NULL;
    GtkWidget *page, *label;
    NautilusPropertyPage *property_page;

    /* only add properties page if a single file is selected */
    if (files == NULL || files->next != NULL) goto end;
    file = files->data;

    /* only add the properties page to these mime types */
    if (!nautilus_file_info_is_mime_type (file, "application/x-gnome-theme") &&
	!nautilus_file_info_is_mime_type (file, "application/x-gnome-theme-installed"))
	goto end;

    /* okay, make the page */
    uri = nautilus_file_info_get_uri (file);
    label = gtk_label_new (_("Theme"));
    page = themus_properties_view_new (uri);
    property_page = nautilus_property_page_new ("theme-properties",
						label, page);

    pages = g_list_prepend (pages, property_page);

 end:
    g_free (uri);
    return pages;
}

/* --- extension interface --- */
void
nautilus_module_initialize (GTypeModule *module)
{
    themus_properties_plugin_register_type (module);
    themus_properties_view_register_type (module);

    /* set up translation catalog */
    bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
    static GType type_list[1];

    type_list[0] = tpp_type;
    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
