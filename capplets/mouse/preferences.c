/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1998, 1999 Red Hat, Inc., Tom Tromey
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>,
 *            Jonathan Blandford <jrb@redhat.com>,
 *            Tom Tromey <tromey@cygnus.com>
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

#include <stdlib.h>

#include <gnome.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>

#include "preferences.h"

/* Maximum number of mouse buttons we handle.  */
#define MAX_BUTTONS 10

/* Half the number of acceleration levels we support.  */
#define MAX_ACCEL 3

/* Maximum threshold we support.  */
#define MAX_THRESH 7

static GtkObjectClass *parent_class;

static void preferences_init             (Preferences *prefs);
static void preferences_class_init       (PreferencesClass *class);

static gint       xml_read_int           (xmlNodePtr node);
static xmlNodePtr xml_write_int          (gchar *name, 
					  gint number);
static gboolean   xml_read_bool          (xmlNodePtr node);
static xmlNodePtr xml_write_bool         (gchar *name,
					  gboolean value);

static gint apply_timeout_cb             (Preferences *prefs);

guint
preferences_get_type (void)
{
	static guint preferences_type = 0;

	if (!preferences_type) {
		GtkTypeInfo preferences_info = {
			"Preferences",
			sizeof (Preferences),
			sizeof (PreferencesClass),
			(GtkClassInitFunc) preferences_class_init,
			(GtkObjectInitFunc) preferences_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		preferences_type = 
			gtk_type_unique (gtk_object_get_type (), 
					 &preferences_info);
	}

	return preferences_type;
}

static void
preferences_init (Preferences *prefs)
{
	prefs->frozen = FALSE;

	/* Code to initialize preferences object to defaults */
}

static void
preferences_class_init (PreferencesClass *class) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	object_class->destroy = preferences_destroy;

	parent_class = 
		GTK_OBJECT_CLASS (gtk_type_class (gtk_object_get_type ()));
}

GtkObject *
preferences_new (void) 
{
	GtkObject *object;

	object = gtk_type_new (preferences_get_type ());

	return object;
}

GtkObject *
preferences_clone (Preferences *prefs)
{
	GtkObject *object;
	Preferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_PREFERENCES (prefs), NULL);

	object = preferences_new ();

	new_prefs = PREFERENCES (object);
	new_prefs->rtol = prefs->rtol;
	new_prefs->acceleration = prefs->acceleration;
	new_prefs->threshold = prefs->threshold;
	new_prefs->nbuttons = prefs->nbuttons;

	return object;
}

void
preferences_destroy (GtkObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	parent_class->destroy (object);
}

void
preferences_load (Preferences *prefs) 
{
        unsigned char buttons[MAX_BUTTONS];
        int acc_num, acc_den, thresh;
        gboolean rtol_default;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

        prefs->nbuttons = XGetPointerMapping (GDK_DISPLAY (), buttons,
					     MAX_BUTTONS);
        g_assert (prefs->nbuttons <= MAX_BUTTONS);

        /* Note that we only handle right-to-left and left-to-right.
           Most weird mappings are treated as l-to-r.
           We could handle this by showing the mouse buttons and letting the
           user drag-and-drop them to reorder.  But I'm not convinced this
           is worth it.  */
        /* FIXME: this ignores the fact that a mouse with the weird little
           roller generates B4 and B5 when the roller is moved.  That
           shouldn't change when we remap the other mouse buttons.  */
        prefs->rtol = gnome_config_get_bool_with_default
		("/Desktop/Mouse/right-to-left=false", &rtol_default);
        if (rtol_default)
                prefs->rtol = (buttons[prefs->nbuttons - 1] == 1);

        prefs->threshold = gnome_config_get_int ("/Desktop/Mouse/threshold=-1");
        prefs->acceleration = gnome_config_get_int 
		("/Desktop/Mouse/acceleration=-1");

        if (prefs->threshold == -1 || prefs->acceleration == -1) {
                XGetPointerControl (GDK_DISPLAY (), &acc_num,
				    &acc_den, &thresh);

                if (prefs->threshold == -1)
                        prefs->threshold = thresh;
                if (prefs->acceleration == -1) {
                        /* Only support cases in our range.  If neither the
			 * numerator nor denominator is 1, then rescale. */
                        if (acc_num != 1 && acc_den != 1) {
                                if (acc_num > acc_den) {
                                        acc_num = (int)
						((double) acc_num / acc_den);
                                        acc_den = 1;
                                } else {
                                        acc_den = (int)
						((double) acc_den / acc_num);
                                        acc_num = 1;
                                }
                        }

                        if (acc_num > MAX_ACCEL)
                                acc_num = MAX_ACCEL;
                        if (acc_den > MAX_ACCEL)
                                acc_den = MAX_ACCEL;
                        if (acc_den == 1)
                                prefs->acceleration = acc_num + MAX_ACCEL - 1;
                        else
                                prefs->acceleration = MAX_ACCEL - acc_den;
                }
        }
}

void 
preferences_save (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

        gnome_config_set_int ("/Desktop/Mouse/acceleration", 
			      prefs->acceleration);
        gnome_config_set_int ("/Desktop/Mouse/threshold", prefs->threshold);
        gnome_config_set_bool ("/Desktop/Mouse/right-to-left", prefs->rtol);

	gnome_config_sync ();
}

void
preferences_changed (Preferences *prefs) 
{
	if (prefs->frozen) return;

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	/* Live update in this case could be very problematic, so we're going
	 * to disable it */

/* 	preferences_apply_now (prefs); */
}

void
preferences_apply_now (Preferences *prefs)
{
        unsigned char buttons[MAX_BUTTONS], i;
        int num, den, max;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	prefs->timeout_id = 0;

        g_assert (prefs->nbuttons <= MAX_BUTTONS);

        /* Ignore buttons above 3 -- these are assumed to be a wheel. If we
	 * have a non-wheeled mouse, this may do weird things */

        XGetPointerMapping(GDK_DISPLAY (), buttons, MAX_BUTTONS);
        max = MIN (prefs->nbuttons, 3);
        for (i = 0; i < max; ++i)
                buttons[i] = prefs->rtol ? (max - i) : (i + 1);

        XSetPointerMapping (GDK_DISPLAY (), buttons, prefs->nbuttons);

        if (prefs->acceleration < MAX_ACCEL) {
		num = 1;
		den = MAX_ACCEL - prefs->acceleration;
	} else {
		num = prefs->acceleration - MAX_ACCEL + 1;
		den = 1;
	}

        XChangePointerControl (GDK_DISPLAY (), True, True, 
			       num, den, prefs->threshold);
}

void preferences_freeze (Preferences *prefs) 
{
	prefs->frozen++;
}

void preferences_thaw (Preferences *prefs) 
{
	if (prefs->frozen > 0) prefs->frozen--;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;

	prefs = PREFERENCES (preferences_new ());

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "mouse-properties"))
		return NULL;

	for (node = root_node->childs; node; node = node->next) {
                if (!strcmp (node->name, "acceleration"))
                        prefs->acceleration = xml_read_int (node);
                else if (!strcmp (node->name, "threshold"))
                        prefs->threshold = xml_read_int (node);
                else if (!strcmp (node->name, "right-to-left"))
                        prefs->rtol = xml_read_bool (node);
	}

	return prefs;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *tmp;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "mouse-properties", NULL);

        xmlAddChild (node, xml_write_int ("acceleration", 
					  prefs->acceleration));

        xmlAddChild (node, xml_write_int ("threshold", 
					  prefs->threshold));

	xmlAddChild (node, xml_write_bool ("right-to-left", prefs->rtol));

	xmlDocSetRootElement (doc, node);

	return doc;
}

/* Read a numeric value from a node */

static gint
xml_read_int (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (text == NULL) 
		return 0;
	else
		return atoi (text);
}

/* Write out a numeric value in a node */

static xmlNodePtr
xml_write_int (gchar *name, gint number) 
{
	xmlNodePtr node;
	gchar *str;

	g_return_val_if_fail (name != NULL, NULL);

	str = g_strdup_printf ("%d", number);
	node = xmlNewNode (NULL, name);
	xmlNodeSetContent (node, str);
	g_free (str);

	return node;
}

/* Read a boolean value from a node */

static gboolean
xml_read_bool (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (!g_strcasecmp (text, "true")) 
		return TRUE;
	else
		return FALSE;
}

/* Write out a boolean value in a node */

static xmlNodePtr
xml_write_bool (gchar *name, gboolean value) 
{
	xmlNodePtr node;

	g_return_val_if_fail (name != NULL, NULL);

	node = xmlNewNode (NULL, name);

	if (value)
		xmlNodeSetContent (node, "true");
	else
		xmlNodeSetContent (node, "false");

	return node;
}

static gint 
apply_timeout_cb (Preferences *prefs) 
{
	preferences_apply_now (prefs);

	return TRUE;
}
