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

	    if (!xmlStrcasecmp (cont, "true") || !xmlStrcasecmp (cont, "1"))
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
    xmlNode *element;
    gchar *ret_val = NULL;
    xmlChar *xml_val_name;
    gint len;

    g_return_val_if_fail (parent != NULL, ret_val);
    g_return_val_if_fail (parent->children != NULL, ret_val);
    g_return_val_if_fail (val_name != NULL, ret_val);

    xml_val_name = xmlCharStrdup (val_name);
    len = xmlStrlen (xml_val_name);

    for (element = parent->children; element != NULL; element = element->next) {
	if (!xmlStrncmp (element->name, xml_val_name, len))
	    ret_val = (gchar *) xmlNodeGetContent (element);
    }

    xmlFree (xml_val_name);
    return ret_val;
}

static gboolean
is_executable_valid (gchar *executable)
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
    xmlNode *root, *section, *element, *value;
    GnomeDAWebItem *web_item;
    GnomeDAMailItem *mail_item;
    GnomeDATermItem *term_item;

    gint i;

    xml_doc = xmlParseFile (filename);

    if (!xml_doc)
	return;

    root = xmlDocGetRootElement (xml_doc);

    for (section = root->children; section != NULL; section = section->next) {
	if (!xmlStrncmp (section->name, "web-browsers", 12)) {
	    for (element = section->children; element != NULL; element = element->next) {
		if (!xmlStrncmp (element->name, "web-browser", 11)) {
		    if (is_executable_valid (gnome_da_xml_get_string (element, "executable"))) {
			web_item = gnome_da_web_item_new ();

			web_item->generic.name = gnome_da_xml_get_string (element, "name");
			web_item->generic.executable = gnome_da_xml_get_string (element, "executable");
			web_item->generic.command = gnome_da_xml_get_string (element, "command");
			web_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			web_item->run_in_terminal = gnome_da_xml_get_bool (element, "run-in-terminal");
			web_item->netscape_remote = gnome_da_xml_get_bool (element, "netscape-remote");
			if (web_item->netscape_remote) {
			    web_item->tab_command = gnome_da_xml_get_string (element, "tab-command");
			    web_item->win_command = gnome_da_xml_get_string (element, "win-command");
			}

			capplet->web_browsers = g_list_append (capplet->web_browsers, web_item);
		    }
		}
	    }
	}
	else if (!xmlStrncmp (section->name, "mail-readers", 12)) {
	    for (element = section->children; element != NULL; element = element->next) {
		if (!xmlStrncmp (element->name, "mail-reader", 11)) {
		    if (is_executable_valid (gnome_da_xml_get_string (element, "executable"))) {
			mail_item = gnome_da_mail_item_new ();

			mail_item->generic.name = gnome_da_xml_get_string (element, "name");
			mail_item->generic.executable = gnome_da_xml_get_string (element, "executable");
			mail_item->generic.command = gnome_da_xml_get_string (element, "command");
			mail_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			mail_item->run_in_terminal = gnome_da_xml_get_bool (element, "run-in-terminal");

			capplet->mail_readers = g_list_append (capplet->mail_readers, mail_item);
		    }
		}
	    }
	}
	else if (!xmlStrncmp (section->name, "terminals", 9)) {
	    for (element = section->children; element != NULL; element = element->next) {
		if (!xmlStrncmp (element->name, "terminal", 8)) {
		    if (is_executable_valid (gnome_da_xml_get_string (element, "executable"))) {
			term_item = gnome_da_term_item_new ();

			term_item->generic.name = gnome_da_xml_get_string (element, "name");
			term_item->generic.executable = gnome_da_xml_get_string (element, "executable");
			term_item->generic.command = gnome_da_xml_get_string (element, "command");
			term_item->generic.icon_name = gnome_da_xml_get_string (element, "icon-name");

			term_item->exec_flag = gnome_da_xml_get_string (element, "exec-flag");

			capplet->terminals = g_list_append (capplet->terminals, term_item);
		    }
		}
	    }
	}
    }

    xmlFreeDoc (xml_doc);
}

void
gnome_da_xml_load_list (GnomeDACapplet *capplet)
{
    const char * const *xdg_dirs;
    gint i;

    xdg_dirs = g_get_system_data_dirs ();
    if (xdg_dirs == NULL)
	return;

    for (i = 0; i < G_N_ELEMENTS (xdg_dirs); i++) {
	gchar *filename;

	filename = g_build_filename (xdg_dirs[i],
				     "gnome-default-applications",
				     "gnome-default-applications.xml",
				     NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS))
	    gnome_da_xml_load_xml (capplet, filename);

	g_free (filename);
    }

    if (capplet->web_browsers == NULL)
	gnome_da_xml_load_xml (capplet, "./gnome-default-applications.xml");
}
