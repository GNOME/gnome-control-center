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
#include "archive.h"
#include "util.h"

#define EX_SET_NOT_FOUND(ev) bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound)

static Archive *user_archive = NULL;
static Archive *global_archive = NULL;

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

static void
archive_destroy_cb (Archive *archive) 
{
	if (archive == global_archive)
		global_archive = NULL;
	else if (archive == user_archive)
		user_archive = NULL;
}

static Bonobo_Unknown
archive_resolve (BonoboMoniker               *moniker,
		 const Bonobo_ResolveOptions *options,
		 const CORBA_char            *requested_interface,
		 CORBA_Environment           *ev) 
{
	const gchar *name;

	Bonobo_Unknown ret;

	if (strcmp (requested_interface, "IDL:ConfigArchiver/Archive:1.0")) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	name = bonobo_moniker_get_name (moniker);

	if (!strcmp (name, "global-archive")) {
		if (global_archive == NULL) {
			global_archive = ARCHIVE (archive_load (TRUE));
			gtk_signal_connect (GTK_OBJECT (global_archive), "destroy", GTK_SIGNAL_FUNC (archive_destroy_cb), NULL);
			ret = CORBA_Object_duplicate (BONOBO_OBJREF (global_archive), ev);
		} else {
			ret = bonobo_object_dup_ref (BONOBO_OBJREF (global_archive), ev);
		}

		if (BONOBO_EX (ev)) {
			g_critical ("Cannot duplicate object");
			bonobo_object_release_unref (ret, NULL);
			ret = CORBA_OBJECT_NIL;
		}
	}
	else if (!strcmp (name, "user-archive")) {
		if (user_archive == NULL) {
			user_archive = ARCHIVE (archive_load (FALSE));
			gtk_signal_connect (GTK_OBJECT (user_archive), "destroy", GTK_SIGNAL_FUNC (archive_destroy_cb), NULL);
			ret = CORBA_Object_duplicate (BONOBO_OBJREF (user_archive), ev);
		} else {
			ret = bonobo_object_dup_ref (BONOBO_OBJREF (user_archive), ev);
		}

		if (BONOBO_EX (ev)) {
			g_critical ("Cannot duplicate object");
			bonobo_object_release_unref (ret, NULL);
			ret = CORBA_OBJECT_NIL;
		}
	} else {
		EX_SET_NOT_FOUND (ev);
		ret = CORBA_OBJECT_NIL;
	}

	return ret;
}

static Bonobo_Unknown
archiverdb_resolve (BonoboMoniker               *moniker,
		    const Bonobo_ResolveOptions *options,
		    const CORBA_char            *requested_interface,
		    CORBA_Environment           *ev)
{
	Bonobo_Moniker          parent;
	Bonobo_ConfigDatabase   db;
	const gchar            *name;
	gchar                  *backend_id, *locid;

	if (strcmp (requested_interface, "IDL:Bonobo/ConfigDatabase:1.0")) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL; 
	}

	parent = bonobo_moniker_get_parent (moniker, ev);
	if (BONOBO_EX (ev))
		return CORBA_OBJECT_NIL;

	if (parent == CORBA_OBJECT_NIL) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	name = bonobo_moniker_get_name (moniker);

	if (parse_name (name, &backend_id, &locid) < 0) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	db = bonobo_config_archiver_new (parent, options, backend_id, locid, ev);

	if (db == CORBA_OBJECT_NIL || BONOBO_EX (ev))
		EX_SET_NOT_FOUND (ev);

	bonobo_object_release_unref (parent, NULL);

	g_free (backend_id);
	g_free (locid);

	return db;
}			


static BonoboObject *
bonobo_moniker_archiver_factory (BonoboGenericFactory *this, 
				 const char           *object_id,
				 void                 *closure)
{
	if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_archiverdb")) {
		return BONOBO_OBJECT (bonobo_moniker_simple_new
				      ("archiverdb:", archiverdb_resolve));
	}
	else if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_archive")) {
		return BONOBO_OBJECT (bonobo_moniker_simple_new
				      ("archive:", archive_resolve));
	} else {
		g_warning ("Failing to manufacture a '%s'", object_id);
	}
	
	return NULL;
}

BONOBO_OAF_FACTORY_MULTI ("OAFIID:Bonobo_Moniker_archiver_Factory",
			  "bonobo xml archiver database moniker", "1.0",
			  bonobo_moniker_archiver_factory,
			  NULL);
