/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *
 *  Copyright (C) 1998 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Jonathan Blandford <jbr@redhat.com>
 *  	     Gene Z. Ragan 	<gzr@eazel.com>
 *
 */

#include <config.h>
#include <capplet-widget.h>
#include <gnome.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "mime-data.h"
#include "nautilus-mime-type-capplet.h"

#define MIME_COMMENT_CHAR '#'

/* Prototypes */
void add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user);
static char *get_priority (char *def, int *priority);


/* Global variables */
/* static char *current_lang; */
static GHashTable *mime_types = NULL;
static GHashTable *initial_user_mime_types = NULL;
GHashTable *user_mime_types = NULL;
static GtkWidget *clist = NULL;
extern GtkWidget *delete_button;
extern GtkWidget *capplet;

/* Initialization functions */
static void
run_error (gchar *message)
{
	GtkWidget *error_box;

	error_box = gnome_message_box_new (
		message,
		GNOME_MESSAGE_BOX_ERROR,
		GNOME_STOCK_BUTTON_OK,
		NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (error_box));
}

static char *
get_priority (char *def, int *priority)
{
	*priority = 0;
	
	if (*def == ','){
		def++;
		if (*def == '1'){
			*priority = 0;
			def++;
		} else if (*def == '2'){
			*priority = 1;
			def++;
		}
	}

	while (*def && *def == ':')
		def++;

	return def;
}

void
add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user)
{
	int priority = 1;
	char *s, *p, *ext;
	int used;
	MimeInfo *info;

	info = g_hash_table_lookup (table, (const void *) mime_type);
	if (info == NULL) {
		info = g_malloc (sizeof (MimeInfo));
		info->mime_type = g_strdup (mime_type);
		info->ext[0] = NULL;
		info->ext[1] = NULL;
                info->user_ext[0] = NULL;
                info->user_ext[1] = NULL;
                info->keys = gnome_vfs_mime_get_keys (mime_type);
		g_hash_table_insert (table, info->mime_type, info);
	}
	
	if (strncmp (def, "ext", 3) == 0) {
		char *tokp;

		def += 3;
		def = get_priority (def, &priority);
		s = p = g_strdup (def);

		used = 0;
		
		while ((ext = strtok_r (s, " \t\n\r,", &tokp)) != NULL){
                        /* FIXME bugzilla.eazel.com 1222: 
                         * We really need to check for duplicates before entering this. 
                         */
                        if (!init_user) {
                                info->ext[priority] = g_list_prepend (info->ext[priority], ext);
                        } else {
                                info->user_ext[priority] = g_list_prepend (info->user_ext[priority], ext);
                        }
			used = 1;
			s = NULL;
		}
		
		if (!used) {
			g_free (p);
		}
	}
}


int
add_mime_vals_to_clist (gchar *mime_type, gpointer mi, gpointer cl)
{
	static gchar *text[2];        
	const char *description;
	MimeInfo *info;
        gint row;

	/* Skip comments */
	if (mime_type[0] == MIME_COMMENT_CHAR) {
		return -1;
	}
		
	info = (MimeInfo *) mi;

	/* Add mime type to first column */
	text[0] = info->mime_type;

	/* Add description to second column */
	description = gnome_vfs_mime_description (mime_type);	
	if (description != NULL && strlen (description) > 0) {
		text[1] = g_strdup (description);
	} else {
		text[1] = g_strdup ("");
	}
	
        row = gtk_clist_insert (GTK_CLIST (cl), 1, text);
        gtk_clist_set_row_data (GTK_CLIST (cl), row, mi);

	g_free (text[1]);
	
        return row;
}

void
add_new_mime_type (gchar *mime_type, gchar *description)
{
        gchar *temp;
        MimeInfo *mi = NULL;
        gint row;
        gchar *desc_key = NULL;
        gchar *ptr, *ptr2;
        
        /* First we make sure that the information is good */
        if (mime_type == NULL || *mime_type == '\000') {
                run_error (_("You must enter a mime-type"));
                return;
        } else if (description == NULL || *description == '\000') {
                run_error (_("You must add a description"));
                return;
        }

        if (strchr (mime_type, '/') == NULL) {
                run_error (_("Please put your mime-type in the format:\nCATEGORY/TYPE\n\nFor Example:\nimage/png"));
                return;
        }

        if (g_hash_table_lookup (user_mime_types, mime_type) ||
            g_hash_table_lookup (mime_types, mime_type)) {
                run_error (_("This mime-type already exists"));
                return;
        }
        
        if (description || *description) {
                ptr2 = desc_key = g_malloc (sizeof (description));
                for (ptr = description; *ptr; ptr++) {
                        if (*ptr != '.' && *ptr != ',') {
                                *ptr2 = *ptr;
                                ptr2 += 1;
                        }
                }
                *ptr2 = '\000';
        }
        
        /* Passed check, Now we add it. */
        if (desc_key) {
                temp = g_strconcat ("description: ", desc_key, NULL);
                add_to_key (mime_type, temp, user_mime_types, TRUE);
                mi = (MimeInfo *) g_hash_table_lookup (user_mime_types, mime_type);
                g_free (temp);
        }

        /* Finally add it to the clist */
        if (mi) {
                row = add_mime_vals_to_clist (mime_type, mi, clist);
                if (row >= 0) {
	                gtk_clist_select_row (GTK_CLIST (clist), row, 0);
	                gtk_clist_moveto (GTK_CLIST (clist), row, 0, 0.5, 0.0);
		}
        }
        
        //g_free (desc_key);
}

#if 0
void
add_new_mime_type (gchar *mime_type, gchar *raw_ext)
{
        gchar *temp;
        MimeInfo *mi = NULL;
        gint row;
        gchar *ext = NULL;
        gchar *ptr, *ptr2;
        
        /* first we make sure that the information is good */
        if (mime_type == NULL || *mime_type == '\000') {
                run_error (_("You must enter a mime-type"));
                return;
        } else if (raw_ext == NULL || *raw_ext == '\000') {
                run_error (_("You must add a file-name extension"));
                return;
        }
        if (strchr (mime_type, '/') == NULL) {
                run_error (_("Please put your mime-type in the format:\nCATEGORY/TYPE\n\nFor Example:\nimage/png"));
                return;
        }
        if (g_hash_table_lookup (user_mime_types, mime_type) ||
            g_hash_table_lookup (mime_types, mime_type)) {
                run_error (_("This mime-type already exists"));
                return;
        }
        if (raw_ext || *raw_ext) {
                ptr2 = ext = g_malloc (sizeof (raw_ext));
                for (ptr = raw_ext;*ptr; ptr++) {
                        if (*ptr != '.' && *ptr != ',') {
                                *ptr2 = *ptr;
                                ptr2 += 1;
                        }
                }
                *ptr2 = '\000';
        }
        
        /* passed check, now we add it. */
        if (ext) {
                temp = g_strconcat ("ext: ", ext, NULL);
                add_to_key (mime_type, temp, user_mime_types, TRUE);
                mi = (MimeInfo *) g_hash_table_lookup (user_mime_types, mime_type);
                g_free (temp);
        }

        /* Finally add it to the clist */
        if (mi) {
                row = add_mime_vals_to_clist (mime_type, mi, clist);
                if (row >= 0) {
	                gtk_clist_select_row (GTK_CLIST (clist), row, 0);
	                gtk_clist_moveto (GTK_CLIST (clist), row, 0, 0.5, 0.0);
		}
        }
        g_free (ext);
}
#endif

static void
write_mime_foreach (gpointer mime_type, gpointer info, gpointer data)
{
/* 	gchar *buf; */
	MimeInfo *mi = (MimeInfo *) info;
	fwrite ((char *) mi->mime_type, 1, strlen ((char *) mi->mime_type), (FILE *) data);
	fwrite ("\n", 1, 1, (FILE *) data);
        fwrite ("\n", 1, 1, (FILE *) data);
}

static void
write_mime (GHashTable *hash)
{
	struct stat s;
	gchar *dirname, *filename;
	FILE *file;
/* 	GtkWidget *error_box; */

	dirname = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/mime-info");
	if ((stat (dirname, &s) < 0) || !(S_ISDIR (s.st_mode))){
		if (errno == ENOENT) {
			if (mkdir (dirname, S_IRWXU) < 0) {
				run_error (_("We are unable to create the directory\n"
					   "~/.gnome/mime-info\n\n"
					   "We will not be able to save the state."));
				return;
			}
		} else {
			run_error (_("We are unable to access the directory\n"
				   "~/.gnome/mime-info\n\n"
				   "We will not be able to save the state."));
			return;
		}
	}
	filename = g_concat_dir_and_file (dirname, "user.mime");
        
        remove (filename);
	file = fopen (filename, "w");
	if (file == NULL) {
		run_error (_("Cannot create the file\n~/.gnome/mime-info/user.mime\n\n"
			     "We will not be able to save the state"));
		return;
	}
	g_hash_table_foreach (hash, write_mime_foreach, file);
	fclose (file);
}

void
write_user_mime (void)
{
        write_mime (user_mime_types);
}

void
write_initial_mime (void)
{
        write_mime (initial_user_mime_types);
}

void
reread_list (void)
{
        gtk_clist_freeze (GTK_CLIST (clist));
        gtk_clist_clear (GTK_CLIST (clist));
        g_hash_table_foreach (mime_types, (GHFunc) add_mime_vals_to_clist, clist);
        gtk_clist_thaw (GTK_CLIST (clist));
}

