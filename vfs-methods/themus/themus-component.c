/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more av.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libbonobo.h>
#include <libgnomevfs/gnome-vfs.h>
#include "themus-component.h"
#include <gnome-theme-info.h>
#include <gnome-theme-apply.h>
#include <gconf/gconf-client.h>

#include <stdlib.h>

#define FONT_KEY           "/desktop/gnome/interface/font_name"

static void
impl_Bonobo_Listener_event (PortableServer_Servant servant,
			    const CORBA_char *event_name,
			    const CORBA_any *args,
			    CORBA_Environment *ev)
{
	ThemusComponent *component;
	const CORBA_sequence_CORBA_string *list;
	
	GnomeVFSURI *uri;
	GnomeThemeMetaInfo *theme;
	GConfClient *client;

	component = THEMUS_COMPONENT (bonobo_object_from_servant (servant));

	if (!CORBA_TypeCode_equivalent (args->_type, TC_CORBA_sequence_CORBA_string, ev)) {
		return;
	}

	list = (CORBA_sequence_CORBA_string *)args->_value;

	g_return_if_fail (component != NULL);
	g_return_if_fail (list != NULL);
	
	if (strcmp (event_name, "ApplyTheme") == 0) {
		uri = gnome_vfs_uri_new (list->_buffer[0]);
		g_assert (uri != NULL);
		

		theme = gnome_theme_read_meta_theme (uri);
		gnome_vfs_uri_unref (uri);
		
		g_assert (theme != NULL);
		
		gnome_meta_theme_set (theme);
		if (theme->application_font)
		{
			client = gconf_client_get_default ();
			gconf_client_set_string (client, FONT_KEY, theme->application_font, NULL);
		}
	}
}


/* initialize the class */
static void
themus_component_class_init (ThemusComponentClass *class)
{
	POA_Bonobo_Listener__epv *epv = &class->epv;
	epv->event = impl_Bonobo_Listener_event;
}


static void
themus_component_init (ThemusComponent *component)
{
	gnome_theme_init (FALSE);
}

BONOBO_TYPE_FUNC_FULL (ThemusComponent, 
		       Bonobo_Listener, 
		       BONOBO_TYPE_OBJECT,
		       themus_component);
