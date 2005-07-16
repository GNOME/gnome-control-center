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
#include <libebook/e-book.h>

#include "e-image-chooser.h"
#include "gnome-about-me-password.h"

#include "capplet-util.h"

#define MAX_HEIGHT 150
#define MAX_WIDTH  150

typedef struct {
	EContact 	*contact;
	EBook    	*book;
	
	GladeXML 	*dialog;

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


static void about_me_set_address_field (EContactAddress *, guint, gchar *);


/*** Utility functions ***/
static void
about_me_error (GtkWindow *parent, gchar *str)
{
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (parent, 
				         GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				         GTK_BUTTONS_OK, str);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}


/********************/
static void
about_me_commit (GnomeAboutMe *me)
{
	EContactName *name;
	GError *error;

	char *strings[4], **stringptr;
	char *fileas;

	name = NULL;
	error = NULL;

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
			g_print ("There was an undetermined error\n");
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
	} else {
		str = "";
	}

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

static gchar *
about_me_get_address_field (EContactAddress *addr, guint cid)
{
	gchar *str;
	
	if (addr == NULL) {
		return NULL;
	}

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
		default:
			str = NULL;
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

	gchar 	      *data;
	gsize 	       length;

	dialog = me->dialog;


	if (me->image_changed && me->have_image) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
		GdkPixbuf *pixbuf, *scaled;
		int height, width;
		gboolean do_scale = FALSE;
		float scale;
		
		widget = WID ("image-chooser");
		e_image_chooser_get_image_data (E_IMAGE_CHOOSER (widget), &data, &length);

		/* Before updating the image in EDS scale it to a reasonable size
		   so that the user doesn't get an application that does not respond
		   or that takes 100% CPU */
		gdk_pixbuf_loader_write (loader, (guchar *)data, length, NULL);
		
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		
		if (pixbuf)
			gdk_pixbuf_ref (pixbuf);
			
		gdk_pixbuf_loader_close (loader, NULL);

		g_object_unref (loader);
		
		height = gdk_pixbuf_get_height (pixbuf);
		width = gdk_pixbuf_get_width (pixbuf);
		
		if (height >= width && height > MAX_HEIGHT) {
			scale = (float)MAX_HEIGHT/height;
			do_scale = TRUE;
		} else if (width > height && width > MAX_WIDTH) {
			scale = (float)MAX_WIDTH/width;
			do_scale = TRUE;
		}

		if (do_scale) {
			char *scaled_data = NULL;
			gsize scaled_length;
			
			scaled = gdk_pixbuf_scale_simple (pixbuf, width*scale, height*scale, GDK_INTERP_BILINEAR);
			gdk_pixbuf_save_to_buffer (scaled, &scaled_data, &scaled_length, "png", NULL, 
						   "compression", "9", NULL);
			
			g_free (data);
			data = scaled_data;
			length = scaled_length;
		}

		photo = g_new0 (EContactPhoto, 1);
		photo->data = data;
		photo->length = length;
		e_contact_set (me->contact, E_CONTACT_PHOTO, photo);

		/* Save the image for GDM */
		/* FIXME: I would have to read the default used by the gdmgreeter program */
		file = g_strdup_printf ("%s/.face", g_get_home_dir ());
		fp = fopen (file, "wb");
		fwrite (photo->data, 1, photo->length, fp);
		fclose (fp);

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
eab_create_image_chooser_widget (gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	GtkWidget *w = NULL;

	w = e_image_chooser_new ();
	gtk_widget_show_all (w);

	return w;
}

static void
about_me_image_clicked_cb (GtkWidget *button, GnomeAboutMe *me)
{
	GtkWidget *chooser_dialog;
	gint response;
	GtkWidget *image_chooser;
	GladeXML  *dialog;
	
	dialog = me->dialog;
	image_chooser = WID ("image-chooser");

	chooser_dialog = gtk_file_chooser_dialog_new (_("Select Image"), GTK_WINDOW (WID ("about-me-dialog")),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							_("No Image"), GTK_RESPONSE_NO,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_window_set_modal (GTK_WINDOW (chooser_dialog), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (chooser_dialog), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser_dialog), g_get_home_dir ());

	response=gtk_dialog_run (GTK_DIALOG (chooser_dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		gchar* filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser_dialog));
		me->have_image = TRUE;
		me->image_changed = TRUE;

		e_image_chooser_set_from_file (E_IMAGE_CHOOSER (image_chooser), filename);
		g_free (filename);
		about_me_update_photo (me);	
	} else if (response == GTK_RESPONSE_NO) {
		me->have_image = FALSE;
		me->image_changed = TRUE;
		e_image_chooser_set_from_file (E_IMAGE_CHOOSER (image_chooser), me->person);
		about_me_update_photo (me);
	}

	gtk_widget_destroy (chooser_dialog);
}

static void
about_me_image_changed_cb (GtkWidget *widget, GnomeAboutMe *me)
{
	me->have_image = TRUE;
	me->image_changed = TRUE;
	about_me_update_photo (me);
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
		if (me->commit_timeout_id) {
			g_source_remove (me->commit_timeout_id);
			about_me_commit (me);
		}

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
	gnome_about_me_password (GTK_WINDOW (WID ("about-me-dialog")));
}

static void
about_me_setup_dialog (void)
{
	GtkWidget    *widget;
	GtkWidget    *main_dialog;
	GtkIconInfo  *icon;
	GladeXML     *dialog;
	GError 	     *error = NULL;

	struct passwd *pwent;
	char *user = NULL;
	gchar *str;

	me = g_new0 (GnomeAboutMe, 1);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-about-me.glade", 
				"about-me-dialog", NULL);

	if (dialog == NULL) {
		g_error ("Unable to load glade file.");
		exit (1);
	}

	me->dialog = dialog;

	/* Connect the close button signal */
	main_dialog = WID ("about-me-dialog");
	g_signal_connect (G_OBJECT (main_dialog), "response",
			  G_CALLBACK (about_me_button_clicked_cb), me);

	gtk_window_set_resizable (GTK_WINDOW (main_dialog), FALSE);
	capplet_set_icon (main_dialog, "user-info");

	/* Setup theme details */
	me->screen = gtk_window_get_screen (GTK_WINDOW (main_dialog));
	me->theme = gtk_icon_theme_get_for_screen (me->screen);

	icon = gtk_icon_theme_lookup_icon (me->theme, "stock_person", 80, 0);

	me->person = g_strdup (gtk_icon_info_get_filename (icon));

	gtk_icon_info_free (icon);

	g_signal_connect_object (me->theme, "changed",
				 G_CALLBACK (about_me_icon_theme_changed),
				 main_dialog,
				 G_CONNECT_SWAPPED);

	/* Get the self contact */
	if (!e_book_get_self (&me->contact, &me->book, &error)) {
		me->create_self = TRUE;
		
		me->contact = e_contact_new ();

		g_warning ("%s\n", error->message);
		g_clear_error (&error);

		if (me->book == NULL) {
			me->book = e_book_new_system_addressbook (&error);
			if (me->book == NULL || error != NULL) {
				g_error ("%s\n", error->message);
				g_clear_error (&error);
			}

			if (e_book_open (me->book, FALSE, NULL) == FALSE) {
				about_me_error (GTK_WINDOW (main_dialog), 
						_("Unable to open address book"));
				g_clear_error (&error);
			}
		} 
	}

	/************************************************/
	user = get_user_login ();
	setpwent ();
	pwent = getpwnam (user);
	if (pwent == NULL) {
		about_me_error (GTK_WINDOW (WID ("about-me-dialog")), 
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

	if (tok[0] == NULL || strlen (tok[0]) == 0) {
		str = g_strdup_printf (_("About %s"), user);
	} else {
		str = g_strdup_printf (_("About %s"), tok[0]);
	}
	gtk_window_set_title (GTK_WINDOW (main_dialog), str);
	g_free (str);

	widget = WID ("password");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (about_me_passwd_clicked_cb), me);

	widget = WID ("button-image");
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (about_me_image_clicked_cb), me);

	widget = WID ("image-chooser");
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (about_me_image_changed_cb), me);

	about_me_load_info (me);

	gtk_widget_show_all (main_dialog);
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

	about_me_setup_dialog ();
	gtk_main ();

	return 0;
}
