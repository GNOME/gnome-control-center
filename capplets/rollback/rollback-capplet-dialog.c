/* -*- mode: c; style: linux -*- */

/* rollback-capplet-dialog.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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
# include "config.h"
#endif

#include <glade/glade.h>
#include <bonobo.h>
#include <gnome-xml/parser.h>

#include <config-archiver/archiver-client.h>
#include <config-archiver/util.h>

#include "rollback-capplet-dialog.h"

static const gchar date_mod_units[] = {
	'y', 'M', 'M', 'd', 'd', 'd', 'h', 'm', 'm', 'm', 'm', '\0'
};

static const gint date_mod_values[] = {
	-1, -6, -1, -7, -3, -1, -1, -30, -10, -5, -1, 0
};

static const gchar *labels[] = {
	N_("1 year ago"), N_("6 months ago"), N_("1 month ago"), N_("1 week ago"),
	N_("3 days ago"), N_("1 day ago"), N_("1 hour ago"), N_("30 minutes ago"),
	N_("10 minutes ago"), N_("5 minutes ago"), N_("1 minute ago"), N_("Current time")
};

#define NUM_ROLLBACK_LEVELS (sizeof (labels) / sizeof (const gchar *))

enum {
	ARG_0,
	ARG_CAPPLET_NAME
};

struct _RollbackCappletDialogPrivate 
{
	GladeXML               *data;
	GtkWidget              *contents;
	GtkWidget              *control_socket;
	GtkWidget              *label;
	gchar                  *capplet_name;
	gchar                  *capplet_moniker_name;

	Bonobo_PropertyControl  property_control;
	Bonobo_PropertyBag      control_pb;

	guint                   rollback_level;

	struct tm               mod_dates[NUM_ROLLBACK_LEVELS - 1];
};

static GnomeDialogClass *parent_class;

static void      rollback_capplet_dialog_init       (RollbackCappletDialog      *rollback_capplet_dialog);
static void      rollback_capplet_dialog_class_init (RollbackCappletDialogClass *class);

static void      rollback_capplet_dialog_set_arg    (GtkObject                  *object, 
						     GtkArg                     *arg, 
						     guint                       arg_id);
static void      rollback_capplet_dialog_get_arg    (GtkObject                  *object, 
						     GtkArg                     *arg, 
						     guint                       arg_id);

static void      rollback_capplet_dialog_destroy    (GtkObject                  *object);
static void      rollback_capplet_dialog_finalize   (GtkObject                  *object);

static gchar    *get_moniker                        (gchar                      *capplet_name,
						     struct tm                  *date);
static void      get_modified_date                  (guint                       rollback_level,
						     struct tm                  *date);
static gboolean  do_setup                           (RollbackCappletDialog      *dialog);
static void      apply_settings                     (RollbackCappletDialog      *dialog);

static gboolean  is_leap_year                       (guint year);
static void      mod_date_by_str                    (struct tm                  *date,
						     gint                        value,
						     gchar                       unit);

static void      rollback_changed_cb                (RollbackCappletDialog      *dialog,
						     GtkAdjustment              *adj);
static void      apply_cb                           (GtkButton                  *button,
						     RollbackCappletDialog      *dialog);
static void      close_cb                           (GtkButton                  *button,
						     RollbackCappletDialog      *dialog);

guint
rollback_capplet_dialog_get_type (void)
{
	static guint rollback_capplet_dialog_type = 0;

	if (!rollback_capplet_dialog_type) {
		GtkTypeInfo rollback_capplet_dialog_info = {
			"RollbackCappletDialog",
			sizeof (RollbackCappletDialog),
			sizeof (RollbackCappletDialogClass),
			(GtkClassInitFunc) rollback_capplet_dialog_class_init,
			(GtkObjectInitFunc) rollback_capplet_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		rollback_capplet_dialog_type = 
			gtk_type_unique (gnome_dialog_get_type (), 
					 &rollback_capplet_dialog_info);
	}

	return rollback_capplet_dialog_type;
}

static void
rollback_capplet_dialog_init (RollbackCappletDialog *dialog)
{
	GtkAdjustment *adj;
	GtkWidget *range;
	GladeXML *data;

	static const gchar *buttons[] = {
		GNOME_STOCK_BUTTON_APPLY,
		GNOME_STOCK_BUTTON_CLOSE,
		NULL
	};

	data = glade_xml_new (GNOMECC_GLADE_DIR "/rollback.glade", "rollback_dialog");

	if (data == NULL) {
		g_critical ("Your Glade file is either missing or corrupt.");
		dialog->p = (gpointer) 0xdeadbeef; 
		return;
	}

	dialog->p                 = g_new0 (RollbackCappletDialogPrivate, 1);
	dialog->p->data           = data;
	dialog->p->contents       = glade_xml_get_widget (dialog->p->data, "rollback_dialog");
	dialog->p->control_socket = glade_xml_get_widget (dialog->p->data, "control_socket");
	dialog->p->label          = glade_xml_get_widget (dialog->p->data, "rollback_level_label");
	dialog->p->rollback_level = 12;

	range = glade_xml_get_widget (dialog->p->data, "rollback_scale");
	adj = GTK_ADJUSTMENT (gtk_adjustment_new (12, 0, 12, 1, 1, 1));
	gtk_range_set_adjustment (GTK_RANGE (range), adj);
	gtk_signal_connect_object (GTK_OBJECT (adj), "value-changed",
				   GTK_SIGNAL_FUNC (rollback_changed_cb), GTK_OBJECT (dialog));

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), dialog->p->contents, TRUE, TRUE, 0);

	gnome_dialog_constructv (GNOME_DIALOG (dialog), _("Rollback"), buttons);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, GTK_SIGNAL_FUNC (apply_cb), dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, GTK_SIGNAL_FUNC (close_cb), dialog);
}

static void
rollback_capplet_dialog_class_init (RollbackCappletDialogClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("RollbackCappletDialog::capplet-name",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_CAPPLET_NAME);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = rollback_capplet_dialog_destroy;
	object_class->finalize = rollback_capplet_dialog_finalize;
	object_class->set_arg = rollback_capplet_dialog_set_arg;
	object_class->get_arg = rollback_capplet_dialog_get_arg;

	parent_class = GNOME_DIALOG_CLASS
		(gtk_type_class (gnome_dialog_get_type ()));
}

static void
rollback_capplet_dialog_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackCappletDialog *dialog;
	gchar                 *tmp;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CAPPLET_DIALOG (object));

	dialog = ROLLBACK_CAPPLET_DIALOG (object);

	if (dialog->p == (gpointer) 0xdeadbeef)
		return;

	switch (arg_id) {
	case ARG_CAPPLET_NAME:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) != NULL);

		dialog->p->capplet_name = GTK_VALUE_POINTER (*arg);

		dialog->p->capplet_moniker_name = g_strdup (dialog->p->capplet_name);
		if ((tmp = strstr (dialog->p->capplet_moniker_name, "-capplet")) != NULL) *tmp = '\0';

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
rollback_capplet_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackCappletDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CAPPLET_DIALOG (object));

	dialog = ROLLBACK_CAPPLET_DIALOG (object);

	switch (arg_id) {
	case ARG_CAPPLET_NAME:
		GTK_VALUE_POINTER (*arg) = dialog->p->capplet_name;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
rollback_capplet_dialog_destroy (GtkObject *object) 
{
	RollbackCappletDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CAPPLET_DIALOG (object));

	dialog = ROLLBACK_CAPPLET_DIALOG (object);

	if (dialog->p != (gpointer) 0xdeadbeef) {
		bonobo_object_release_unref (dialog->p->property_control, NULL);
		bonobo_object_release_unref (dialog->p->control_pb, NULL);
		gtk_object_destroy (GTK_OBJECT (dialog->p->data));
	}

	if (dialog->p->capplet_name != NULL) {
		g_free (dialog->p->capplet_name);
		dialog->p->capplet_name = NULL;
	}

	if (dialog->p->capplet_moniker_name != NULL) {
		g_free (dialog->p->capplet_moniker_name);
		dialog->p->capplet_moniker_name = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
rollback_capplet_dialog_finalize (GtkObject *object) 
{
	RollbackCappletDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CAPPLET_DIALOG (object));

	dialog = ROLLBACK_CAPPLET_DIALOG (object);

	if (dialog->p != (gpointer) 0xdeadbeef)
		g_free (dialog->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkObject *
rollback_capplet_dialog_new (gchar *capplet_name) 
{
	GtkObject *object;

	object = gtk_object_new (rollback_capplet_dialog_get_type (),
				 "capplet-name", capplet_name,
				 NULL);

	if (ROLLBACK_CAPPLET_DIALOG (object)->p == (gpointer) 0xdeadbeef) {
		gtk_object_destroy (object);
		return NULL;
	}

	if (do_setup (ROLLBACK_CAPPLET_DIALOG (object))) {
		return object;
	} else {
		gtk_object_destroy (object);
		return NULL;
	}
}

static gchar *
get_moniker (gchar *capplet_name, struct tm *date) 
{
	if (date == NULL)
		return g_strconcat ("archive:user-archive#archiverdb:", capplet_name, NULL);
	else
		return g_strdup_printf
			("archive:user-archive#archiverdb:[|%04d%02d%02d%02d%02d%02d]%s",
			 date->tm_year + 1900, date->tm_mon + 1, date->tm_mday, 
			 date->tm_hour, date->tm_min, date->tm_sec, capplet_name);
}

static void
get_modified_date (guint rollback_level, struct tm *date) 
{
	time_t t;

	t = time (NULL);
	gmtime_r (&t, date);
	mod_date_by_str (date, date_mod_values[rollback_level], date_mod_units[rollback_level]);
}

/* do_setup
 *
 * Sets up the dialog's controls
 *
 * Returns TRUE on success and FALSE on failure
 */

static gboolean
do_setup (RollbackCappletDialog *dialog) 
{
	CORBA_Environment       ev;
	Bonobo_Control          control;

	BonoboControlFrame     *cf;

	GtkWidget              *control_wid;
	GtkWidget              *err_dialog;

	gchar                  *tmp, *tmp1;
	gchar                  *oaf_iid;
	gchar                  *moniker;

	guint                   i;

	CORBA_exception_init (&ev);

	tmp = g_strdup (dialog->p->capplet_moniker_name);
	while ((tmp1 = strchr (tmp, '-'))) *tmp1 = '_';

	oaf_iid = g_strconcat ("OAFIID:Bonobo_Control_Capplet_", tmp, NULL);
	dialog->p->property_control = bonobo_get_object (oaf_iid, "IDL:Bonobo/PropertyControl:1.0", &ev);
	g_free (oaf_iid);
	g_free (tmp);

	if (BONOBO_EX (&ev) || dialog->p->property_control == CORBA_OBJECT_NIL) {
		err_dialog = gnome_error_dialog ("Could not load the capplet.");
		gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));
		return FALSE;
	}

	control = Bonobo_PropertyControl_getControl (dialog->p->property_control, 0, &ev);

	if (BONOBO_EX (&ev) || control == CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (dialog->p->property_control, NULL);
		return FALSE;
	}

	control_wid = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);

	if (control_wid == NULL) {
		bonobo_object_release_unref (dialog->p->property_control, NULL);
		bonobo_object_release_unref (control, NULL);
		return FALSE;
	}

	for (i = 0; i < NUM_ROLLBACK_LEVELS - 1; i++)
		get_modified_date (i, &(dialog->p->mod_dates[i]));

	moniker = get_moniker (dialog->p->capplet_moniker_name, NULL);

	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (control_wid));
	dialog->p->control_pb = bonobo_control_frame_get_control_property_bag (cf, &ev);
	bonobo_property_bag_client_set_value_string (dialog->p->control_pb, "moniker", moniker, &ev);
	g_free (moniker);

	if (BONOBO_EX (&ev)) {
		err_dialog = gnome_error_dialog ("Could not load your configuration settings.");
		gnome_dialog_run_and_close (GNOME_DIALOG (err_dialog));
		bonobo_object_release_unref (dialog->p->property_control, NULL);
		bonobo_object_release_unref (dialog->p->control_pb, NULL);
		gtk_object_destroy (GTK_OBJECT (control_wid));
		return FALSE;
	}

/*  	gtk_widget_set_sensitive (control_wid, FALSE); */
	gtk_container_add (GTK_CONTAINER (dialog->p->control_socket), control_wid);

	gtk_widget_show_all (dialog->p->contents);

	CORBA_exception_free (&ev);

	return TRUE;
}

static void
apply_settings (RollbackCappletDialog *dialog) 
{
	ConfigArchiver_Archive   archive;
	ConfigArchiver_Location  location;
	CORBA_Environment        ev;
	xmlDocPtr                doc;

	if (dialog->p->rollback_level == NUM_ROLLBACK_LEVELS - 1)
		return;

	CORBA_exception_init (&ev);

	archive = bonobo_get_object ("archive:user-archive", "IDL:ConfigArchiver/Archive:1.0", &ev);

	if (BONOBO_EX (&ev) || archive == CORBA_OBJECT_NIL) {
		g_critical ("Could not retrieve archive (%s)", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	location = ConfigArchiver_Archive__get_currentLocation (archive, &ev);

	if (BONOBO_EX (&ev) || location == CORBA_OBJECT_NIL) {
		g_critical ("Could not retrieve current location (%s)", ev._repo_id);
		bonobo_object_release_unref (archive, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	doc = location_client_load_rollback_data
		(location, &(dialog->p->mod_dates[dialog->p->rollback_level]), 0, dialog->p->capplet_moniker_name, TRUE, &ev);

	if (BONOBO_EX (&ev) || doc == NULL) {
		gchar *filename;

		filename = g_strconcat (GNOMECC_DEFAULTS_DIR "/", dialog->p->capplet_moniker_name, ".xml", NULL);
		doc = xmlParseFile (filename);
		g_free (filename);

		if (doc == NULL) {
			g_critical ("Could not load rollback data (%s)", ev._repo_id);
			bonobo_object_release_unref (location, NULL);
			bonobo_object_release_unref (archive, NULL);
			CORBA_exception_free (&ev);
			return;
		}

		CORBA_exception_init (&ev);
	}

	location_client_store_xml
		(location, dialog->p->capplet_moniker_name, doc, ConfigArchiver_STORE_MASK_PREVIOUS, &ev);

	if (BONOBO_EX (&ev) || doc == NULL)
		g_critical ("Could not store rollback data (%s)", ev._repo_id);

	xmlFreeDoc (doc);
	bonobo_object_release_unref (archive, NULL);
	bonobo_object_release_unref (location, NULL);

	CORBA_exception_free (&ev);
}

static gboolean
is_leap_year (guint year) 
{
	if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
		return TRUE;
	else
		return FALSE;
}

/* mod_date_by_str
 *
 * Modify the given date structure using the given time differential string
 * encoding
 */

static void
mod_date_by_str (struct tm *date, gint value, gchar unit) 
{
	static const guint month_days[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	switch (unit) {
	case 'm':
		date->tm_min += value;
		break;
	case 'h':
		date->tm_hour += value;
		break;
	case 'd':
		date->tm_mday += value;
		break;
	case 'M':
		date->tm_mon += value;
		break;
	case 'y':
		date->tm_year += value;
		break;
	}

	if (date->tm_min < 0) {
		date->tm_min = 59;
		date->tm_hour--;
	}

	if (date->tm_hour < 0) {
		date->tm_hour = 23;
		date->tm_mday--;
	}

	if (date->tm_mday < 1) {
		if (date->tm_mon == 2 && is_leap_year (date->tm_year))
			date->tm_mday = 29;
		else
			date->tm_mday = month_days[(date->tm_mon + 11) % 12];

		date->tm_mon--;
	}

	if (date->tm_mon < 0) {
		date->tm_mon = 11;
		date->tm_year--;
	}
}

static void
rollback_changed_cb (RollbackCappletDialog *dialog,
		     GtkAdjustment         *adj)
{
	gchar             *moniker;

	CORBA_Environment  ev;

	CORBA_exception_init (&ev);

	dialog->p->rollback_level = adj->value;
	gtk_label_set_text (GTK_LABEL (dialog->p->label), labels[dialog->p->rollback_level]);

	if (dialog->p->rollback_level == NUM_ROLLBACK_LEVELS - 1)
		moniker = get_moniker (dialog->p->capplet_moniker_name, NULL);
	else
		moniker = get_moniker (dialog->p->capplet_moniker_name, &(dialog->p->mod_dates[dialog->p->rollback_level]));

	bonobo_property_bag_client_set_value_string (dialog->p->control_pb, "moniker", moniker, &ev);
	g_free (moniker);

	if (BONOBO_EX (&ev)) {
		g_critical ("Could not load settings for rollback level %.0f (%s)", adj->value, ev._repo_id);

		if (adj->value != dialog->p->rollback_level)
			gtk_adjustment_set_value (adj, dialog->p->rollback_level);
	} else {
		dialog->p->rollback_level = adj->value;
	}

	CORBA_exception_free (&ev);
}

static void
apply_cb (GtkButton *button, RollbackCappletDialog *dialog)
{
	apply_settings (dialog);
}

static void
close_cb (GtkButton *button, RollbackCappletDialog *dialog)
{
	gnome_dialog_close (GNOME_DIALOG (dialog));
}
