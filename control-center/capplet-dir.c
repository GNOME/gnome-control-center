/* -*- mode: c; style: linux -*- */

/* capplet-dir.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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
#include "capplet-dir-window.h"

static void capplet_activate     (Capplet *capplet);
static void capplet_dir_activate (CappletDir *capplet_dir);

static void capplet_shutdown     (Capplet *capplet);
static void capplet_dir_shutdown (CappletDir *capplet_dir);

static CappletDirEntry **read_entries (gchar *path);

static void start_capplet_through_root_manager (GnomeDesktopEntry *gde);

CappletDirEntry *
capplet_new (gchar *desktop_path) 
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
capplet_dir_new (gchar *dir_path) 
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
capplet_dir_entry_activate (CappletDirEntry *entry)
{
	g_return_if_fail (entry != NULL);

	if (entry->type == TYPE_CAPPLET)
		capplet_activate (CAPPLET (entry));
	else if (entry->type == TYPE_CAPPLET_DIR)
		capplet_dir_activate (CAPPLET_DIR (entry));
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

static void
capplet_dir_activate (CappletDir *capplet_dir) 
{
	capplet_dir->entries = read_entries (capplet_dir->path);
	capplet_dir->window = capplet_dir_window_new (capplet_dir);
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

	if (capplet_dir->window)
		capplet_dir_window_destroy (capplet_dir->window);

	if (capplet_dir->entries) {
		for (i = 0; capplet_dir->entries[i]; i++)
			capplet_dir_entry_destroy 
				(capplet_dir->entries[i]);
		g_free (capplet_dir->entries);
	}
}

static CappletDirEntry **
read_entries (gchar *path) 
{
        DIR *parent_dir;
        struct dirent *child_dir;
        struct stat filedata;
        GList *list_head, *list_tail;
	CappletDirEntry *entry;
	gchar *fullpath;
	CappletDirEntry **entry_array;
	int i;

        parent_dir = opendir (path);
        if (parent_dir == NULL)
                return NULL;

	list_head = list_tail = NULL;

        while ((child_dir = readdir (parent_dir)) != NULL) {
                if (child_dir->d_name[0] != '.') {
                        /* we check to see if it is interesting. */
			fullpath = g_concat_dir_and_file (path,
							  child_dir->d_name);

                        if (stat (fullpath, &filedata) != -1) {
                                gchar* test;

				entry = NULL;

                                if (S_ISDIR (filedata.st_mode)) {
					entry = capplet_dir_new (fullpath);
				} else {
					test = rindex(child_dir->d_name, '.');

					if (test && 
					    !strcmp (".desktop", test)) 
                                        /* it's a .desktop file --
					 * it's interesting for sure! */
						entry = capplet_new (fullpath);
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

			dup2 (pipe_fd[0], 0);

			arg[0] = gnome_is_program_in_path ("root-manager");
			arg[1] = NULL;
			execv (arg[0], arg);
		}
	}

	cmdline = g_strjoinv (" ", gde->exec + 1);
	fprintf (output, "%s\n", cmdline);
	g_free (cmdline);
}
