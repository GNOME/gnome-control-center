/* -*- mode: c; style: linux -*- */

/* preview.c
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <X11/Xatom.h>

#include "preview.h"
#include "rc-parse.h"      /* For get_screensaver_dir_list () */

static GtkWidget *preview_window;
static pid_t preview_pid;
static int timeout_id;
static int expose_id = 0;
static GdkPixbuf *pixbuf = NULL;

#if 0

/* DEBUG FUNCTION */
static void
print_args (char **args) 
{
	int i;

	printf ("Command line:\n");
	for (i = 0; args[i]; i++) {
		printf ("%s ", args[i]);
	}
	printf ("\n");
}

#endif

static void
expose_func (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	if (!pixbuf)
		return;
	
	gdk_pixbuf_render_to_drawable
		(pixbuf, (GdkDrawable *) preview_window->window,
		 preview_window->style->fg_gc[0],
		 event->area.x, event->area.y,
		 event->area.x, event->area.y,
		 event->area.width, event->area.height,
		 GDK_RGB_DITHER_NORMAL, 0, 0);
}

void 
set_preview_window (GtkWidget *widget) 
{
	if (expose_id)
		gtk_signal_disconnect (GTK_OBJECT (preview_window), expose_id);
	preview_window = widget;
	expose_id = gtk_signal_connect (GTK_OBJECT (preview_window),
					"expose_event", expose_func, NULL);
}

static char **
strip_arg (char **args, char *arg) 
{
	int i, j, arg_len;

	arg_len = strlen (arg);

	for (i = 0; args[i]; i++) {
		if (!strncmp (args[i], arg, arg_len)) {
			for (j = i; args[j]; j++)
				args[j] = args[j + 1];
			i--;
		}
	}

	return args;
}

static void
setup_path (void) 
{
	GString *newpath;
	char *path;
#if !defined(HAVE_SETENV) && defined(HAVE_PUTENV)
	char *str;
#endif
	GList *node;

	node = get_screensaver_dir_list ();

	path = g_getenv ("PATH");
	newpath = g_string_new (path);

	for (; node; node = node->next) {
		g_string_append (newpath, ":");
		g_string_append (newpath, (gchar *) node->data);
	}

#if defined(HAVE_SETENV)
	setenv ("PATH", newpath->str, TRUE);
#elif defined(HAVE_PUTENV)
	str = g_strdup_printf ("PATH=%s", newpath->str);
	putenv (str);
	g_free (str);
#endif

	g_string_free (newpath, TRUE);
}

/* Warning: memory leaks, please do not use except in a separate
 * process 
 */

static char **
add_window_arg (char **args, GdkWindow *window) 
{
	int i;
	char *x_window_id;

	for (i = 0; args[i]; i++);

	x_window_id = 
		g_strdup_printf ("0x%x", 
				 (guint) GDK_WINDOW_XWINDOW (window));

	args = g_renew (char *, args, i + 4);
	args[i] = "-window";
	args[i + 1] = "-window-id";
	args[i + 2] = x_window_id;
	args[i + 3] = NULL;

	return args;
}

/* fix_arguments
 *
 * Given an array of CLI arguments naively split, convert them into actual CLI 
 * arguments. Note: leaks memory a lot.
 */

static char **
fix_arguments (char **argv) 
{
	char **out;
	gchar *tmp, *tmp1;
	int i, j, argc;

	for (argc = 0; argv[argc]; argc++);

	out = g_new0 (char *, argc + 1);

	for (i = 0, j = 0; i < argc; i++) {
		if (argv[i][0] != '\"') {
			out[j++] = argv[i];
		} else {
			tmp = g_strdup (argv[i] + 1);
			while (i < argc) {
				if (argv[i][strlen (argv[i]) - 1] == '\"') {
					tmp[strlen (tmp) - 1] = '\0';
					break;
				}
				i++;
				tmp1 = g_strconcat (tmp, " ", argv[i], NULL);
				g_free (tmp);
				tmp = tmp1;
			}

			out[j++] = tmp;
		}
	}

	return out;
}

/* show_screensaver
 *
 * Given a GdkWindow in which to render and a particular screensaver,
 * fork off a process and start the screensaver.
 */

static void 
show_screensaver (GdkWindow *window, Screensaver *saver, pid_t *pid) 
{
	char **args;
	gchar *command_line;
	
	if (saver->command_line)
		command_line = saver->command_line;
	else
	{
		command_line = saver->compat_command_line;
	}

	*pid = fork ();

	if (*pid == (pid_t) -1) {
		perror ("fork");
		abort ();
	}
	else if (*pid == 0) {
		nice (20);    /* Very low priority */

		args = g_strsplit (command_line, " ", -1);
		args = fix_arguments (args);
		args = strip_arg (args, "-root");
		args = add_window_arg (args, window);

		setup_path ();

		if (execvp (args[0], args) == -1) {
			perror ("execv");
			abort ();
		}

		exit (1);
	}
}

static gint
show_screensaver_timeout (void) 
{
	int ret;

	ret = waitpid (preview_pid, NULL, WNOHANG);

	if (ret == -1) {
		g_error ("waitpid: %s", g_strerror (errno));
	}
	else if (ret > 0) {
		if (pixbuf)
			gdk_pixbuf_unref (pixbuf);

		pixbuf = gdk_pixbuf_new_from_file 
			(GNOMECC_PIXMAPS_DIR "/no-hack.png", NULL);
		gdk_pixbuf_render_to_drawable
			(pixbuf, (GdkDrawable *) preview_window->window,
			 preview_window->style->fg_gc[0], 0, 0, 0, 0,
			 300, 250, GDK_RGB_DITHER_NONE, 0, 0);
	}

	timeout_id = 0;

	return FALSE;
}

void 
show_preview (Screensaver *saver) 
{
	/* Note: kill this next line for a very interesting effect ... */
	close_preview ();
	if (!(saver->command_line || saver->compat_command_line || saver->fakepreview)) return;
	gtk_widget_map (preview_window);

	if (pixbuf)
	{
		gdk_pixbuf_unref (pixbuf);
		pixbuf = NULL;
	}

	if (saver->fakepreview)
	{
		pixbuf = gdk_pixbuf_new_from_file (saver->fakepreview, NULL);
		gdk_pixbuf_render_to_drawable
			(pixbuf, (GdkDrawable *) preview_window->window,
			 preview_window->style->fg_gc[0], 0, 0, 0, 0,
			 gdk_pixbuf_get_width (pixbuf),
			 gdk_pixbuf_get_height (pixbuf),
			 GDK_RGB_DITHER_NONE, 0, 0);
	}
	else
	{	
		show_screensaver (preview_window->window, saver, &preview_pid);
		timeout_id =
			gtk_timeout_add (500, (GtkFunction)
					 show_screensaver_timeout, NULL);
	}
}

void 
close_preview (void) 
{
	if (timeout_id) {
		gtk_timeout_remove (timeout_id);
		timeout_id = 0;
	}

	if (preview_pid) {
		kill (preview_pid, SIGTERM);
		preview_pid = 0;
		gtk_widget_unmap (preview_window);
	}
}

void
show_blank_preview (void)
{
	close_preview ();
	gtk_widget_map (preview_window);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	pixbuf = gdk_pixbuf_new_from_file (GNOMECC_PIXMAPS_DIR "/blank-screen.png", NULL);
	gdk_pixbuf_render_to_drawable
		(pixbuf, (GdkDrawable *) preview_window->window,
		 preview_window->style->fg_gc[0], 0, 0, 0, 0,
		 300, 250, GDK_RGB_DITHER_NONE, 0, 0);
}

/* Ick... */

static GdkWindow *
gdk_window_new_from_xwindow (Window xwindow) 
{
	return gdk_window_foreign_new (xwindow);
}

static GdkWindow *
find_xscreensaver_window (Display *dpy) 
{
	static Atom XA_SCREENSAVER;
	GdkAtom actual_type;
	gint actual_format;
	Window root, parent, *children;
	Window root_ret;
	GdkWindow *ret;
	gint number_children, i;
	unsigned long nitems, bytes_after;
	gboolean found;
	unsigned char *data;
	gint x_error;

	if (!XA_SCREENSAVER)
		XA_SCREENSAVER = 
			XInternAtom (dpy, "_SCREENSAVER_VERSION", FALSE);

	gdk_error_trap_push ();

	root = GDK_ROOT_WINDOW ();

	XQueryTree (dpy, DefaultRootWindow (dpy), 
		    &root_ret, &parent, &children, &number_children);

	if (root_ret != root) abort ();
	if (parent) abort ();
	if (!children || !number_children) return NULL;

	for (i = 0; i < number_children; i++) {
		found = XGetWindowProperty (dpy, children[i],
					    XA_SCREENSAVER,
					    0, 0, FALSE, XA_STRING,
					    &actual_type,
					    &actual_format, &nitems,
					    &bytes_after, &data);
		free (data);

		if (actual_type != None) {
			ret = gdk_window_new_from_xwindow (children[i]);
			free (children);
			return ret;
		}
	}

	free (children);

	gdk_flush ();
	x_error = gdk_error_trap_pop ();

	if (x_error && x_error != BadWindow)
		g_error ("X error");

	return NULL;
}

void 
show_demo (Screensaver *saver)
{
	static GdkAtom demo_atom, xscreensaver_atom;
	GdkWindow *xscreensaver_window;
	GdkEventClient *event;

	if (!demo_atom)
		demo_atom = gdk_atom_intern ("DEMO", FALSE);

	if (!xscreensaver_atom)
		xscreensaver_atom = gdk_atom_intern ("SCREENSAVER", FALSE);

	xscreensaver_window = find_xscreensaver_window (gdk_display);

	if (!xscreensaver_window)
		g_error ("Could not find XScreensaver window");

	event = g_new0 (GdkEventClient, 1);
	event->type = GDK_CLIENT_EVENT;
	event->window = xscreensaver_window;
	event->message_type = xscreensaver_atom;
	event->data_format = 32;
	event->data.l[0] = demo_atom;
	event->data.l[1] = 300;           /* XA_DEMO protocol version number */
	event->data.l[2] = saver->id + 1;

	gdk_event_send_client_message 
		((GdkEvent *) event, GDK_WINDOW_XWINDOW (xscreensaver_window));

	gdk_flush ();
	g_free (xscreensaver_window);
	g_free (event);
}

void
close_demo (void) 
{
}
