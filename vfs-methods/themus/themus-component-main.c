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

#include <string.h>
#include "themus-component.h"
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#define VIEW_IID_APPLY      "OAFIID:Themus_Component_Apply"

static CORBA_Object
image_shlib_make_object (PortableServer_POA poa,
			 const char *iid,
			 gpointer impl_ptr,
			 CORBA_Environment *ev)
{
	ThemusComponent *component;

	component = g_object_new (TYPE_THEMUS_COMPONENT, NULL);

	bonobo_activation_plugin_use (poa, impl_ptr);

	return CORBA_Object_duplicate (BONOBO_OBJREF (component), ev);
}

static const BonoboActivationPluginObject plugin_list[] = {
	{ VIEW_IID_APPLY, image_shlib_make_object },
	{ NULL }
};

const BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list,
	"Themus Component"
};
