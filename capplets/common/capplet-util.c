/* -*- mode: c; style: linux -*- */

/* capplet-util.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>

#include "capplet-util.h"

static CreateDialogFn          create_dialog_cb = NULL;
static ApplySettingsFn         apply_settings_cb = NULL;
static SetupPropertyEditorsFn  setup_cb = NULL;

static BonoboControl          *control = NULL;
static GtkWidget              *widget;

/* apply_cb
 *
 * Callback issued when the user clicks "Apply" or "Ok". This function is
 * responsible for making sure the current settings are properly saved.
 */

static void
apply_cb (BonoboPropertyControl *pc, Bonobo_PropertyControl_Action action) 
{
	BonoboPropertyFrame *pf;
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	if (action == Bonobo_PropertyControl_APPLY) {
		CORBA_exception_init (&ev);

		pf = gtk_object_get_data (GTK_OBJECT (pc), "property-frame");
		db = gtk_object_get_data (GTK_OBJECT (pf), "config-database");
		bonobo_pbproxy_update (pf->proxy);
		Bonobo_ConfigDatabase_sync (db, &ev);

		CORBA_exception_free (&ev);
	}
}

/* changed_cb
 *
 * Callback issued when a setting in the ConfigDatabase changes.
 */

static void
changed_cb (BonoboListener *listener, gchar *event_name, CORBA_any *any,
	    CORBA_Environment *ev, Bonobo_ConfigDatabase db) 
{
	if (apply_settings_cb != NULL)
		apply_settings_cb (db);
}

/* get_moniker_cb
 *
 * Callback issued to retrieve the name of the moniker being used. This function
 * is just a formality.
 */

static void
get_moniker_cb (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id,
		CORBA_Environment *ev, BonoboControl *control) 
{
	BONOBO_ARG_SET_STRING (arg, gtk_object_get_data (GTK_OBJECT (control), "moniker"));
}

/* pf_destroy_cb
 *
 * Callback issued when the property frame is destroyed. Ensures that the
 * configuration database is properly unrefed
 */

static void
pf_destroy_cb (BonoboPropertyFrame *pf, Bonobo_ConfigDatabase db) 
{
	bonobo_object_release_unref (db, NULL);
}

/* set_moniker_cb
 *
 * Callback issued when the name of the moniker to be used is set. This function
 * does most of the dirty work -- creating the property editors that connect
 * properties to the dialog box.
 */

static void
set_moniker_cb (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id,
		CORBA_Environment *ev, BonoboControl *control) 
{
	gchar *moniker;
	gchar *full_moniker;
	BonoboPropertyFrame *pf;
	Bonobo_PropertyBag proxy;
	Bonobo_ConfigDatabase db;

	if (arg_id != 1) return;

	moniker = BONOBO_ARG_GET_STRING (arg);
	full_moniker = g_strconcat (moniker, "#config:/main", NULL);

	pf = BONOBO_PROPERTY_FRAME (bonobo_control_get_widget (control));
	bonobo_property_frame_set_moniker (pf, full_moniker);

	if (pf->proxy->bag == CORBA_OBJECT_NIL)
		gnome_error_dialog ("Could not load your configuration settings.");

	proxy = BONOBO_OBJREF (pf->proxy);

	db = gtk_object_get_data (GTK_OBJECT (pf), "config-database");

	if (db != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (db, ev);

	db = bonobo_get_object (moniker, "IDL:Bonobo/ConfigDatabase:1.0", ev);

	if (BONOBO_EX (ev) || db == CORBA_OBJECT_NIL)
		g_critical ("Could not resolve configuration moniker; will not be able to apply settings");

	gtk_object_set_data (GTK_OBJECT (pf), "config-database", db);
	gtk_signal_connect (GTK_OBJECT (pf), "destroy",
			    GTK_SIGNAL_FUNC (pf_destroy_cb), db);

	if (setup_cb != NULL)
		setup_cb (GTK_BIN (pf)->child, proxy);
}

/* get_control_cb
 *
 * Callback to construct the main dialog box for this capplet; invoked by Bonobo
 * whenever capplet activation is requested. Returns a BonoboObject representing
 * the control that encapsulates the object.
 */

static BonoboObject *
get_control_cb (BonoboPropertyControl *property_control, gint page_number) 
{
	BonoboPropertyBag    *pb;
	GtkWidget            *pf;

	if (control == NULL) {
		widget = create_dialog_cb ();

		if (widget == NULL)
			return NULL;

		pf = bonobo_property_frame_new (NULL, NULL);
		gtk_object_set_data (GTK_OBJECT (property_control),
				     "property-frame", pf);
		gtk_container_add (GTK_CONTAINER (pf), widget);
		gtk_widget_show_all (pf);

		control = bonobo_control_new (pf);

		pb = bonobo_property_bag_new ((BonoboPropertyGetFn) get_moniker_cb, 
					      (BonoboPropertySetFn) set_moniker_cb,
					      control);
		bonobo_control_set_properties (control, pb);
		bonobo_object_unref (BONOBO_OBJECT (pb));

		bonobo_property_bag_add (pb, "moniker", 1, BONOBO_ARG_STRING, NULL,
					 "Moniker for configuration",
					 BONOBO_PROPERTY_WRITEABLE);

		bonobo_control_set_automerge (control, TRUE);
	} else {
		return NULL;
	}

	return BONOBO_OBJECT (control);
}

/* real_quit_cb
 *
 * Release all objects and close down
 */

static gint 
real_quit_cb (BonoboPropertyControl *pc) 
{
#if 0
	CORBA_Environment ev;
	Bonobo_EventSource_ListenerId id;
	Bonobo_ConfigDatabase db;

/* Next bit won't work because object has been destroyed */
	DEBUG_MSG ("Enter");
	CORBA_exception_init (&ev);
	id = (Bonobo_EventSource_ListenerId)
		gtk_object_get_data (GTK_OBJECT (pc), "listener-id");
	db = (Bonobo_ConfigDatabase)
		gtk_object_get_data (GTK_OBJECT (pc), "db");

	bonobo_event_source_client_remove_listener (db, id, &ev);
	bonobo_object_release_unref (db, &ev);
	CORBA_exception_free (&ev);
	DEBUG_MSG ("Exit");
#endif
	gtk_main_quit ();

	return FALSE;
}

static void
quit_cb (BonoboPropertyControl *pc, Bonobo_ConfigDatabase db)
{
	gtk_object_set_data (GTK_OBJECT (pc), "db", db);
	gtk_idle_add ((GtkFunction)real_quit_cb, pc);
}

/* create_control_cb
 *
 * Small function to create the PropertyControl and return it.
 */

static BonoboObject *
create_control_cb (BonoboGenericFactory *factory, Bonobo_ConfigDatabase db) 
{
	static BonoboPropertyControl  *property_control = NULL;
	CORBA_Environment              ev;
	Bonobo_EventSource_ListenerId  id;

	if (property_control != NULL)
		return NULL;

	CORBA_exception_init (&ev);

	property_control = bonobo_property_control_new
		((BonoboPropertyControlGetControlFn) get_control_cb, 1, NULL);
	gtk_signal_connect (GTK_OBJECT (property_control), "action",
			    GTK_SIGNAL_FUNC (apply_cb), NULL);

	id = bonobo_event_source_client_add_listener
		(db, (BonoboListenerCallbackFn) changed_cb,
		 "Bonobo/ConfigDatabase:change", &ev, db);
	gtk_object_set_data (GTK_OBJECT (property_control), "listener-id", (gpointer) id);

	gtk_signal_connect (GTK_OBJECT (property_control), "destroy",
			    GTK_SIGNAL_FUNC (quit_cb), db);

	CORBA_exception_free (&ev);

	return BONOBO_OBJECT (property_control);
}

/* get_factory_name
 *
 * Construct the OAF IID of the factory from the binary name
 */

static gchar *
get_factory_name (const gchar *binary) 
{
	gchar *s, *tmp, *tmp1, *res;

	s = g_strdup (binary);
	tmp = strrchr (s, '/');
	if (tmp == NULL) tmp = s;
	else tmp++;
	if ((tmp1 = strstr (tmp, "-capplet")) != NULL) *tmp1 = '\0';
	while ((tmp1 = strchr (tmp, '-')) != NULL) *tmp1 = '_';

	res = g_strconcat ("OAFIID:Bonobo_Control_Capplet_", tmp, "_Factory", NULL);
	g_free (s);
	return res;
}

/* get_default_moniker
 *
 * Construct the default moniker for configuration from the binary name
 */

static gchar *
get_default_moniker (const gchar *binary) 
{
	gchar *s, *tmp, *tmp1, *res;

	s = g_strdup (binary);
	tmp = strrchr (s, '/');
	if (tmp == NULL) tmp = s;
	else tmp++;
	if ((tmp1 = strstr (tmp, "-capplet")) != NULL) *tmp1 = '\0';

	res = g_strconcat ("archiver:", tmp, NULL);
	g_free (s);
	return res;
}

/* get_property_name
 *
 * Get the property name associated with this capplet
 */

static gchar *
get_property_name (const gchar *binary) 
{
	gchar *s, *tmp, *tmp1, *res;

	s = g_strdup (binary);
	tmp = strrchr (s, '/');
	if (tmp == NULL) tmp = s;
	else tmp++;
	if ((tmp1 = strstr (tmp, "-capplet")) != NULL) *tmp1 = '\0';

	for (tmp1 = tmp; *tmp1 != '\0'; tmp1++) {
		*tmp1 = toupper (*tmp1);
		if (*tmp1 == '-') *tmp1 = '_';
	}

	res = g_strconcat ("GNOME_", tmp, NULL);
	g_free (s);
	return res;
}

/* setup_session_mgmt
 *
 * Make sure the capplet launches and applies its settings next time the user
 * logs in
 */

static void
setup_session_mgmt (const gchar *binary_name) 
{
	GnomeClient *client;
	GnomeClientFlags flags;
	gint token;
	gchar *restart_args[3];
	gchar *prop_name;

	g_return_if_fail (binary_name != NULL);

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		prop_name = get_property_name (binary_name);
		token = gnome_startup_acquire_token
			(prop_name, gnome_client_get_id (client));
		g_free (prop_name);

		if (token) {
			gnome_client_set_priority (client, 20);
			gnome_client_set_restart_style
				(client, GNOME_RESTART_ANYWAY);
			restart_args[0] = g_strdup (binary_name);
			restart_args[1] = "--init-session-settings";
			restart_args[2] = NULL;
			gnome_client_set_restart_command
				(client, 2, restart_args);
			g_free (restart_args[0]);
		} else {
			gnome_client_set_restart_style
				(client, GNOME_RESTART_NEVER);
		}
	}
}

/* capplet_init -- see documentation in capplet-util.h
 */

void
capplet_init (int                      argc,
	      char                   **argv,
	      ApplySettingsFn          apply_fn,
	      CreateDialogFn           create_dialog_fn,
	      SetupPropertyEditorsFn   setup_fn,
	      GetLegacySettingsFn      get_legacy_fn) 
{
	BonoboGenericFactory  *factory;
	Bonobo_ConfigDatabase  db;
	CORBA_ORB              orb;
	CORBA_Environment      ev;
	gchar                 *factory_iid;
	gchar                 *default_moniker;

	static gboolean apply_only;
	static gboolean init_session;
	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &init_session, 0,
		  N_("Initialize the session"), NULL },
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init (&ev);

	gnomelib_register_popt_table (cap_options, _("Capplet options"));
	gnome_init_with_popt_table (argv[0], VERSION, argc, argv,
				    oaf_popt_options, 0, NULL);

	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize bonobo");

	default_moniker = get_default_moniker (argv[0]);
	db = bonobo_get_object (default_moniker, "IDL:Bonobo/ConfigDatabase:1.0", &ev);

	if (db == CORBA_OBJECT_NIL) {
		g_critical ("Cannot open configuration database %s", default_moniker);
		exit (-1);
	}

	g_free (default_moniker);

	if ((apply_only || init_session) && apply_fn != NULL) {
		apply_fn (db);
	}
	else if (get_legacy && get_legacy_fn != NULL) {
		setup_session_mgmt (argv[0]);
		get_legacy_fn (db);
		Bonobo_ConfigDatabase_sync (db, &ev);
	} else {
		setup_session_mgmt (argv[0]);
		create_dialog_cb = create_dialog_fn;
		apply_settings_cb = apply_fn;
		setup_cb = setup_fn;
		factory_iid = get_factory_name (argv[0]);
		factory = bonobo_generic_factory_new
			(factory_iid, (BonoboGenericFactoryFn) create_control_cb, db);
		g_free (factory_iid);
		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
		bonobo_main ();
	}

	CORBA_exception_free (&ev);
}
