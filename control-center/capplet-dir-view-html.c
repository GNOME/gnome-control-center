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

static void
html_clear (CappletDirView *view)
{
	g_return_if_fail (GTK_IS_HTML (view->view));

	gtk_html_load_empty (GTK_HTML (view->view));
}

static void
html_clean (CappletDirView *view)
{
	g_return_if_fail (GTK_IS_HTML (view->view));
	
	gtk_object_destroy (GTK_OBJECT (view->view));
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
html_populate (CappletDirView *view)
{
	GtkHTMLStream *stream;
	CappletDirEntry *entry;
	GSList *item;
	int i;
	char *s;

	g_return_if_fail (GTK_IS_HTML (view->view));

	stream = gtk_html_begin (GTK_HTML (view->view));

	s =
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
"<html>\n"
"  <head>\n"
"    <title>GNOME Control Center</title>\n"
"  </head>\n"
"  <body marginheight=\"0\" marginwidth=\"0\">\n"
#if 0
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
"      <tr height=\"64\">\n"
"        <td colspan=\"4\">\n"
"          Select something you want to customize.\n"
"        </td>\n"
"      <\tr>\n"
"      <tr>\n"
"        <td colspan=\"4\">\n"
"          &nbsp;\n";

	gtk_html_write (GTK_HTML (view->view), stream, s, strlen (s));

	write_parent_html (view->capplet_dir->entry.dir, GTK_HTML (view->view), stream);

	for (i = 0, item = view->capplet_dir->entries; item; item = item->next, i++) {
		if (!(i%2)) {
			s = "      <tr>\n";
			gtk_html_write (GTK_HTML (view->view), stream, s, strlen (s));
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

		gtk_html_write (GTK_HTML (view->view), stream, s, strlen (s));
		g_free (s);

		if (i%2 || !item->next) {
			s = "      </tr>\n";
			gtk_html_write (GTK_HTML (view->view), stream, s, strlen (s));
		}
	}

	s =
"    </table>\n"
"  </body>\n"
"</html>\n";
	gtk_html_write (GTK_HTML (view->view), stream, s, strlen (s));

	gtk_html_end (GTK_HTML (view->view), stream, GTK_HTML_STREAM_OK);
}

static GtkWidget *
html_create (CappletDirView *view)
{
	GtkWidget *w = gtk_html_new ();

	gtk_signal_connect (GTK_OBJECT (w), "url_requested",
			    GTK_SIGNAL_FUNC (handle_url_cb), view);

	gtk_signal_connect (GTK_OBJECT (w), "link_clicked",
			    GTK_SIGNAL_FUNC (handle_link_cb), view);

	return w;
}

CappletDirViewImpl capplet_dir_view_html = {
	html_clear,
	html_clean,
	html_populate,
	html_create
};

