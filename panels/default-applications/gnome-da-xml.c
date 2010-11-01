/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libxml/parser.h>

#include "gnome-da-capplet.h"
#include "gnome-da-xml.h"
#include "gnome-da-item.h"


static gboolean
gnome_da_xml_get_bool (const xmlNode *parent, const gchar *val_name)
{
    xmlNode *element;
    gboolean ret_val = FALSE;
    xmlChar *xml_val_name;
    gint len;

    g_return_val_if_fail (parent != NULL, FALSE);
    g_return_val_if_fail (parent->children != NULL, ret_val);
    g_return_val_if_fail (val_name != NULL, FALSE);

    xml_val_name = xmlCharStrdup (val_name);
    len = xmlStrlen (xml_val_name);

    for (element = parent->children; element != NULL; element = element->next) {
	if (!xmlStrncmp (element->name, xml_val_name, len)) {
	    xmlChar *cont = xmlNodeGetContent (element);

	    if (!xmlStrcasecmp (cont, (const xmlChar *) "true") || !xmlStrcasecmp (cont, (const xmlChar *) "1"))
		ret_val = TRUE;
	    else
		ret_val = FALSE;

	    xmlFree (cont);
	}
    }

    xmlFree (xml_val_name);
    return ret_val;
}

static gchar*
gnome_da_xml_get_string (const xmlNode *parent, const gchar *val_name)
{
    const gchar * const *sys_langs;
    xmlChar *node_lang;
    xmlNode *element;
    gchar *ret_val = NULL;
    xmlChar *xml_val_name;
    gint len;
    gint i;

    g_return_val_if_fail (parent != NULL, ret_val);
    g_return_val_if_fail (parent->children != NULL, ret_val);
    g_return_val_if_fail (val_name != NULL, ret_val);

#if GLIB_CHECK_VERSION (2, 6, 0)
    sys_langs = g_get_language_names ();
#endif

    xml_val_name = xmlCharStrdup (val_name);
    len = xmlStrlen (xml_val_name);

    for (element = parent->children; element != NULL; element = element->next) {
	if (!xmlStrncmp (element->name, xml_val_name, len)) {
	    node_lang = xmlNodeGetLang (element);

	    if (node_lang == NULL) {
		ret_val = (gchar *) xmlNodeGetContent (element);
	    }
	    else {
		for (i = 0; sys_langs[i] != NULL; i++) {
			if (!strcmp ((const char *) sys_langs[i], (const char *) node_lang)) {
			ret_val = (gchar *) xmlNodeGetContent (element);
			/* since sys_langs is sorted from most desirable to
			 * least desirable, exit at first match
			 */
			break;
		    }
		}
	    }
	    xmlFree (node_lang);
	}
    }

    xmlFree (xml_val_name);
    return ret_val;
}

static gboolean
is_executable_valid (const gchar *executable)
{
    gchar *path;

    path = g_find_program_in_path (executable);

    if (path) {
	g_free (path);
	return TRUE;
    }

    return FALSE;
}

static void
gnome_da_xml_load_xml (GnomeDACapplet *capplet, const gchar * filename)
{
    xmlDoc *xml_doc;
    xmlNode *root, *section, *element;
    gchar *executable;
    GnomeDATermItem *term_item;
    GnomeDAVisualItem *visual_item;
    GnomeDAMobilityItem *mobility_item;

    xml_doc = xmlParseFile (filename);

    if (!xml_doc)
	return;

    root = xmlDocGetRootElement (xml_doc);

    for (section = root->children; section != NULL; section = section->next) {
	 if (!xmlStrncmp (section->name, (const xmlChar *) "terminals", 9)) {
	     for (element = section->children; element != NULL; element = element->next) {
		 if (!xmlStrncmp (element->name, (const xmlChar *) "terminal", 8)) {
		    executable = gnome_da_xml_get_string (element, "executable");
		    if (is_executable_valid (executable)) {
			term_item = gnome_da_term_item_new ();

			term_item->generic.name = gnome_da_xml_get_string (element, "name");
			term_item->generic.executable = executable;
			term_item->generic.command = gnome_da_xml_get_string (element, "command");
			term_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			term_item->exec_flag = gnome_da_xml_get_string (element, "exec-flag");

			capplet->terminals = g_list_append (capplet->terminals, term_item);
		    }
		    else
			g_free (executable);
		 }
	    }
	 }
	 else if (!xmlStrncmp (section->name, (const xmlChar *) "a11y-visual", 11)) {
	    for (element = section->children; element != NULL; element = element->next) {
		 if (!xmlStrncmp (element->name, (const xmlChar *) "visual", 6)) {
		    executable = gnome_da_xml_get_string (element,"executable");
		    if (is_executable_valid (executable)) {
			visual_item = gnome_da_visual_item_new ();

			visual_item->generic.name = gnome_da_xml_get_string (element, "name");
			visual_item->generic.executable = executable;
			visual_item->generic.command = gnome_da_xml_get_string (element, "command");
			visual_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			visual_item->run_at_startup = gnome_da_xml_get_bool (element, "run-at-startup");

			capplet->visual_ats = g_list_append (capplet->visual_ats, visual_item);
		    }
                    else
                        g_free (executable);
		}
	    }
	 }
	 else if (!xmlStrncmp (section->name, (const xmlChar *) "a11y-mobility", 13)) {
	    for (element = section->children; element != NULL; element = element->next) {
	        if (!xmlStrncmp (element->name, (const xmlChar *) "mobility", 8)) {
		    executable = gnome_da_xml_get_string (element,"executable");
		    if (is_executable_valid (executable)) {
			mobility_item = gnome_da_mobility_item_new ();

			mobility_item->generic.name = gnome_da_xml_get_string (element, "name");
			mobility_item->generic.executable = executable;
			mobility_item->generic.command = gnome_da_xml_get_string (element, "command");
			mobility_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			mobility_item->run_at_startup = gnome_da_xml_get_bool (element, "run-at-startup");

			capplet->mobility_ats = g_list_append (capplet->mobility_ats, mobility_item);
		    }
                    else
                        g_free (executable);
		}
	    }
	 }
    }

    xmlFreeDoc (xml_doc);
}

static GList *
load_url_handlers (GnomeDACapplet *capplet, const gchar *scheme)
{
    GList *app_list, *l, *ret;

    app_list = g_app_info_get_all_for_type (scheme);
    ret = NULL;

    for (l = app_list; l != NULL; l = l->next) {
        const gchar *executable;
        GAppInfo *app_info = l->data;

	executable = g_app_info_get_executable (app_info);
	if (is_executable_valid (executable)) {
            GnomeDAURLItem *url_item;

	    url_item = gnome_da_url_item_new ();
	    url_item->generic.name = g_strdup (g_app_info_get_display_name (app_info));
	    url_item->generic.executable = g_strdup (executable);
	    url_item->generic.command = g_strdup (g_app_info_get_commandline (app_info));
	    url_item->generic.icon_name = g_strdup (g_app_info_get_name (app_info));
	    /* Steal the reference */
	    url_item->app_info = app_info;

	    ret = g_list_prepend (ret, url_item);
	} else {
	    g_object_unref (app_info);
	}
    }
    g_list_free (app_list);

    return g_list_reverse (ret);
}

void
gnome_da_xml_load_list (GnomeDACapplet *capplet)
{
    GDir *app_dir = g_dir_open (GNOMECC_APPS_DIR, 0, NULL);

    /* First load all applications from the XML files */
    if (app_dir != NULL) {
        const gchar *extra_file;
        gchar *filename;

        while ((extra_file = g_dir_read_name (app_dir)) != NULL) {
            filename = g_build_filename (GNOMECC_APPS_DIR, extra_file, NULL);

            if (g_str_has_suffix (filename, ".xml"))
                gnome_da_xml_load_xml (capplet, filename);

            g_free (filename);
        }
        g_dir_close (app_dir);
    }

    /* Now load URL handlers */
    capplet->web_browsers = load_url_handlers (capplet, "x-scheme-handler/http");
    capplet->mail_readers = load_url_handlers (capplet, "x-scheme-handler/mailto");
}

void
gnome_da_xml_free (GnomeDACapplet *capplet)
{
    g_list_foreach (capplet->web_browsers, (GFunc) gnome_da_url_item_free, NULL);
    g_list_foreach (capplet->mail_readers, (GFunc) gnome_da_url_item_free, NULL);
    g_list_foreach (capplet->terminals, (GFunc) gnome_da_term_item_free, NULL);
    g_list_foreach (capplet->visual_ats, (GFunc) gnome_da_visual_item_free, NULL);
    g_list_foreach (capplet->mobility_ats, (GFunc) gnome_da_mobility_item_free, NULL);

    g_list_free (capplet->web_browsers);
    g_list_free (capplet->mail_readers);
    g_list_free (capplet->terminals);
    g_list_free (capplet->visual_ats);
    g_list_free (capplet->mobility_ats);

    g_object_unref (capplet->builder);
    g_free (capplet);
}
