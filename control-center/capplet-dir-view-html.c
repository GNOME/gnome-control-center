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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "capplet-dir-view.h"

typedef struct {
	GtkHTML *top;
	GtkHTML *main;
} HtmlViewData;

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

	g_print ("activating: %s\n", url);

	entry = capplet_lookup (url);
	if (entry)
		capplet_dir_entry_activate (entry, view);
}

static void
write_parent_html (CappletDir *dir, GtkHTML *html, GtkHTMLStream *stream)
{
	char *s;

	if (!dir)
		return;

	g_return_if_fail (html != NULL);
	g_return_if_fail (stream != NULL);

	write_parent_html (dir->entry.dir, html, stream);

	s = g_strdup_printf ("%s <a href=\"%s\">%s</a>", dir->entry.dir ? " |" : "", 
			     dir->entry.path, dir->entry.label);
	gtk_html_write (html, stream, s, strlen (s));
	g_free (s);	
}

static void
header_populate (CappletDirView *view)
{
	GtkHTMLStream *stream;
	HtmlViewData *data;
	char *s;

	data = view->view_data;
	stream = gtk_html_begin (data->top);

#warning this should probably be loaded from a file yo
	s = 
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
"<html>\n"
"<head></head>\n"
"<body marginwidth=\"0\" marginheight=\"0\" background=\""ART_DIR"/bgtop.png\">\n"
"<img src=\""ART_DIR"/left_top.png\" alt=\"\" width=\"47\" height=\"139\" />\n"
"<img src=\""ART_DIR"/empty.png\" alt=\"\" width=\"5\" height=\"110\" />\n"
"<img src=\""ART_DIR"/foot.png\" alt=\"Gnome\" />\n"
"<img src=\""ART_DIR"/empty.png\" alt=\"\" width=\"5\" height=\"110\" />\n"
"<FONT face=\"Trebuchet MS CE,Trebuchet MS, Verdana CE, Verdana, Sans-Serif CE, Sans-Serif\" size=\"6\" color=\"white\">Your Gnome</font>\n"
#if 0
"<img src=\""ART_DIR"/yourgnome.png\" alt=\"\" />\n"
#endif
"</body>\n"
"</html>";

	gtk_html_write (data->top, stream, s, strlen (s));
	gtk_html_end (data->top, stream, GTK_HTML_STREAM_OK);
	gtk_widget_set_usize (GTK_WIDGET (data->top), 0, 139);
}

static void
html_populate (CappletDirView *view)
{
	HtmlViewData *data;
	GtkHTMLStream *stream;
	CappletDirEntry *entry;
	GSList *item;
	int i;
	char *s;

	header_populate (view);

	data = view->view_data;
	stream = gtk_html_begin (data->main);

	s =
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
"<html>\n"
"  <head>\n"
"    <title>GNOME Control Center</title>\n"
"  </head>\n"
"  <body marginheight=\"0\" marginwidth=\"0\" background=\""ART_DIR"/bg.png\">\n"
"    <img src=\""ART_DIR"/left.png\" align=\"left\" alt=\"\">\n"
"    <p align=\"center\">\n"
#if 0
"    <img src=\""ART_DIR"/empty.png\" alt=\"\" width=\"600\" height=\"30\" />\n"
"    <table bgcolor=\"#292928\" width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" columns=\"2\">\n"
"      <tr>\n"
"        <td>\n"
"          <img src=\"%s\" height=\"24\" width=\"24\"></td>\n"
"        <td align=\"right\">\n"
"          <font size=\"+3\" color=\"#757575\"><b>%s</b></font>\n"
"        </td>\n"
"      </tr>\n"
"    </table>\n"
#endif
"    <table width=\"100%%\" columns=\"4\" cellpadding=\"4\" cellspacing=\"0\" border=\"0\">\n"
#if 0
"      <tr height=\"64\">\n"
"        <td colspan=\"4\">\n"
"          Select something you want to customize.\n"
"        </td>\n"
"      <\tr>\n"
#endif
"      <tr>\n"
"        <td colspan=\"4\">\n"
"          &nbsp;\n";

	gtk_html_write (data->main, stream, s, strlen (s));

	/* write_parent_html (view->capplet_dir->entry.dir, data->main, stream); */

	for (i = 0, item = view->capplet_dir->entries; item; item = item->next, i++) {
		if (!(i%2)) {
			s = "      <tr>\n";
			gtk_html_write (data->main, stream, s, strlen (s));
		}

		entry = CAPPLET_DIR_ENTRY (item->data);
		s = g_strdup_printf (
"        <td width=\"36\">\n"
"          <a href=\"%s\"><img src=\"%s\" border=\"0\" height=\"36\" width=\"36\"></a></td>\n"
"        <td width=\"%%50\">\n"
"          <font size=\"+1\"><b><a href=\"%s\">%s</a></b></font><br>\n"
"          <font size=\"-1\">%s</font>\n"
"        </td>\n"
, entry->path, entry->icon, entry->path, entry->label, entry->entry->comment);

		gtk_html_write (data->main, stream, s, strlen (s));
		g_free (s);

		if (i%2 || !item->next) {
			s = "      </tr>\n";
			gtk_html_write (data->main, stream, s, strlen (s));
		}
	}

	s =
"    </table>\n"
"    </p>\n"
"  </body>\n"
"</html>\n";
	gtk_html_write (data->main, stream, s, strlen (s));

	gtk_html_end (data->main, stream, GTK_HTML_STREAM_OK);
}

static GtkWidget *
html_create (CappletDirView *view)
{	
	GtkWidget *vbox;
	HtmlViewData *data;
	GtkWidget *sw;

	data = g_new (HtmlViewData, 1);
	view->view_data = data;

	vbox = gtk_vbox_new (FALSE, 0);

	/* top widget */
	data->top = GTK_HTML (gtk_html_new ());

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (data->top));
	gtk_box_pack_start (GTK_BOX (vbox), sw, FALSE, FALSE, 0);

	/* main widget */
	data->main = GTK_HTML (gtk_html_new ());
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (data->main));
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
			    
	gtk_signal_connect (GTK_OBJECT (data->top), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->main), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (data->main), "link_clicked",
			    GTK_SIGNAL_FUNC (handle_link_cb), view);

	gtk_widget_show_all (vbox);
	return vbox;
}

CappletDirViewImpl capplet_dir_view_html = {
	html_clear,
	html_clean,
	html_populate,
	html_create
};
