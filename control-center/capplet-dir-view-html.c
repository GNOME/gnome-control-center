/* -*- mode: c; style: linux -*- */

/* capplet-dir-view-html.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Jacob Berkman <jacob@helixcode.com>
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

#include <config.h>
#include <gtkhtml/gtkhtml.h>
#include <gal/widgets/e-unicode.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include "capplet-dir-view.h"

#define INTRO_FILE INTRO_DIR"/intro.xml"

typedef struct {
	GtkHTML *top;
	GtkHTML *sidebar;
	GtkHTML *main;
	CappletDir *root_dir;
       	int icon_cols;
       	gboolean dont_update_main;	
       	gboolean showing_intro;	
} HtmlViewData;

static void html_view_intro_page (HtmlViewData *data);

static void
html_clear (CappletDirView *view)
{
	HtmlViewData *data;

	data = view->view_data;

	gtk_html_load_empty (data->top);
	gtk_html_load_empty (data->main);
}

static void
html_clean (CappletDirView *view)
{
	/* gtk_widget_unparent (view->view); */
	g_free (view->view_data);
	view->view_data = NULL;
}

#define BUFLEN 4096
static void
handle_url_cb (GtkHTML *html, const gchar *url, GtkHTMLStream *stream, CappletDirView *view)
{
	char buf[BUFLEN];
	int fd;
	ssize_t s;

	fd = open (url, O_RDONLY);
	if (fd == -1)
		goto loading_error;

	while (1) {
		s = read (fd, buf, BUFLEN);
		switch (s) {
		case -1:
			if (! (errno == EINTR || errno == EAGAIN) )
				goto loading_error;
			break;
		case 0:
			gtk_html_end (html, stream, GTK_HTML_STREAM_OK);
			return;
		default:
			gtk_html_write (html, stream, buf, s);
			break;
		}
	}

	return;

 loading_error:
	gtk_html_end (html, stream, GTK_HTML_STREAM_ERROR);
}
#undef BUFLEN

static void
handle_link_cb (GtkHTML *html, const gchar *url, CappletDirView *view)
{
	CappletDirEntry *entry;
	const gchar *launch = "launch:";
	int llen = strlen (launch);

	if (!strcmp (url, "!INTRO_PAGE!"))
	{
		html_view_intro_page (view->view_data);
		sidebar_populate (view);
	}
	else if ((strlen (url) > llen) && !strncmp (url, launch, llen))
	{
		const gchar *the_rest = url + llen;
		if (!strcmp (the_rest, "bug-buddy"))
		{
			char *argv[] = { "bug-buddy" };
			gnome_execute_async (NULL, 1, argv);
		}
		else
			gnome_url_show (the_rest);
	}
	else
	{
		entry = capplet_lookup (url);
		if (entry)
			capplet_dir_entry_activate (entry, view);
	}
}

static void
write_parent_html (CappletDir *dir, GtkHTML *html, GtkHTMLStream *stream)
{
	char *s, *utfs;

	if (!dir)
		return;

	g_return_if_fail (html != NULL);
	g_return_if_fail (stream != NULL);

	write_parent_html (dir->entry.dir, html, stream);

	utfs = e_utf8_from_locale_string (dir->entry.label);
	s = g_strdup_printf ("%s <a href=\"%s\">%s</a>", dir->entry.dir ? " |" : "", 
			     dir->entry.path, utfs);
	g_free (utfs);
	
	gtk_html_write (html, stream, s, strlen (s));
	g_free (s);	
}

static gboolean save_cb (gpointer engine, gchar *data, guint len, gpointer user_data)
{
	fprintf (user_data, data);
}

static void
header_populate (CappletDirView *view)
{
	GtkHTMLStream *stream;
	HtmlViewData *data;
	char *s, *utf_title, *utf_path;

	data = view->view_data;
	stream = gtk_html_begin (data->top);

	utf_title = e_utf8_from_locale_string (_("GNOME Control Center:"));
	utf_path = e_utf8_from_locale_string (CAPPLET_DIR_ENTRY (view->capplet_dir)->label);
	
	s = g_strdup_printf (
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"
"<html>"
"<head>"
"</head>"
"<body background=\"" ART_DIR "/bcg_top.png\" marginheight=\"0\" marginwidth=\"0\">"
"<table border=\"0\" width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr valign=\"center\"><td width=\"48\"><a href=\"!INTRO_PAGE!\"><img src=\"" ART_DIR "/title.png\" alt=\"\" width=\"48\" height=\"48\" border=\"0\"></a></td>"
"<td><b><font face=\"Trebuchet MS CE,Trebuchet MS, Verdana CE, Verdana, Sans-Serif CE, Sans-Serif\" color=\"white\" size=\"+2\">%s&nbsp;&nbsp;&nbsp;</font></b><font face=\"Trebuchet MS CE,Trebuchet MS, Verdana CE, Verdana, Sans-Serif CE, Sans-Serif\" color=\"white\" align=\"left\" valign=\"center\">%s</font></td></tr></table>"
"</body></html>", utf_title, utf_path);
	g_free (utf_title);
	g_free (utf_path);
	
	gtk_html_write (data->top, stream, s, strlen (s));
	g_free (s);

	gtk_html_end (data->top, stream, GTK_HTML_STREAM_OK);
}

static int
total_num_items (GSList *items)
{
	GSList *l;
	CappletDirEntry *entry;
	int num = 0;

	for (l = items; l != NULL; l = l->next)
	{
		entry = CAPPLET_DIR_ENTRY (l->data);
		if (entry->type == TYPE_CAPPLET_DIR)
		{
			num += total_num_items (CAPPLET_DIR (entry)->entries);
			continue;
		}	
		else
			num++;
	}

	return num;
}

static void
sidebar_populate (CappletDirView *view)
{
	GtkHTMLStream *stream;
	HtmlViewData *data;
	CappletDirEntry *entry;
	GSList *item;
	char *s, *utfs;

	data = view->view_data;
	
	stream = gtk_html_begin (data->sidebar);

	utfs = e_utf8_from_locale_string (CAPPLET_DIR_ENTRY (data->root_dir)->label);
	s = g_strdup_printf (
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"
"<html>"
"<head>"
"</head>"
//"<body bgcolor=\"#AAAAAA\" marginheight=\"0\" marginwidth=\"0\">"
"<body bgcolor=\"#d9d9d9\" marginheight=\"0\" marginwidth=\"0\">"
"<table border=\"0\" width=\"100%%\" cellspacing=\"1\" cellpadding=\"4\">"
"<tr><td colspan=\"3\">&nbsp;</td></tr>"
"<tr valign=\"center\"><td width=\"48\"><a href=\"%s\"><img src=\"%s\" alt=\"\" border=\"0\" align=\"center\"/></a></td><td><a href=\"%s\"><b>%s</b></a></td><td width=\"8\"><img src=\"%s\" alt=\"\" border=\"0\" align=\"center\"></tr>", CAPPLET_DIR_ENTRY (data->root_dir)->path, CAPPLET_DIR_ENTRY (data->root_dir)->icon, CAPPLET_DIR_ENTRY (data->root_dir)->path, utfs, (data->root_dir == view->capplet_dir && !data->showing_intro) ? ART_DIR "/active.png" : ART_DIR "/blank.png");
	g_free (utfs);
	gtk_html_write (data->sidebar, stream, s, strlen (s));
	g_free (s);

	for (item = data->root_dir->entries; item; item = item->next) {
		entry = CAPPLET_DIR_ENTRY (item->data);
		if (entry->type != TYPE_CAPPLET_DIR)
			continue;
		if (total_num_items (CAPPLET_DIR (entry)->entries) < 1)
			continue;

		utfs = e_utf8_from_locale_string (entry->label);
		
		s = g_strdup_printf ("<tr valign=\"center\"><td width=\"48\"><a href=\"%s\"><img src=\"%s\" alt=\"\" border=\"0\" align=\"center\"/></a></td><td><a href=\"%s\"><b>%s</b></a></td><td width=\"8\"><img src=\"%s\" alt=\"\" border=\"0\" align=\"center\"></tr>", entry->path, entry->icon, entry->path, utfs, (CAPPLET_DIR (entry) == view->capplet_dir && !data->showing_intro) ? ART_DIR "/active.png" : ART_DIR "/blank.png");
		g_free (utfs);
		gtk_html_write (data->sidebar, stream, s, strlen (s));
		g_free (s);
	}
	
	s = 
"</table></html>";
	gtk_html_write (data->sidebar, stream, s, strlen (s));
}

/* Write a row of up to 4 items and return a pointer to the next 4 */
static GSList*
html_write_row (GtkHTML *html, GtkHTMLStream *stream, GSList *list, int the_max)
{
	CappletDirEntry *entry;
	int i;
	char *s, *utfs;
	GSList *item;
	
	g_return_val_if_fail (list != NULL, NULL);

	s = g_strdup_printf ("<tr><td colspan=\"%i\"><img src=\"" ART_DIR "/blank.png\" height=\"16\" width=\"100%%\"></td></tr><tr><td><img src=\"" ART_DIR "/blank.png\" height=\"1\" width=\"8\"></td>", the_max * 2 + 1);
	gtk_html_write (html, stream, s, strlen (s));
	g_free (s);

	i = 0;
	for (item = list; item; item = item->next) {
		entry = CAPPLET_DIR_ENTRY (item->data);
		if (entry->type != TYPE_CAPPLET)
			continue;
		s = g_strdup_printf ("<td><center><a href=\"%s\"><img src=\"%s\" alt=\"\" border=\"0\" align=\"center\"/></a></center></td><td><img src=\"" ART_DIR "/blank.png\" height=\"1\" width=\"8\"></td>", entry->path, entry->icon);
		gtk_html_write (html, stream, s, strlen (s));
		g_free (s);

		/* Control. Can't go in for-decl because it would inc in all
		 * cases, including the skipped directories. */
		i++;
		if (!(i < the_max))
			break;
	}

	s = "</tr><tr><td><img src=\"" ART_DIR "/blank.png\" height=\"1\" width=\"8\"></td>";
	gtk_html_write (html, stream, s, strlen (s));
	
	i = 0;	
	for (item = list; item; item = item->next) {
		entry = CAPPLET_DIR_ENTRY (item->data);
		if (entry->type != TYPE_CAPPLET)
			continue;
		utfs = e_utf8_from_locale_string (entry->label);
		s = g_strdup_printf ("<td><center><a href=\"%s\">%s</a></center></td><td><img src=\"" ART_DIR "/blank.png\" height=\"1\" width=\"8\"></td>", entry->path, utfs);
		g_free (utfs);
		gtk_html_write (html, stream, s, strlen (s));
		g_free (s);

		i++;
		if (!(i < the_max))
		{
			item = item->next;
			break;
		}
	}

	s = "tr>\n";
	gtk_html_write (html, stream, s, strlen (s));

	return item;
}

static void
html_populate (CappletDirView *view)
{
	HtmlViewData *data;
	GtkHTMLStream *stream;
	GSList *item;
	char *s;

	data = view->view_data;
	if (!data->root_dir)
		data->root_dir = view->capplet_dir;

	if (!data->dont_update_main)
		data->showing_intro = FALSE;

	header_populate (view);
	sidebar_populate (view);
	
	if (data->dont_update_main)
		return;

	stream = gtk_html_begin (data->main);

	s = 
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"
"<html>"
"<head>"
"</head>"
"<body marginheight=\"0\" marginwidth=\"0\">"
"<table border=\"0\" width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\">";
	gtk_html_write (data->main, stream, s, strlen (s));

	/* write_parent_html (view->capplet_dir->entry.dir, data->main, stream); */

	for (item = view->capplet_dir->entries; item; )
	{
		item = html_write_row (data->main, stream, item, data->icon_cols);
	}

	s = g_strdup_printf ("<tr><td colspan=\"%i\"><img src=\"" ART_DIR "/blank.png\" height=\"16\" width=\"100%%\"></td></tr>", data->icon_cols * 2 + 1);
	gtk_html_write (data->main, stream, s, strlen (s));
	g_free (s);

	s =
"    </table>\n"
"  </body>\n"
"</html>\n";
	gtk_html_write (data->main, stream, s, strlen (s));

	gtk_html_end (data->main, stream, GTK_HTML_STREAM_OK);
//	gtk_widget_set_usize (GTK_WIDGET (data->main), html_engine_calc_min_width (data->main->engine), 0);
//	g_print ("%i\n", html_engine_calc_min_width (data->main->engine));
}

static void
parse_node_recursively (xmlNodePtr node, gchar *root_dir)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (root_dir != NULL);
	
	while (node)
	{
		if (!strcmp (node->name, "task"))
		{
			gboolean valid = TRUE;
			xmlChar *dentry = xmlGetProp (node, "href");
			if (dentry)
			{
				gchar *filename = g_concat_dir_and_file (root_dir, dentry);
				if (g_file_exists (filename))
					xmlSetProp (node, "href", filename);
				else
					valid = FALSE;
	
				g_free (filename);
				g_free (dentry);
			}
			else
				valid = FALSE;
			
			if (valid)
				xmlNodeSetName (node, "a");
			else
			{
				/* RemoveNode not implemented, turn to plan b */
				xmlNodeSetName (node, "invalid");
				if (node->childs)
				{
					xmlFreeNodeList (node->childs);
					node->childs = NULL;
				}
				xmlNodeSetContent (node, "");
			}
		}
		if (!strcmp (node->name, "img"))
		{
			xmlChar *src = xmlGetProp (node, "src");
			
			if (src && (src[0] != '/'))
			{
				gchar *filename = g_concat_dir_and_file (ART_DIR, src);
				xmlSetProp (node, "src", filename);
				g_free (filename);
			}

			xmlFree (src);
		}
		else if (node->childs)
		{
			parse_node_recursively (node->childs, root_dir);
		}

		node = node->next;
	}
}

static xmlChar*
load_intro_page (int *len)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	gchar *root_dir;
	xmlChar *ret;
	
	g_return_val_if_fail (len != NULL, NULL);
	
	doc = xmlParseFile (INTRO_FILE);
	if (!doc)
		return NULL;
	
	node = xmlDocGetRootElement (doc);
	if (!node)
	{
		xmlFreeDoc (doc);
		return NULL;
	}

	root_dir = (get_root_capplet_dir ())->entry.path;
	
	parse_node_recursively (node, root_dir);
	xmlDocDumpMemory (doc, &ret, len);
	return ret;
}

static void
html_view_intro_page (HtmlViewData *data)
{
	GtkHTMLStream *stream;
	xmlChar *s;
	int len;

	s = load_intro_page (&len);
	
	stream = gtk_html_begin (data->main);

	gtk_html_write (data->main, stream, s, len);

	gtk_html_end (data->main, stream, GTK_HTML_STREAM_OK);

	xmlFree (s);

	data->showing_intro = TRUE;
}

static void
main_allocate_cb (GtkWidget *widget, GtkAllocation *allocation, CappletDirView *view)
{
	int new_cols = allocation->width / 64 - 1;
	HtmlViewData *data = view->view_data;
	if (new_cols != data->icon_cols)
	{
		data->icon_cols = new_cols;
		html_view_intro_page (data);
		
		data->dont_update_main = TRUE;
		html_populate (view);
		data->dont_update_main = FALSE;
	}
}

static GtkWidget *
html_create (CappletDirView *view)
{	
	GtkWidget *vbox;
	GtkWidget *hbox;
	HtmlViewData *data;
	GtkWidget *sw;

	data = g_new (HtmlViewData, 1);
	view->view_data = data;

	data->root_dir = NULL;
	data->icon_cols = 3;
	data->dont_update_main = FALSE;	
	data->showing_intro = TRUE;	

	vbox = gtk_vbox_new (FALSE, 0);
	/* top widget */
	data->top = GTK_HTML (gtk_html_new ());
	gtk_html_set_default_content_type (GTK_HTML (data->top),
					   "text/html; charset=utf-8");
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (data->top));
	gtk_widget_set_usize (GTK_WIDGET (data->top), 0, 48);
	gtk_box_pack_start (GTK_BOX (vbox), sw, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	/* sidebar */
	data->sidebar = GTK_HTML (gtk_html_new ());
	gtk_html_set_default_content_type (GTK_HTML (data->sidebar),
					   "text/html; charset=utf-8");
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (data->sidebar));
	gtk_widget_set_usize (GTK_WIDGET (data->sidebar), 140, 0);
	gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);

	/* main widget */
	data->main = GTK_HTML (gtk_html_new ());
	gtk_html_set_default_content_type (GTK_HTML (data->main),
					   "text/html; charset=utf-8");
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_ALWAYS);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (data->main));
	gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);
			  
	gtk_signal_connect (GTK_OBJECT (data->top), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->sidebar), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->main), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->top), "link_clicked",
			    GTK_SIGNAL_FUNC (handle_link_cb), view);
	
	gtk_signal_connect (GTK_OBJECT (data->sidebar), "link_clicked",
			    GTK_SIGNAL_FUNC (handle_link_cb), view);
	
	gtk_signal_connect (GTK_OBJECT (data->main), "link_clicked",
			    GTK_SIGNAL_FUNC (handle_link_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->main), "size_allocate",
			    GTK_SIGNAL_FUNC (main_allocate_cb), view);

	gtk_html_allow_selection (data->top, FALSE);
	gtk_html_allow_selection (data->sidebar, FALSE);
	gtk_html_allow_selection (data->main, FALSE);

	gtk_widget_show_all (vbox);
	return vbox;
}

CappletDirViewImpl capplet_dir_view_html = {
	html_clear,
	html_clean,
	html_populate,
	html_create
};
