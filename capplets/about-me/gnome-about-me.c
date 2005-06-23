/* gnome-about-me.c
 * Copyright (C) 2002 Diego Gonzalez 
 *
 * Written by: Diego Gonzalez <diego@pemas.net>
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

#include <gnome.h>
#include <pwd.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <pwd.h>
#include <unistd.h>
#include <bonobo/bonobo-main.h>
#include <libebook/e-book.h>

#include "e-image-chooser.h"
#include "gnome-about-me-password.h"

#include "capplet-util.h"

typedef struct {
	EContact 	*contact;
	EBook    	*book;
	
	GladeXML 	*dialog;

	GtkWidget    	*fsel;
	GdkScreen    	*screen;
	GtkIconTheme 	*theme;

	EContactAddress *addr1;
	EContactAddress *addr2;

	gboolean      	 have_image;
	gboolean      	 image_changed;
	gboolean      	 create_self;

	gchar        	*person;
	gchar 		*login;
	gchar 		*username;

	guint	      	 commit_timeout_id;
} GnomeAboutMe;

static GnomeAboutMe *me;

struct WidToCid{
	gchar *wid;
	guint  cid;
};

enum {
	ADDRESS_STREET = 1,
	ADDRESS_POBOX,
	ADDRESS_LOCALITY,
	ADDRESS_CODE,
	ADDRESS_REGION,
	ADDRESS_COUNTRY
};

#define ADDRESS_HOME		21
#define ADDRESS_WORK		27

struct WidToCid ids[] = {

	{ "email-work-e",      E_CONTACT_EMAIL_1             },	/* 00 */
	{ "email-home-e",      E_CONTACT_EMAIL_2             }, /* 01 */

	{ "phone-home-e",      E_CONTACT_PHONE_HOME          }, /* 02 */
	{ "phone-mobile-e",    E_CONTACT_PHONE_MOBILE        }, /* 03 */
	{ "phone-work-e",      E_CONTACT_PHONE_BUSINESS      }, /* 04 */
	{ "phone-work-fax-e",  E_CONTACT_PHONE_BUSINESS_FAX  }, /* 05 */

	{ "im-jabber-e",       E_CONTACT_IM_JABBER_HOME_1    }, /* 06 */
	{ "im-msn-e",          E_CONTACT_IM_MSN_HOME_1       }, /* 07 */
	{ "im-icq-e",          E_CONTACT_IM_ICQ_HOME_1       }, /* 08 */
	{ "im-yahoo-e",        E_CONTACT_IM_YAHOO_HOME_1     }, /* 09 */
	{ "im-aim-e",          E_CONTACT_IM_AIM_HOME_1       }, /* 10 */
	{ "im-groupwise-e",    E_CONTACT_IM_GROUPWISE_HOME_1 }, /* 11 */

        { "web-homepage-e",    E_CONTACT_HOMEPAGE_URL        }, /* 12 */
        { "web-calendar-e",    E_CONTACT_CALENDAR_URI        }, /* 13 */
        { "web-weblog-e",      E_CONTACT_BLOG_URL            }, /* 14 */

        { "job-profession-e",  E_CONTACT_ROLE                }, /* 15 */
        { "job-title-e",       E_CONTACT_TITLE               }, /* 16 */
        { "job-dept-e",        E_CONTACT_ORG_UNIT            }, /* 17 */
        { "job-assistant-e",   E_CONTACT_ASSISTANT           }, /* 18 */
        { "job-company-e",     E_CONTACT_ORG                 }, /* 19 */
        { "job-manager-e",     E_CONTACT_MANAGER             }, /* 20 */

	{ "addr-street-1",     ADDRESS_STREET                }, /* 21 */
	{ "addr-po-1", 	       ADDRESS_POBOX                 }, /* 22 */
	{ "addr-locality-1",   ADDRESS_LOCALITY              }, /* 23 */
	{ "addr-code-1",       ADDRESS_CODE                  }, /* 24 */
	{ "addr-region-1",     ADDRESS_REGION                }, /* 25 */
	{ "addr-country-1",    ADDRESS_COUNTRY               }, /* 26 */

	{ "addr-street-2",     ADDRESS_STREET                }, /* 27 */
	{ "addr-po-2", 	       ADDRESS_POBOX                 }, /* 28 */
	{ "addr-locality-2",   ADDRESS_LOCALITY              }, /* 29 */
	{ "addr-code-2",       ADDRESS_CODE                  }, /* 30 */
	{ "addr-region-2",     ADDRESS_REGION                }, /* 31 */
	{ "addr-country-2",    ADDRESS_COUNTRY               }, /* 32 */

        {     NULL,            0                             }
};

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static void about_me_set_address_field (EContactAddress *, guint, gchar *);


/*** Utility functions ***/
static void
about_me_error (GtkWidget *parent, gchar *str)
{
	GtkWidget *warn;
	
	warn = gtk_message_dialog_new (GTK_WINDOW (parent), 
				       GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK, str);

	g_signal_connect (warn, "response", G_CALLBACK (gtk_widget_destroy), NULL);
}


/********************/
static void
about_me_commit (GnomeAboutMe *me)
{
	EContactName *name;
	char *fileas;
	char *strings[4], **stringptr;
	GError *error = NULL;


	if (me->create_self) {
		if (me->username == NULL)
			fileas = g_strdup ("Myself");
		else {
			name = e_contact_name_from_string (me->username);

			stringptr = strings;
			if (name->family && *name->family)
				*(stringptr++) = name->family;
			if (name->given && *name->given)
				*(stringptr++) = name->given;
			*stringptr = NULL;
			fileas = g_strjoinv (", ", strings);
		}

		e_contact_set (me->contact, E_CONTACT_FILE_AS, fileas);
		e_contact_set (me->contact, E_CONTACT_NICKNAME, "nickname");
		e_contact_set (me->contact, E_CONTACT_FULL_NAME, me->username);

		e_contact_name_free (name);
		g_free (fileas);
	}

	if (me->create_self) {
		e_book_add_contact (me->book, me->contact, &error);
		e_book_set_self (me->book, me->contact, &error);
	} else {
		if (e_book_commit_contact (me->book, me->contact, &error) == FALSE)
			g_print ("There was an undeterminad error\n");
	}

	me->create_self = FALSE;
}

static gboolean 
about_me_commit_from_timeout (GnomeAboutMe *me)
{
	about_me_commit (me);

	return FALSE;
}

static gboolean
about_me_focus_out (GtkWidget *widget, GdkEventFocus *event, GnomeAboutMe *me)
{
	gchar *str = NULL;
	const gchar *wid;
	gint i;
	
	wid = glade_get_widget_name (widget);
	
	for (i = 0; ids[i].wid != NULL; i++)
		if (g_ascii_strcasecmp (ids[i].wid, wid) == 0)
			break;

	if (ids[i].cid == 0) {
		return FALSE;
	}

	if (GTK_IS_ENTRY (widget)) {
		str = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer   *buffer;
		GtkTextIter      iter_start;
		GtkTextIter	 iter_end;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		gtk_text_buffer_get_start_iter (buffer, &iter_start);
		iter_end = iter_start;
		gtk_text_iter_forward_to_end (&iter_end);
		str = gtk_text_iter_get_text (&iter_start, &iter_end);
	}

	str = str ? str : "";
	
	/* FIXME: i'm getting an empty address field in evolution */
	if (i >= ADDRESS_HOME && i < ADDRESS_WORK) {
		about_me_set_address_field (me->addr1, ids[i].cid, str);
		e_contact_set (me->contact, E_CONTACT_ADDRESS_HOME, me->addr1);
	} else if (i >= ADDRESS_WORK) {
		about_me_set_address_field (me->addr2, ids[i].cid, str);
		e_contact_set (me->contact, E_CONTACT_ADDRESS_WORK, me->addr2);
	} else {
		e_contact_set (me->contact, ids[i].cid, str);
	}

	g_free (str);

	if (me->commit_timeout_id) {
		g_source_remove (me->commit_timeout_id);
	}

	me->commit_timeout_id = g_timeout_add (600, (GSourceFunc) about_me_commit_from_timeout, me);

	return FALSE;
}

/********************/

static char *
get_user_login (void)
{
	char buf[LINE_MAX * 4];
	struct passwd pwd, *err;

	int i;
	i = getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &err);
	return ((i == 0) && (err == &pwd)) ? g_strdup(pwd.pw_name) : NULL;
}

/*
 * Helpers
 */

GtkWidget * 
about_me_get_widget (GladeXML *dialog, gchar *name, gint suffix)
{
	GtkWidget *widget;
	gchar *str;

	str = g_strdup_printf ("%s-%d", name, suffix);
	widget = WID(str);
	g_free (str);

	return widget;
}

static gchar *
about_me_get_address_field (EContactAddress *addr, guint cid)
{
	gchar *str = NULL;
	
	if (addr == NULL)
		return NULL;

	switch (cid) {
		case ADDRESS_STREET:
			str = addr->street;
			break;
		case ADDRESS_POBOX:
			str = addr->po;
			break;
		case ADDRESS_LOCALITY:
			str = addr->locality;
			break;
		case ADDRESS_CODE:
			str = addr->code;
			break;
		case ADDRESS_REGION:
			str = addr->region;
			break;
		case ADDRESS_COUNTRY:
			str = addr->country;
			break;
	}

	return str;
}

static void
about_me_set_address_field (EContactAddress *addr, guint cid, gchar *str)
{
	switch (cid) {
		case ADDRESS_STREET:
			if (addr->street)
				g_free (addr->street);
			addr->street = g_strdup (str);
			break;
		case ADDRESS_POBOX:
			if (addr->po)
				g_free (addr->po);
			addr->po = g_strdup (str);
			break;
		case ADDRESS_LOCALITY:
			if (addr->locality)
				g_free (addr->locality);
			addr->locality = g_strdup (str);
			break;
		case ADDRESS_CODE:
			if (addr->code)
				g_free (addr->code);
			addr->code = g_strdup (str);
			break;
		case ADDRESS_REGION:
			if (addr->region)
				g_free (addr->region);
			addr->region = g_strdup (str);
			break;
		case ADDRESS_COUNTRY:
			if (addr->country)
				g_free (addr->country);
			addr->country = g_strdup (str);
			break;
	}
}

/**
 * about_me_load_string_field:
 *
 * wid: glade widget name
 * cid: id of the field (EDS id)
 * aid: position in the array WidToCid
 **/

static void
about_me_load_string_field (GnomeAboutMe *me, const gchar *wid, guint cid, guint aid)
{
	GtkWidget *widget;
	GladeXML  *dialog;
	gchar     *str;

	dialog = me->dialog;

	widget = WID (wid);

	if (me->create_self == TRUE) {
		g_signal_connect (widget, "focus-out-event", G_CALLBACK (about_me_focus_out), me);
		return;
	}

	if (aid >= ADDRESS_HOME && aid < ADDRESS_WORK) {
		str = about_me_get_address_field (me->addr1, cid);
	} else if (aid >= ADDRESS_WORK) {
		str = about_me_get_address_field (me->addr2, cid);
	} else {
		str = e_contact_get_const (me->contact, cid);
	}

	str = str ? str : "";
	
	if (GTK_IS_ENTRY (widget)) {
		gtk_entry_set_text (GTK_ENTRY (widget), str);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		gtk_text_buffer_set_text (buffer, str, -1);
	}

	g_signal_connect (widget, "focus-out-event", G_CALLBACK (about_me_focus_out), me);
}

static void
about_me_load_photo (GnomeAboutMe *me, EContact *contact)
{
	GtkWidget     *widget;
	GladeXML      *dialog;
	EContactPhoto *photo;

	dialog = me->dialog;

	widget = WID ("image-chooser");

	e_image_chooser_set_from_file (E_IMAGE_CHOOSER (widget), me->person);

	photo = e_contact_get (contact, E_CONTACT_PHOTO);

	if (photo) {
		me->have_image = TRUE;
		e_image_chooser_set_image_data (E_IMAGE_CHOOSER (widget), photo->data, photo->length);
		e_contact_photo_free (photo);
	} else {
		me->have_image = FALSE;
	}
}

static void
about_me_update_photo (GnomeAboutMe *me)
{
	GtkWidget     *widget;
	GladeXML      *dialog;	
	EContactPhoto *photo;
	gchar         *file;
	FILE	      *fp;

	dialog = me->dialog;

	if (me->image_changed && me->have_image) {
		widget = WID ("image-chooser");

		photo = g_new0 (EContactPhoto, 1);
		e_image_chooser_get_image_data (E_IMAGE_CHOOSER (widget), &photo->data, &photo->length);
		e_contact_set (me->contact, E_CONTACT_PHOTO, photo);

		/* Save the image for GDM */
		file = g_strdup_printf ("%s/.face", g_get_home_dir ());
		fp = fopen (file, "wb");
		fwrite (photo->data, 1, photo->length, fp);
		fclose (fp);

		/* Update GDM configuration */
		/*
		gnome_config_set_string ("/gdmphotosetup/last/picture", file);
		gnome_config_set_string ("/gdm/face/picture", file);
		gnome_config_sync ();
		*/
		g_free (file);	

		e_contact_photo_free (photo);

	} else if (me->image_changed && !me->have_image) {
		/* Update the image in the card */
		e_contact_set (me->contact, E_CONTACT_PHOTO, NULL);

		/* Update GDM configuration */
		gnome_config_set_string ("/gdmphotosetup/last/picture", "");
		gnome_config_set_string ("/gdm/face/picture", "");
		gnome_config_sync ();
	}

	about_me_commit (me);	
}

static void
about_me_load_info (GnomeAboutMe *me)
{
	gint i;
	
	if (me->create_self == FALSE) {
		me->addr1 = e_contact_get (me->contact, E_CONTACT_ADDRESS_HOME);
		if (me->addr1 == NULL)
			me->addr1 = g_new0 (EContactAddress, 1);
		me->addr2 = e_contact_get (me->contact, E_CONTACT_ADDRESS_WORK);
		if (me->addr2 == NULL)
			me->addr2 = g_new0 (EContactAddress, 1);
	} else {
		me->addr1 = g_new0 (EContactAddress, 1);
		me->addr2 = g_new0 (EContactAddress, 1);
	}

	for (i = 0; ids[i].wid != NULL; i++) {
		about_me_load_string_field (me, ids[i].wid, ids[i].cid, i);
	}
}

GtkWidget *
eab_create_image_chooser_widget(gchar *name,
				gchar *string1, gchar *string2,
				gint int1, gint int2)
{
	GtkWidget *w = NULL;

	w = e_image_chooser_new ();
	gtk_widget_show_all (w);

	return w;
}

/*
 * Functions to handle the photo changing stuff
 */

static void
image_selected (GnomeAboutMe *me)
{
	GtkWidget *widget;
	GladeXML  *dialog;
	gchar     *image;

	me->have_image = TRUE;
	me->image_changed = TRUE;

	dialog = me->dialog;

	widget = WID ("image-chooser");

	/* obtener el fichero seleccionado */
	image = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (me->fsel));
	e_image_chooser_set_from_file (E_IMAGE_CHOOSER (widget), image);
	g_free (image);

	about_me_update_photo (me);

	return;
}

static void
image_cleared (GnomeAboutMe *me)
{
	GtkWidget *widget;
	GladeXML  *dialog;

	me->have_image = FALSE;
	me->image_changed = TRUE;

	dialog = me->dialog;

	widget = WID ("image-chooser");

	e_image_chooser_set_from_file (E_IMAGE_CHOOSER (widget), me->person);
	about_me_update_photo (me);

	return;
}

static void
about_me_file_chooser_response (GtkWidget *widget, gint response, GnomeAboutMe *me)
{
	if (response == GTK_RESPONSE_ACCEPT)
		image_selected (me);
	else if (response == GTK_RESPONSE_NO)
		image_cleared (me);

	gtk_widget_hide (me->fsel);
}

static void
about_me_image_clicked_cb (GtkWidget *button, GnomeAboutMe *me)
{
	char *title = _("Select Image");
	char *noimage = _("No Image");
	
	me->fsel = gtk_file_chooser_dialog_new (title, NULL,
						GTK_FILE_CHOOSER_ACTION_OPEN,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						noimage, GTK_RESPONSE_NO,
						NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (me->fsel), GTK_RESPONSE_ACCEPT);

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (me->fsel), g_get_home_dir ());

	g_signal_connect (me->fsel, "response",
			  G_CALLBACK (about_me_file_chooser_response), me);

	gtk_window_present (GTK_WINDOW (me->fsel));
}



/* About Me Dialog Callbacks */

static void
about_me_icon_theme_changed (GtkWindow    *window,
			     GtkIconTheme *theme)
{
	GtkWidget   *widget;
	GtkIconInfo *icon;
	GladeXML    *dialog;

	icon = gtk_icon_theme_lookup_icon (me->theme, "stock_person", 80, 0);
	if (icon == NULL) {
		g_print ("Icon not found\n");
	}
	g_free (me->person);
	me->person = g_strdup (gtk_icon_info_get_filename (icon));

	gtk_icon_info_free (icon);

	if (me->have_image) {
		dialog = me->dialog;

		widget = WID ("image-chooser");
		e_image_chooser_set_from_file (E_IMAGE_CHOOSER (widget), me->person);
	}
}

static void
about_me_button_clicked_cb (GtkDialog *dialog, gint response_id, GnomeAboutMe *me) 
{
	if (response_id == GTK_RESPONSE_HELP)
		g_print ("Help goes here");
	else {
		e_contact_address_free (me->addr1);
		e_contact_address_free (me->addr2);

		g_object_unref (me->contact);
		g_object_unref (me->book);
		g_object_unref (me->dialog);

		g_free (me->person);
		g_free (me);

		gtk_main_quit ();
	}
}

static void
about_me_passwd_clicked_cb (GtkWidget *button, GnomeAboutMe *me)
{
	GladeXML *dialog;
	
	dialog = me->dialog;
	gnome_about_me_password (WID ("about-me-dialog"));
}

static void
about_me_setup_dialog (void)
{
	GtkWidget    *widget;
	GtkIconInfo  *icon;
	GladeXML     *dialog;
	GError 	     *error = NULL;

	struct passwd *pwent;
	char *user = NULL;
	gchar *str;

	me = g_new0 (GnomeAboutMe, 1);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-about-me.glade", 
				"about-me-dialog", NULL);

	me->dialog = dialog;

	/* Setup theme details */
	me->screen = gtk_window_get_screen (WID ("about-me-dialog"));
	me->theme = gtk_icon_theme_get_for_screen (me->screen);

	icon = gtk_icon_theme_lookup_icon (me->theme, "stock_person", 80, 0);
	if (icon == NULL) {
		g_print ("Icon not found\n");
	}

	me->person = g_strdup (gtk_icon_info_get_filename (icon));

	gtk_icon_info_free (icon);

	g_signal_connect_object (me->theme, "changed",
				 G_CALLBACK (about_me_icon_theme_changed),
				 GTK_WIDGET (WID ("about-me-dialog")),
				 G_CONNECT_SWAPPED);

	/* Get the self contact */
	if (!e_book_get_self (&me->contact, &me->book, &error)) {
		me->create_self = TRUE;
		
		me->contact = e_contact_new ();
		g_print ("%s\n", error->message);

		g_clear_error (&error);

		if (me->book == NULL) {
			me->book = e_book_new_system_addressbook (&error);
			if (me->book == NULL)
				g_print ("error message: %s\n", error->message);

			if (e_book_open (me->book, FALSE, NULL) == FALSE) {
				about_me_error (WID ("about-me-dialog"), 
						_("Unable to open address book"));
			}
		} 
	}

	/************************************************/
	user = get_user_login ();
	setpwent ();
	pwent = getpwnam (user);
	if (pwent == NULL) {
		about_me_error (WID ("about-me-dialog"), 
				_("Unknown login ID, the user database might be corrupted"));
		return ;
	}
	gchar **tok;
	tok = g_strsplit (pwent->pw_gecos, ",", 0);

	/************************************************/

	me->login = g_strdup (user);
	if (tok[0] == NULL || strlen (tok[0]) == 0)
		me->username = NULL;
	else
		me->username = g_strdup (tok[0]);

	/* Contact Tab */
	about_me_load_photo (me, me->contact);

	widget = WID ("fullname"); 
	if (tok[0] == NULL || strlen (tok[0]) == 0) {
		str = g_strdup_printf ("<b><span size=\"xx-large\">%s</span></b>", user);
	} else {
		str = g_strdup_printf ("<b><span size=\"xx-large\">%s</span></b>", tok[0]);
	}

	gtk_label_set_markup (GTK_LABEL (widget), str);
	g_free (str);

	widget = WID ("login");
	gtk_label_set_text (GTK_LABEL (widget), user);

	widget = WID ("about-me-dialog");
	if (tok[0] == NULL || strlen (tok[0]) == 0) {
		str = g_strdup_printf ("About %s", user);
	} else {
		str = g_strdup_printf ("About %s", tok[0]);
	}
	gtk_window_set_title (GTK_WINDOW (widget), str);
	g_free (str);

	widget = WID("password");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (about_me_passwd_clicked_cb), me);

	widget = WID ("button-image");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (about_me_image_clicked_cb), me);

	about_me_load_info (me);

	/* Connect the close button signal */
	widget = WID ("about-me-dialog");
	g_signal_connect (G_OBJECT (widget), "response",
			  G_CALLBACK (about_me_button_clicked_cb), me);

	/* TODO: Set dialog icon */
	gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);

	capplet_set_icon (widget, "gnome-about-me.png");

	gtk_widget_show_all (widget);
}

int
main (int argc, char **argv)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-about-me", VERSION,
		LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		NULL);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo");

	about_me_setup_dialog ();
	gtk_main ();

	return 0;
}
