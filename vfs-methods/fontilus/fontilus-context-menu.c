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

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libbonobo.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#define GTK_FONT_KEY "/desktop/gnome/interface/font_name"

static GConfClient *default_client;

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

/* create a font string suitable to store in GTK_FONT_KEY using font_name.
 * tries to preserve the font size from the old string */
static gchar *
make_font_string(const gchar *old_string, const gchar *font_name)
{
    const gchar *space;

    if (!old_string)
	return g_strdup(font_name);

    space = strrchr(old_string, ' ');
    /* if no space, or the character after the last space is not a digit */
    if (!space || (space[1] < '0' || '9' < space[1]))
	return g_strdup(font_name);

    /* if it looks like the last word is a number, append it to the
     * font name to form the new name */
    return g_strconcat(font_name, space, NULL);
}

static void
handle_event(BonoboListener *listener, const gchar *event_name,
	     const CORBA_any *args, CORBA_Environment *ev, gpointer user_data)
{
    const CORBA_sequence_CORBA_string *list;
    gchar *font_name = NULL;

    if (!CORBA_TypeCode_equivalent(args->_type, TC_CORBA_sequence_CORBA_string, ev)) {
	goto end;
    }
    list = (CORBA_sequence_CORBA_string *)args->_value;

    if (list->_length != 1) {
	goto end;
    }

    font_name = get_font_name(list->_buffer[0]);
    if (!font_name) goto end;

    /* set font */
    if (!strcmp(event_name, "SetAsApplicationFont")) {
	gchar *curval, *newval;

	curval = gconf_client_get_string(default_client,
					 GTK_FONT_KEY, NULL);
	newval = make_font_string(curval, font_name);
	gconf_client_set_string(default_client, GTK_FONT_KEY, newval, NULL);
	g_free(newval);
	g_free(curval);
    } else {
	goto end;
    }

 end:
    g_free(font_name);
}

/* --- factory --- */

static BonoboObject *
view_factory(BonoboGenericFactory *this_factory,
	     const gchar *iid,
	     gpointer user_data)
{
    BonoboListener *listener;

    listener = bonobo_listener_new(handle_event, NULL);

    return BONOBO_OBJECT(listener);
}

int
main(int argc, char *argv[])
{
    bindtextdomain(GETTEXT_PACKAGE, FONTILUS_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    BONOBO_FACTORY_INIT(_("Font context menu items"), VERSION,
			&argc, argv);

    default_client = gconf_client_get_default();
    g_return_val_if_fail(default_client != NULL, -1);

    return bonobo_generic_factory_main("OAFIID:Fontilus_Context_Menu_Factory",
				       view_factory, NULL);
}
