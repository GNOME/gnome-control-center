/*
 * bonobo-moniker-archiver.c: archiver xml database moniker implementation
 *
 * Author:
 *   Dietmar Maurer (dietmar@ximian.com)
 *   Bradford Hovinen  <hovinen@ximian.com>
 *
 * Copyright 2001 Ximian, Inc.
 */
#include <config.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-moniker-simple.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-exception.h>

#include "bonobo-config-archiver.h"

#define EX_SET_NOT_FOUND(ev) bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound)

/* parse_name
 *
 * Given a moniker with a backend id and (possibly) a location id encoded
 * therein, parse out the backend id and the location id and set the pointers
 * given to them.
 *
 * FIXME: Is this encoding really the way we want to do this? Ask Dietmar and
 * Michael.
 */

static gboolean
parse_name (const gchar *name, gchar **backend_id, gchar **location) 
{
	gchar *e;

	if (name[0] == '[') {
		e = strchr (name + 1, ']');

		if (e != NULL)
			*location = g_strndup (name + 1, e - name + 1);
		else
			return FALSE;

		*backend_id = g_strdup (e + 1);
	} else {
		*backend_id = g_strdup (name);
		*location = NULL;
	}

	return TRUE;
}

static Bonobo_Unknown
archiver_resolve (BonoboMoniker               *moniker,
		  const Bonobo_ResolveOptions *options,
		  const CORBA_char            *requested_interface,
		  CORBA_Environment           *ev)
{
	Bonobo_Moniker         parent;
	Bonobo_ConfigDatabase  db, pdb = CORBA_OBJECT_NIL;
	const gchar           *name;
	gchar                 *backend_id, *location;

	if (strcmp (requested_interface, "IDL:Bonobo/ConfigDatabase:1.0")) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL; 
	}

	parent = bonobo_moniker_get_parent (moniker, ev);
	if (BONOBO_EX (ev))
		return CORBA_OBJECT_NIL;

	name = bonobo_moniker_get_name (moniker);

	if (parent != CORBA_OBJECT_NIL) {
		pdb = Bonobo_Moniker_resolve (parent, options, 
					      "IDL:Bonobo/ConfigDatabase:1.0", ev);
    
		bonobo_object_release_unref (parent, NULL);
		
		if (BONOBO_EX (ev) || pdb == CORBA_OBJECT_NIL)
			return CORBA_OBJECT_NIL;
	}

	if (parse_name (name, &backend_id, &location) < 0) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	if (!(db = bonobo_config_archiver_new (backend_id, location))) {
		g_free (backend_id);
		g_free (location);
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL; 
	}

	g_free (backend_id);
	g_free (location);

	if (pdb != CORBA_OBJECT_NIL) {
		Bonobo_ConfigDatabase_addDatabase (db, pdb, "", Bonobo_ConfigDatabase_DEFAULT, ev);
		
		if (BONOBO_EX (ev)) {
			bonobo_object_release_unref (pdb, NULL);
			bonobo_object_release_unref (db, NULL);
			return CORBA_OBJECT_NIL; 
		}
	}

	return db;
}			


static BonoboObject *
bonobo_moniker_archiver_factory (BonoboGenericFactory *this, 
				 const char           *object_id,
				 void                 *closure)
{
	if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_archiver")) {

		return BONOBO_OBJECT (bonobo_moniker_simple_new (
		        "archiver:", archiver_resolve));
	
	} else
		g_warning ("Failing to manufacture a '%s'", object_id);
	
	return NULL;
}

BONOBO_OAF_FACTORY_MULTI ("OAFIID:Bonobo_Moniker_archiver_Factory",
			  "bonobo xml archiver database moniker", "1.0",
			  bonobo_moniker_archiver_factory,
			  NULL);
