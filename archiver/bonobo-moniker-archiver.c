/*
 * bonobo-moniker-archiver.c: archiver xml database moniker implementation
 *
 * Author:
 *   Dietmar Maurer (dietmar@ximian.com)
 *   Bradford Hovinen  <hovinen@ximian.com>
 *
 * Copyright 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <bonobo.h>

#include "bonobo-config-archiver.h"
#include "archive.h"
#include "util.h"

static Archive *user_archive = NULL;
static Archive *global_archive = NULL;

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
	BonoboConfigArchiver   *archiver_db;
	const gchar            *name;

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

	archiver_db = bonobo_config_archiver_new (parent, options, name, ev);

	bonobo_object_release_unref (parent, NULL);

	if (archiver_db == NULL || BONOBO_EX (ev)) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	return CORBA_Object_duplicate (BONOBO_OBJREF (archiver_db), ev);
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
