/* -*- mode: c; style: linux -*- */

/* capplet-dir.c
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>,
 *            Jonathan Blandford <jrb@redhat.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "capplet-dir.h"
#include "capplet-dir-view.h"

static void capplet_activate     (Capplet *capplet);
static void capplet_dir_activate (CappletDir *capplet_dir,
				  CappletDirView *launcher);

static void capplet_shutdown     (Capplet *capplet);
static void capplet_dir_shutdown (CappletDir *capplet_dir);

static CappletDirEntry **read_entries (CappletDir *dir);

static void start_capplet_through_root_manager (GnomeDesktopEntry *gde);

CappletDirView *(*get_view_cb) (CappletDir *dir, CappletDirView *launcher);

CappletDirEntry *
capplet_new (CappletDir *dir, gchar *desktop_path) 
{
	Capplet *capplet;
	CappletDirEntry *entry;
	
	g_return_val_if_fail (desktop_path != NULL, NULL);

	capplet = g_new0 (Capplet, 1);
	entry = CAPPLET_DIR_ENTRY (capplet);

	entry->type = TYPE_CAPPLET;
	entry->entry = gnome_desktop_entry_load (desktop_path);
	entry->label = entry->entry->name;
	entry->icon = entry->entry->icon;
	entry->dir = dir;

	/* Don't continue if this is just the control center again */
	if (!strcmp (entry->entry->exec[0], "gnomecc")) {
		capplet_dir_entry_destroy (entry);
		return NULL;
	}

	if (!entry->icon)
		entry->icon = PIXMAPS_DIR "/control-center.png";

	return entry;
}

CappletDirEntry *
capplet_dir_new (CappletDir *dir, gchar *dir_path) 
{
	CappletDir *capplet_dir;
	CappletDirEntry *entry;
	gchar *desktop_path;

	g_return_val_if_fail (dir_path != NULL, NULL);

	desktop_path = g_concat_dir_and_file (dir_path, ".directory");

	capplet_dir = g_new0 (CappletDir, 1);
	entry = CAPPLET_DIR_ENTRY (capplet_dir);

	entry->type = TYPE_CAPPLET_DIR;
	entry->entry = gnome_desktop_entry_load (desktop_path);
	entry->dir = dir;

	g_free (desktop_path);

	if (entry->entry) {
		entry->label = entry->entry->name;
		entry->icon = entry->entry->icon;

		if (!entry->icon)
			entry->icon = PIXMAPS_DIR "/control-center.png";
	} else {
		/* If the .directory file could not be found or read, abort */
		g_free (capplet_dir);
		return NULL;
	}

	capplet_dir->path = g_strdup (dir_path);

	return entry;
}

void 
capplet_dir_entry_destroy (CappletDirEntry *entry)
{
	if (entry->type == TYPE_CAPPLET) {
		capplet_shutdown (CAPPLET (entry));
	} else {
		capplet_dir_shutdown (CAPPLET_DIR (entry));
		g_free (CAPPLET_DIR (entry)->path);
	}

	gnome_desktop_entry_free (entry->entry);
	g_free (entry);
}

void 
capplet_dir_entry_activate (CappletDirEntry *entry, 
			    CappletDirView *launcher)
{
	g_return_if_fail (entry != NULL);

	if (entry->type == TYPE_CAPPLET)
		capplet_activate (CAPPLET (entry));
	else if (entry->type == TYPE_CAPPLET_DIR)
		capplet_dir_activate (CAPPLET_DIR (entry), launcher);
	else
		g_assert_not_reached ();
}

void 
capplet_dir_entry_shutdown (CappletDirEntry *entry)
{
	if (entry->type == TYPE_CAPPLET)
		capplet_shutdown (CAPPLET (entry));
	else if (entry->type == TYPE_CAPPLET_DIR)
		capplet_dir_shutdown (CAPPLET_DIR (entry));
	else
		g_assert_not_reached ();
}

static void
capplet_activate (Capplet *capplet) 
{
	GnomeDesktopEntry *entry;

	entry = CAPPLET_DIR_ENTRY (capplet)->entry;

	if (!strcmp (entry->exec[0], "root-manager"))
		start_capplet_through_root_manager (entry);
	else
		gnome_desktop_entry_launch (entry);
}

void
capplet_dir_load (CappletDir *capplet_dir) 
{
	if (capplet_dir->entries) return;
	capplet_dir->entries = read_entries (capplet_dir);
}

static void
capplet_dir_activate (CappletDir *capplet_dir, CappletDirView *launcher) 
{
	capplet_dir_load (capplet_dir);
	capplet_dir->view = get_view_cb (capplet_dir, launcher);

	capplet_dir_view_load_dir (capplet_dir->view, capplet_dir);
	gtk_widget_show_all (GTK_WIDGET (capplet_dir->view));
}

static void
capplet_shutdown (Capplet *capplet) 
{
	/* Can't do much here ... :-( */
}

static void
capplet_dir_shutdown (CappletDir *capplet_dir) 
{
	int i;

	if (capplet_dir->view)
		gtk_object_unref (GTK_OBJECT (capplet_dir->view));

	if (capplet_dir->entries) {
		for (i = 0; capplet_dir->entries[i]; i++)
			capplet_dir_entry_destroy
				(capplet_dir->entries[i]);
		g_free (capplet_dir->entries);
	}
}

static gint 
node_compare (gconstpointer a, gconstpointer b) 
{
	return strcmp (CAPPLET_DIR_ENTRY (a)->entry->name, 
		       CAPPLET_DIR_ENTRY (b)->entry->name);
}

/* Adapted from the original control center... */

static CappletDirEntry **
read_entries (CappletDir *dir) 
{
        DIR *parent_dir;
        struct dirent *child_dir;
        struct stat filedata;
        GList *list_head, *list_tail;
	CappletDirEntry *entry;
	gchar *fullpath;
	CappletDirEntry **entry_array;
	int i;

        parent_dir = opendir (dir->path);
        if (parent_dir == NULL)
                return NULL;

	list_head = list_tail = NULL;

        while ((child_dir = readdir (parent_dir)) != NULL) {
                if (child_dir->d_name[0] != '.') {
                        /* we check to see if it is interesting. */
			fullpath = g_concat_dir_and_file (dir->path,
							  child_dir->d_name);

                        if (stat (fullpath, &filedata) != -1) {
                                gchar* test;

				entry = NULL;

                                if (S_ISDIR (filedata.st_mode)) {
					entry = capplet_dir_new 
						(dir, fullpath);
				} else {
					test = rindex(child_dir->d_name, '.');

					if (test && 
					    !strcmp (".desktop", test)) 
                                        /* it's a .desktop file --
					 * it's interesting for sure! */
						entry = capplet_new
							(dir, fullpath);
                                }

				if (entry) {
					list_tail = g_list_append
						(list_tail, entry);
					if (!list_head)
						list_head = list_tail;
					else
						list_tail = list_tail->next;
				}
                        }

			g_free (fullpath);
                }
        }
        
        closedir (parent_dir);

	list_head = g_list_sort (list_head, node_compare);

	/* Allocate the array and copy the list contents over */
	entry_array = g_new0 (CappletDirEntry *, 
			      g_list_length (list_head) + 1);

	i = 0;
	while (list_head) {
		entry_array[i++] = list_head->data;
		list_head = g_list_remove_link (list_head, list_head);
	}

	entry_array[i] = NULL;

        return entry_array;
}

static void
start_capplet_through_root_manager (GnomeDesktopEntry *gde) 
{
	static FILE *output = NULL;
	pid_t pid;
	char *cmdline;

	if (!output) {
		gint pipe_fd[2];
		pipe (pipe_fd);

		pid = fork ();

		if (pid == (pid_t) -1) {
			g_error ("%s", g_strerror (errno));
		}
		else if (pid == 0) {
			char *arg[2];
			int i;

			dup2 (pipe_fd[0], 0);
      
			for (i = 3; i < FOPEN_MAX; i++) close(i);

			arg[0] = gnome_is_program_in_path ("root-manager");
			arg[1] = NULL;
			execv (arg[0], arg);
		}
		else
		{
			output = fdopen(pipe_fd[1], "a");
		}
	}

	cmdline = g_strjoinv (" ", gde->exec + 1);
	fprintf (output, "%s\n", cmdline);
	fflush (output);
	g_free (cmdline);
}

void 
capplet_dir_init (CappletDirView *(*cb) (CappletDir *, CappletDirView *)) 
{
	get_view_cb = cb;
}

CappletDir *
get_root_capplet_dir (void)
{
	static CappletDir *root_dir = NULL;

	if (root_dir == NULL) {
		root_dir = CAPPLET_DIR (capplet_dir_new (NULL, SETTINGS_DIR));

		if (!root_dir)
			g_error ("Could not find directory of control panels");
	}

	return root_dir;
}
