/* -*- mode: c; style: linux -*- */

/* sound-properties-capplet.c
 * Copyright (C) 2000 Ximian, Inc.
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

#define DEBUG_MSG(str, args...) \
              g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "(%d:%s) " str, \
		     getpid (), __FUNCTION__ , ## args)

/* Macros for variables that vary from capplet to capplet */

#define DEFAULT_MONIKER          "archiver:sound-properties"
#define FACTORY_IID              "OAFIID:Bonobo_Control_Capplet_sound_properties_Factory"
#define GLADE_FILE               GLADE_DATADIR "/sound-properties.glade"
#define G_LOG_DOMAIN             "sound-properties"

#include <gnome.h>
#include <bonobo.h>

#include <glade/glade.h>

/* FIXME: We should really have a single bonobo-conf.h header */

#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo-conf/bonobo-property-editor.h>
#include <bonobo-conf/bonobo-property-frame.h>

/* Needed only for the sound capplet */

#include <stdlib.h>
#include <esd.h>
#include <sys/types.h>

static BonoboControl *control = NULL;
static GladeXML      *dialog;
static GtkWidget     *widget;

/* Capplet-specific prototypes */

static void start_esd (void);

/* apply_settings
 *
 * Apply the settings of the property bag. This function is per-capplet, though
 * there are some cases where it does not do anything.
 */

static void
apply_settings (Bonobo_ConfigDatabase db, CORBA_Environment *ev) 
{
	CORBA_any *value;

	value = Bonobo_ConfigDatabase_getValue (db, "enable_esd", NULL, ev);

	if (BONOBO_EX (ev)) return;

        if (BONOBO_ARG_GET_BOOLEAN (value) && gnome_sound_connection < 0)
                start_esd ();

	/* I'm not going to deal with reloading samples until later. It's
	 * entirely too painful */
}

/* start_esd
 *
 * Start the Enlightenment Sound Daemon. This function is specific to the sound
 * properties capplet.
 */

static void
start_esd (void) 
{
#ifdef HAVE_ESD
        int esdpid;
        static const char *esd_cmdline[] = {"esd", "-nobeeps", NULL};
        char *tmpargv[3];
        char argbuf[32];
        time_t starttime;
        GnomeClient *client = gnome_master_client ();

        esdpid = gnome_execute_async (NULL, 2, (char **)esd_cmdline);
        g_snprintf (argbuf, sizeof (argbuf), "%d", esdpid);
        tmpargv[0] = "kill"; tmpargv[1] = argbuf; tmpargv[2] = NULL;
        gnome_client_set_shutdown_command (client, 2, tmpargv);
        starttime = time (NULL);
        gnome_sound_init (NULL);

        while (gnome_sound_connection < 0
	       && ((time(NULL) - starttime) < 4)) 
        {
#ifdef HAVE_USLEEP
                usleep(1000);
#endif
                gnome_sound_init(NULL);
        }
#endif
}

/* get_moniker_cb
 *
 * Callback issued to retrieve the name of the moniker being used. This function
 * is just a formality and does not vary between capplets
 */

static void
get_moniker_cb (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id,
		CORBA_Environment *ev, BonoboControl *control) 
{
	BONOBO_ARG_SET_STRING (arg, gtk_object_get_data (GTK_OBJECT (control), "moniker"));
}

/* set_moniker_cb
 *
 * Callback issued when the name of the moniker to be used is set. This function
 * does most of the dirty work -- creating the property editors that connect
 * properties to the dialog box. The portion of this function appropriately
 * labelled must be written once for each capplet.
 */

/* Macro to make it easier to reference Glade widgets */

#define WID(s) glade_xml_get_widget (dialog, s)

static void
set_moniker_cb (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id,
		CORBA_Environment *ev, BonoboControl *control) 
{
	gchar *moniker;
	BonoboPEditor *ed;
	BonoboPropertyFrame *pf;
	Bonobo_PropertyBag proxy;
	GladeXML *dialog;

	if (arg_id != 1) return;

	moniker = BONOBO_ARG_GET_STRING (arg);

	pf = BONOBO_PROPERTY_FRAME (bonobo_control_get_widget (control));
	bonobo_property_frame_set_moniker (pf, moniker);
	proxy = BONOBO_OBJREF (pf->proxy);
	dialog = gtk_object_get_data (GTK_OBJECT (control), "dialog");

	/* Begin per-capplet part */

	ed = BONOBO_PEDITOR (bonobo_peditor_boolean_construct (WID ("enable_toggle")));
	bonobo_peditor_set_property (ed, proxy, "start_esd", TC_boolean, NULL);

	ed = BONOBO_PEDITOR (bonobo_peditor_boolean_construct (WID ("events_toggle")));
	bonobo_peditor_set_property (ed, proxy, "event_sounds", TC_boolean, NULL);

	/* End per-capplet part */
}

/* close_cb
 *
 * Callback issued when the dialog is destroyed. Just resets the control pointer
 * to NULL so that the program does not think the dialog exists when it does
 * not. Does not vary from capplet to capplet.
 */

static void
close_cb (void)
{
	gtk_object_destroy (GTK_OBJECT (dialog));
	control = NULL;
}

/* create_dialog_cb
 *
 * Callback to construct the main dialog box for this capplet; invoked by Bonobo
 * whenever capplet activation is requested. Returns a BonoboObject representing
 * the control that encapsulates the object. This function should not vary from
 * capplet to capplet, though it assumes that the dialog data in the glade file
 * has the name "prefs_widget".  */

static BonoboObject *
create_dialog_cb (BonoboGenericFactory *factory, gpointer data) 
{
	BonoboPropertyBag    *pb;
	GtkWidget            *pf;

	if (control == NULL) {
		DEBUG_MSG ("Creating control");

		dialog = glade_xml_new (GLADE_FILE, "prefs_widget");

		if (dialog == NULL) {
			g_critical ("Could not load glade file");
			return NULL;
		}

		widget = glade_xml_get_widget (dialog, "prefs_widget");

		if (widget == NULL) {
			g_critical ("Could not find preferences widget");
			return NULL;
		}

		DEBUG_MSG ("Loaded dialog: %p, %p", dialog, widget);

		pf = bonobo_property_frame_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (pf), widget);
		gtk_widget_show_all (pf);

		control = bonobo_control_new (pf);
		gtk_object_set_data (GTK_OBJECT (control), "dialog", dialog);

		pb = bonobo_property_bag_new ((BonoboPropertyGetFn) get_moniker_cb, 
					      (BonoboPropertySetFn) set_moniker_cb,
					      control);
		bonobo_control_set_properties (control, pb);
		bonobo_object_unref (BONOBO_OBJECT (pb));

		bonobo_property_bag_add (pb, "moniker", 1, BONOBO_ARG_STRING, NULL,
					 "Moniker for configuration",
					 BONOBO_PROPERTY_WRITEABLE);

		bonobo_control_set_automerge (control, TRUE);

		gtk_signal_connect (GTK_OBJECT (widget), "destroy",
				    GTK_SIGNAL_FUNC (close_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (control), "destroy",
				    GTK_SIGNAL_FUNC (close_cb), NULL);
	} else {
		gtk_widget_show_all (widget);
	}

	return BONOBO_OBJECT (control);
}

/* main -- This function should not vary from capplet to capplet
 *
 * FIXME: Should there be this much code in main()? Seems a tad complicated;
 * some of it could be factored into a library, but which library should we use?
 * libcapplet is to be deprecated, so we don't want to use that, and it doesn't
 * seem right to use bonobo-conf for this purpose.
 *
 * *sigh*. I have a headache.
 */

int
main (int argc, char **argv) 
{
	BonoboGenericFactory *factory;
	Bonobo_ConfigDatabase db;
	CORBA_ORB orb;
	CORBA_Environment ev;

	static gboolean apply_only;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	glade_gnome_init ();
	gnomelib_register_popt_table (cap_options, _("Capplet options"));
	gnome_init_with_popt_table (argv[0], VERSION, argc, argv,
				    oaf_popt_options, 0, NULL);

	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize bonobo");

	if (apply_only) {
		CORBA_exception_init (&ev);
		db = bonobo_get_object (DEFAULT_MONIKER,
					"IDL:Bonobo/ConfigDatabase:1.0", &ev);

		if (db == CORBA_OBJECT_NIL) {
			g_critical ("Cannot open configuration database");
			return -1;
		}

		apply_settings (db, &ev);

		CORBA_exception_free (&ev);
	} else {
		factory = bonobo_generic_factory_new
			(FACTORY_IID, (BonoboGenericFactoryFn) create_dialog_cb, NULL);
		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
		bonobo_main ();
	}

	return 0;
}
