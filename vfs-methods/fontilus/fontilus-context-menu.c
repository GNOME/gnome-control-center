/* -*- mode: C; c-basic-offset: 4 -*-
 * Copyright (C) 2002 James Henstridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-menu-provider.h>

#define GTK_FONT_KEY "/desktop/gnome/interface/font_name"

#define FONTILUS_TYPE_CONTEXT_MENU (fontilus_context_menu_get_type ())
#define FONTILUS_CONTEXT_MENU(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), FONTILUS_TYPE_CONTEXT_MENU))

typedef struct {
    GObject parent;
} FontilusContextMenu;
typedef struct {
    GObjectClass parent_class;
} FontilusContextMenuClass;

static GType fcm_type = 0;
static GObjectClass *parent_class = NULL;
static GConfClient *default_client = NULL;

static void fontilus_context_menu_init       (FontilusContextMenu *self);
static void fontilus_context_menu_class_init (FontilusContextMenuClass *class);
static void menu_provider_iface_init (NautilusMenuProviderIface *iface);

static GList *fontilus_context_menu_get_file_items (NautilusMenuProvider *provider,
						    GtkWidget *window,
						    GList *files);
static void fontilus_context_menu_activate (NautilusMenuItem *item,
					    NautilusFileInfo *file);

static GType
fontilus_context_menu_get_type (void)
{
    return fcm_type;
}

static void
fontilus_context_menu_register_type (GTypeModule *module)
{
    static const GTypeInfo info = {
	sizeof (FontilusContextMenuClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) fontilus_context_menu_class_init,
	NULL,
	NULL,
	sizeof (FontilusContextMenu),
	0,
	(GInstanceInitFunc) fontilus_context_menu_init
    };
    static const GInterfaceInfo menu_provider_iface_info = {
	(GInterfaceInitFunc)menu_provider_iface_init,
	NULL,
	NULL
    };

    fcm_type = g_type_module_register_type (module,
					    G_TYPE_OBJECT,
					    "FontilusContextMenu",
					    &info, 0);
    g_type_module_add_interface (module,
				 fcm_type,
				 NAUTILUS_TYPE_MENU_PROVIDER,
				 &menu_provider_iface_info);
}

static void
fontilus_context_menu_class_init (FontilusContextMenuClass *class)
{
    parent_class = g_type_class_peek_parent (class);
}
static void menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
    iface->get_file_items = fontilus_context_menu_get_file_items;
}
static void
fontilus_context_menu_init (FontilusContextMenu *self)
{
}

static GList *
fontilus_context_menu_get_file_items (NautilusMenuProvider *provider,
				      GtkWidget *window,
				      GList *files)
{
    GList *items = NULL;
    NautilusFileInfo *file;
    NautilusMenuItem *item;
    char *scheme = NULL;

    /* only add a menu item if a single file is selected */
    if (files == NULL || files->next != NULL) goto end;

    file = files->data;
    scheme = nautilus_file_info_get_uri_scheme (file);

    /* only handle files under the fonts URI scheme */
    if (!scheme || g_ascii_strcasecmp (scheme, "fonts") != 0) goto end;
    if (nautilus_file_info_is_directory (file)) goto end;

    /* create the context menu item */
    item = nautilus_menu_item_new ("fontilus-set-default-font",
				   _("Set as Application Font"),
				   _("Sets the default application font"),
				   NULL);
    g_signal_connect_object (item, "activate",
			     G_CALLBACK (fontilus_context_menu_activate),
			     file, 0);
    items = g_list_prepend (items, item);
 end:
    g_free (scheme);

    return items;
}

static gchar *
get_font_name(const gchar *uri)
{
    gchar *unescaped;
    gchar *base;

    unescaped = gnome_vfs_unescape_string(uri, "/");
    if (!unescaped) return NULL;

    base = g_path_get_basename(unescaped);
    g_free(unescaped);
    if (!base) return NULL;

    return base;
}

static void
fontilus_context_menu_activate (NautilusMenuItem *item,
				NautilusFileInfo *file)
{
    char *uri, *font_name, *default_font;
    PangoFontDescription *fontdesc, *new_fontdesc;

    /* get the existing font */
    default_font = gconf_client_get_string(default_client,
					   GTK_FONT_KEY, NULL);
    if (default_font) {
	fontdesc = pango_font_description_from_string (default_font);
    } else {
	fontdesc = pango_font_description_new ();
    }
    g_free (default_font);

    /* get the new font name */
    uri = nautilus_file_info_get_uri (file);
    font_name = get_font_name (uri);
    g_free (uri);
    if (font_name) {
	new_fontdesc = pango_font_description_from_string (font_name);
    } else {
	new_fontdesc = pango_font_description_new ();
    }
    g_free (font_name);

    /* merge the new description into the old one */
    pango_font_description_merge (fontdesc, new_fontdesc, TRUE);

    default_font = pango_font_description_to_string (fontdesc);
    pango_font_description_free (fontdesc);
    pango_font_description_free (new_fontdesc);

    gconf_client_set_string(default_client, GTK_FONT_KEY, default_font, NULL);
    g_free (default_font);
}

/* --- extension interface --- */
void
nautilus_module_initialize (GTypeModule *module)
{
    fontilus_context_menu_register_type (module);

    default_client = gconf_client_get_default();

    /* set up translation catalog */
    bindtextdomain (GETTEXT_PACKAGE, FONTILUS_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
nautilus_module_shutdown (void)
{
    if (default_client)
	g_object_unref (default_client);
    default_client = NULL;
}
void
nautilus_module_list_types (const GType **types,
			    int          *num_types)
{
    static GType type_list[1];

    type_list[0] = FONTILUS_TYPE_CONTEXT_MENU;
    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
