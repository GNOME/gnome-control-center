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

/* For stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "capplet-util.h"

#if 0

/* apply_cb
 *
 * Callback issued when the user clicks "Apply" or "Ok". This function is
 * responsible for making sure the current settings are properly saved.
 */

static void
apply_cb (BonoboPropertyControl *pc, Bonobo_PropertyControl_Action action) 
{
	if (action == Bonobo_PropertyControl_APPLY)
		gconf_engine_commit_change_set (gconf_engine_get_default (),
						changeset, TRUE, NULL);
}

/* properties_changed_cb
 *
 * Callback issued when some setting has changed
 */

static void
properties_changed_cb (GConfEngine *engine, guint cnxn_id, GConfEntry *entry, gpointer user_data) 
{
	if (apply_settings_cb != NULL)
		apply_settings_cb ();
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
	BonoboControl        *control;
	GtkWidget            *widget;

	widget = create_dialog_cb ();

	if (widget == NULL)
		return NULL;

	control = bonobo_control_new (widget);
	setup_property_editors_cb (widget, changeset);

	bonobo_control_set_automerge (control, TRUE);

	return BONOBO_OBJECT (control);
}

/* create_control_cb
 *
 * Small function to create the PropertyControl and return it.
 */

static BonoboObject *
create_control_cb (BonoboGenericFactory *factory, const gchar *component_id) 
{
	BonoboObject                  *obj;
	BonoboPropertyControl         *property_control;

	static const gchar            *prefix1 = "OAFIID:Bonobo_Control_Capplet_";

	g_message ("%s: Enter", G_GNUC_FUNCTION);

	if (!strncmp (component_id, prefix1, strlen (prefix1))) {
		property_control = bonobo_property_control_new
			((BonoboPropertyControlGetControlFn) get_control_cb, 1, NULL);
		g_signal_connect (G_OBJECT (property_control), "action",
				  G_CALLBACK (apply_cb), NULL);
		obj = BONOBO_OBJECT (property_control);
	} else {
		g_critical ("Not creating %s", component_id);
		obj = NULL;
	}

	return obj;
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
	if ((tmp1 = strstr (tmp, "-control")) != NULL) *tmp1 = '\0';
	if ((tmp1 = strstr (tmp, "-capplet")) != NULL) *tmp1 = '\0';
	while ((tmp1 = strchr (tmp, '-')) != NULL) *tmp1 = '_';

	res = g_strconcat ("OAFIID:Bonobo_", tmp, "_Factory", NULL);
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
	if ((tmp1 = strstr (tmp, "-control")) != NULL) *tmp1 = '\0';
	if ((tmp1 = strstr (tmp, "-capplet")) != NULL) *tmp1 = '\0';

	for (tmp1 = tmp; *tmp1 != '\0'; tmp1++) {
		*tmp1 = toupper (*tmp1);
		if (*tmp1 == '-') *tmp1 = '_';
	}

	res = g_strconcat ("GNOME_", tmp, NULL);
	g_free (s);
	return res;
}

#endif

/* setup_session_mgmt
 *
 * Make sure the capplet launches and applies its settings next time the user
 * logs in
 */

void
setup_session_mgmt (const gchar *binary_name) 
{
/* Disabled. I never really understood this code anyway, and I am absolutely
 * unclear about how to port it to GNOME 2.0 */
#if 0
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
#endif
}

#if 0

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
	gchar                         *factory_iid;
	BonoboGenericFactory          *factory;

	static gboolean apply_only;
	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	if (!bonobo_init (&argc, argv))
		g_error ("Cannot initialize bonobo");

	if (apply_only && apply_fn != NULL) {
		setup_session_mgmt (argv[0]);
		apply_fn ();
	}
	else if (get_legacy && get_legacy_fn != NULL) {
		setup_session_mgmt (argv[0]);
		get_legacy_fn ();
	} else {
		setup_session_mgmt (argv[0]);

		create_dialog_cb = create_dialog_fn;
		apply_settings_cb = apply_fn;
		setup_property_editors_cb = setup_fn;

		factory_iid = get_factory_name (argv[0]);
		factory = bonobo_generic_factory_new
			(factory_iid, (BonoboFactoryCallback) create_control_cb, NULL);
		g_free (factory_iid);
		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));

		changeset = gconf_change_set_new ();

		bonobo_main ();

		gconf_change_set_unref (changeset);
	}
}

#endif
