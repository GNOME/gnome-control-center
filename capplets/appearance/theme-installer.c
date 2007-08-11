/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "appearance.h"

#include <string.h>
#include <libwindow-settings/gnome-wm-manager.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gnome-theme-info.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"
#include "theme-installer.h"
#include "theme-util.h"

enum {
	THEME_INVALID,
	THEME_ICON,
	THEME_GNOME,
	THEME_GTK,
	THEME_ENGINE,
	THEME_METACITY
};

enum {
	TARGZ,
	TARBZ,
	DIRECTORY
};

static void
cleanup_tmp_dir (const gchar *tmp_dir)
{
	if (gnome_vfs_remove_directory (tmp_dir) == GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY) {
		GList *list;

		list = g_list_prepend (NULL, gnome_vfs_uri_new (tmp_dir));
		gnome_vfs_xfer_delete_list (list, GNOME_VFS_XFER_RECURSIVE,
					GNOME_VFS_XFER_ERROR_MODE_ABORT, NULL, NULL);
		gnome_vfs_uri_list_free (list);
	}
}

static int
file_theme_type (const gchar *dir)
{
	gchar *filename = NULL;
	GnomeVFSURI *src_uri = NULL;
	gboolean exists;

	if (!dir) return THEME_INVALID;

	filename = g_build_filename (dir, "index.theme", NULL);
	src_uri = gnome_vfs_uri_new (filename);
	exists = gnome_vfs_uri_exists (src_uri);
	gnome_vfs_uri_unref (src_uri);

	if (exists) {
		GPatternSpec *pattern = NULL;
		gchar *file_contents = NULL;
		gint file_size;
		gchar *uri;
		gboolean match;

		uri = gnome_vfs_get_uri_from_local_path (filename);
		gnome_vfs_read_entire_file (uri, &file_size, &file_contents);
		g_free (uri);

		pattern = g_pattern_spec_new ("*[Icon Theme]*");
		match = g_pattern_match_string (pattern, file_contents);
		g_pattern_spec_free (pattern);

		if (match)
			return THEME_ICON;

		pattern = g_pattern_spec_new ("*[X-GNOME-Metatheme]*");
		match = g_pattern_match_string (pattern, file_contents);
		g_pattern_spec_free (pattern);

		if (match)
			return THEME_GNOME;
	}
	g_free (filename);

	filename = g_build_filename (dir, "gtk-2.0", "gtkrc", NULL);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	exists = gnome_vfs_uri_exists (src_uri);
	gnome_vfs_uri_unref (src_uri);

	if (exists)
		return THEME_GTK;

	filename = g_build_filename (dir, "metacity-1", "metacity-theme-1.xml", NULL);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	exists = gnome_vfs_uri_exists (src_uri);
	gnome_vfs_uri_unref (src_uri);

	if (exists)
		return THEME_METACITY;

	filename = g_build_filename (dir, "configure", NULL);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	exists = gnome_vfs_uri_exists (src_uri);
	gnome_vfs_uri_unref (src_uri);

	if (exists)
		return THEME_ENGINE;

	return THEME_INVALID;
}

static void
transfer_cancel_cb (GtkWidget *dlg, gchar *path)
{
	gnome_vfs_unlink (path);
	g_free (path);
	gtk_widget_destroy (dlg);
}

static void
missing_utility_message_dialog (const gchar *utility)
{
	GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Cannot install theme.\nThe %s utility is not installed."), utility);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* this works around problems when doing fork/exec in a threaded app
 * with some locks being held/waited on in different threads.
 *
 * we do the idle callback so that the async xfer has finished and
 * cleaned up its vfs job.  otherwise it seems the slave thread gets
 * woken up and it removes itself from the job queue before it is
 * supposed to.  very strange.
 *
 * see bugzilla.gnome.org #86141 for details
 */
static gboolean
transfer_done_tgz_tbz (const gchar *util, const gchar *tmp_dir, const gchar *archive)
{
	gboolean rc;
	int status;
	gchar *command, *filename, *zip, *tar;

	if (!(zip = g_find_program_in_path (util))) {
		missing_utility_message_dialog (util);
		return FALSE;
	}
	if (!(tar = g_find_program_in_path ("tar"))) {
		missing_utility_message_dialog ("tar");
		g_free (zip);
		return FALSE;
	}

	filename = g_shell_quote (archive);

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'cd \"%s\"; %s -d -c < \"%s\" | %s xf - '",
				   tmp_dir, zip, filename, tar);
	g_free (zip);
	g_free (tar);
	g_free (filename);

	rc = (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0);
	g_free (command);

	if (rc == FALSE) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Cannot install theme.\nThere was a problem while extracting the theme."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	return rc;
}

static gboolean
transfer_done_archive (gint filetype, const gchar *tmp_dir, const gchar *archive)
{
	if (filetype == TARGZ)
		return transfer_done_tgz_tbz ("gzip", tmp_dir, archive);
	else if (filetype == TARBZ)
		return transfer_done_tgz_tbz ("bzip2", tmp_dir, archive);
	else
		return FALSE;
}

static gboolean
gnome_theme_install_real (gint filetype, const gchar *tmp_dir, const gchar *theme_name)
{
	gboolean success = TRUE;
	GtkWidget *dialog, *apply_button;
	int xfer_options;
	GnomeVFSURI *theme_source_dir, *theme_dest_dir;
	gint theme_type;
	gchar *user_message = NULL;
	gchar *target_dir = NULL;

	/* What type of theme it is ? */
	theme_type = file_theme_type (tmp_dir);
	if (theme_type == THEME_ICON) {
		target_dir = g_build_path (G_DIR_SEPARATOR_S,
					   g_get_home_dir (), ".icons",
					   theme_name, NULL);
	} else if (theme_type == THEME_GNOME) {
		target_dir = g_build_path (G_DIR_SEPARATOR_S,
					   g_get_home_dir (), ".themes",
			 		   theme_name, NULL);
		user_message = g_strdup_printf (_("GNOME Theme %s correctly installed"),
						theme_name);
	} else if (theme_type == THEME_METACITY ||
		   theme_type == THEME_GTK) {
		target_dir = g_build_path (G_DIR_SEPARATOR_S,
					   g_get_home_dir (), ".themes",
					   theme_name, NULL);
	} else if (theme_type == THEME_ENGINE) {
		dialog = gtk_message_dialog_new (NULL,
			       GTK_DIALOG_MODAL,
			       GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_OK,
			       _("The theme is an engine. You need to compile it."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		dialog = gtk_message_dialog_new (NULL,
			       GTK_DIALOG_MODAL,
			       GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_OK,
			       _("The file format is invalid"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	/* see if there is an icon theme lurking in this package */
	if (theme_type == THEME_GNOME)
	{
		gchar *path;

		path = g_build_path (G_DIR_SEPARATOR_S,
				     tmp_dir, "icons", NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)
		    && (file_theme_type (path) == THEME_ICON))
		{
			gchar *new_path, *update_icon_cache;

			new_path = g_build_path (G_DIR_SEPARATOR_S,
						 g_get_home_dir (),
						 ".icons",
						 theme_name, NULL);
			update_icon_cache = g_strdup_printf ("gtk-update-icon-cache %s", new_path);
			/* XXX: make some noise if we couldn't install it? */
			gnome_vfs_move (path, new_path, FALSE);

			/* update icon cache - shouldn't really matter if this fails */
			g_spawn_command_line_async (update_icon_cache, NULL);

			g_free (new_path);
			g_free (update_icon_cache);
		}
		g_free (path);
	}

	/* Move the dir to the target dir */
	theme_source_dir = gnome_vfs_uri_new (tmp_dir);
	theme_dest_dir = gnome_vfs_uri_new (target_dir);

	xfer_options = GNOME_VFS_XFER_DELETE_ITEMS | GNOME_VFS_XFER_RECURSIVE;
	if (filetype != DIRECTORY)
		xfer_options |= GNOME_VFS_XFER_REMOVESOURCE;

	if (gnome_vfs_xfer_uri (theme_source_dir, theme_dest_dir, xfer_options,
				GNOME_VFS_XFER_ERROR_MODE_ABORT,
				GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				NULL, NULL) != GNOME_VFS_OK) {
		dialog = gtk_message_dialog_new (NULL,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					_("Installation Failed"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		success = FALSE;
	} else {
		if (theme_type == THEME_ICON)
		{
			gchar *update_icon_cache;

			/* update icon cache - shouldn't really matter if this fails */
			update_icon_cache = g_strdup_printf ("gtk-update-icon-cache %s", target_dir);
			g_spawn_command_line_async (update_icon_cache, NULL);

			g_free (update_icon_cache);
		}
		/* Ask to apply theme (if we can) */
		if (theme_type == THEME_GTK || theme_type == THEME_METACITY || theme_type == THEME_ICON)
		{
			/* TODO: currently cannot apply "gnome themes" */
			gchar *str;

			str = g_strdup_printf(_("The theme \"%s\" has been installed."), theme_name);
			user_message = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s</span>", str);
			g_free (str);

			dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, user_message);

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Would you like to apply it now, or keep your current theme?"));

			gtk_dialog_add_button (GTK_DIALOG (dialog), _("Keep Current Theme"), GTK_RESPONSE_CLOSE);

			apply_button = gtk_button_new_with_label (_("Apply New Theme"));
			gtk_button_set_image (GTK_BUTTON (apply_button), gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
			gtk_dialog_add_action_widget (GTK_DIALOG (dialog), apply_button, GTK_RESPONSE_APPLY);
			GTK_WIDGET_SET_FLAGS (apply_button, GTK_CAN_DEFAULT);
			gtk_widget_show (apply_button);

			gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_APPLY);

			if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_APPLY)
			{
				/* apply theme here! */
				GConfClient *gconf_client;
				const gchar *gconf_key;

				switch (theme_type)
				{
					case THEME_GTK:
						gconf_key = GTK_THEME_KEY;
						break;
					case THEME_METACITY:
						gconf_key = METACITY_THEME_KEY;
						break;
					case THEME_ICON:
						gconf_key = ICON_THEME_KEY;
						break;
					default: /* keep gcc happy */
						gconf_key = NULL;
						break;
				}

				gconf_client = gconf_client_get_default ();
				gconf_client_set_string (gconf_client, gconf_key, theme_name, NULL);
				g_object_unref (gconf_client);
			}
		} else {
			dialog = gtk_message_dialog_new (NULL,
			       GTK_DIALOG_MODAL,
			       GTK_MESSAGE_INFO,
			       GTK_BUTTONS_OK,
			       user_message);
			gtk_dialog_run (GTK_DIALOG (dialog));
		}
		gtk_widget_destroy (dialog);
	}

	g_free (user_message);
	g_free (target_dir);

	return success;
}

static void
transfer_done_cb (GtkWidget *dlg, gchar *path)
{
	GtkWidget *dialog;
	gint filetype;

	/* XXX: path should be on the local filesystem by now? */

	if (dlg)
		gtk_widget_destroy (dlg);

	if (g_str_has_suffix (path, ".tar.gz") || g_str_has_suffix (path, ".tgz") || g_str_has_suffix(path, ".gtp"))
		filetype = TARGZ;
	else if (g_str_has_suffix (path, ".tar.bz2"))
		filetype = TARBZ;
	else if (g_file_test (path, G_FILE_TEST_IS_DIR))
		filetype = DIRECTORY;
	else {
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("This theme is not in a supported format."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (path);
		return;
	}

	if (filetype == DIRECTORY) {
		gchar *name = g_path_get_basename (path);
		gnome_theme_install_real (filetype, path, name);
		g_free (name);
	} else {
		/* Create a temp directory and uncompress file there */
		GDir *dir;
		const gchar *name;
		gchar *tmp_dir;
		gboolean ok;

		tmp_dir = g_strdup_printf ("%s/.themes/.theme-%u",
					   g_get_home_dir (),
					   g_random_int ());

		if ((gnome_vfs_make_directory (tmp_dir, 0700)) != GNOME_VFS_OK) {
			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
							GTK_BUTTONS_OK,
							_("Failed to create temporary directory"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			g_free (tmp_dir);
			g_free (path);
			return;
		}


		if (!transfer_done_archive (filetype, tmp_dir, path) ||
		    ((dir = g_dir_open (tmp_dir, 0, NULL)) == NULL))
		{
			cleanup_tmp_dir (tmp_dir);
			g_free (tmp_dir);
			g_free (path);
			return;
		}

		gnome_vfs_unlink (path);

		ok = TRUE;
		for (name = g_dir_read_name (dir); name && ok;
		     name = g_dir_read_name (dir))
		{
			gchar *theme_dir;

			theme_dir = g_build_path (G_DIR_SEPARATOR_S,
						  tmp_dir, name, NULL);

			if (g_file_test (theme_dir, G_FILE_TEST_IS_DIR))
				ok = gnome_theme_install_real (filetype, theme_dir, name);

			g_free (theme_dir);
		}
		g_dir_close (dir);

		cleanup_tmp_dir (tmp_dir);
		g_free (tmp_dir);
	}

	g_free (path);
}

void
gnome_theme_install_from_uri (const gchar *filename, GtkWindow *parent)
{
	GtkWidget *dialog;
	gchar *path, *base;
	GList *src, *target;
	GnomeVFSURI *uri;
	gchar *temppath;
	const gchar *template;
	int cmp;

	if (filename == NULL || strcmp (filename, "") == 0) {
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("No theme file location specified to install"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	/* see if someone dropped a directory */
	if (g_file_test (filename, G_FILE_TEST_IS_DIR))	{
		transfer_done_cb (NULL, g_strdup (filename));
		return;
	}

	/* we can't tell if this is an icon theme yet, so just make a
	 * temporary copy in .themes */
	path = g_build_filename (g_get_home_dir (), ".themes", NULL);

	if (access (path, X_OK | W_OK) != 0) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Insufficient permissions to install the theme in:\n%s"), path);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (path);
		return;
	}

	/* To avoid the copy of /root/.themes to /root/.themes/.themes
	 * which causes an infinite loop. The user asks to transfer the all
	 * contents of a folder, to a folder under itself. So ignore the
	 * situation.
	 */
	temppath = g_build_filename (filename, ".themes", NULL);
	cmp = strcmp (temppath, path);
	g_free (path);
	g_free (temppath);

	if (cmp == 0) {
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("%s is the path where the theme files will be installed. This can not be selected as the source location"), filename);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	uri = gnome_vfs_uri_new (filename);
	base = gnome_vfs_uri_extract_short_name (uri);
	src = g_list_append (NULL, uri);

	if (g_str_has_suffix (base, ".tar.gz") || g_str_has_suffix (base, ".tgz") || g_str_has_suffix (base, ".gtp"))
		template = "gnome-theme-%d.gtp";
	else if (g_str_has_suffix (base, ".tar.bz2"))
		template = "gnome-theme-%d.tar.bz2";
	else {
		dialog = gtk_message_dialog_new (NULL,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					_("The file format is invalid."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (base);
		gnome_vfs_uri_list_unref (src);
		return;
	}
	g_free (base);

	path = NULL;
	do {
	  	gchar *file_tmp;

		g_free (path);
    		file_tmp = g_strdup_printf (template, rand ());
	    	path = g_build_filename (g_get_home_dir (), ".themes", file_tmp, NULL);
	  	g_free (file_tmp);
	} while (g_file_test (path, G_FILE_TEST_EXISTS));


	target = g_list_append (NULL, gnome_vfs_uri_new (path));

	dialog = file_transfer_dialog_new_with_parent (parent);
	file_transfer_dialog_wrap_async_xfer (FILE_TRANSFER_DIALOG (dialog),
					      src, target,
					      GNOME_VFS_XFER_RECURSIVE,
					      GNOME_VFS_XFER_ERROR_MODE_QUERY,
					      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
					      GNOME_VFS_PRIORITY_DEFAULT);
	gnome_vfs_uri_list_unref (src);
	gnome_vfs_uri_list_unref (target);
	g_signal_connect (G_OBJECT (dialog), "cancel",
			  G_CALLBACK (transfer_cancel_cb), path);
	g_signal_connect (G_OBJECT (dialog), "done",
			  G_CALLBACK (transfer_done_cb), path);
	gtk_widget_show (dialog);
}

void
gnome_theme_installer_run (GtkWindow *parent, const gchar *filename)
{
	static gboolean running_theme_install = FALSE;
	static gchar old_folder[512] = "";
	GtkWidget *dialog;
	gchar *filename_selected, *folder;
	GtkFileFilter *filter;

	if (running_theme_install)
		return;

	running_theme_install = TRUE;

	if (filename == NULL)
		filename = old_folder;

	dialog = gtk_file_chooser_dialog_new (_("Select Theme"), parent, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Theme Packages"));
	gtk_file_filter_add_mime_type (filter, "application/x-bzip-compressed-tar");
	gtk_file_filter_add_mime_type (filter, "application/x-compressed-tar");
	gtk_file_filter_add_mime_type (filter, "application/x-gnome-theme-package");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	if (strcmp (old_folder, ""))
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), old_folder);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename_selected = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gnome_theme_install_from_uri (filename_selected, parent);
		g_free (filename_selected);
	}

	folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
	g_strlcpy (old_folder, folder, 255);
	g_free (folder);

	/*
	 * we're relying on the gnome theme info module to pick up changes
	 * to the themes so we don't need to update the model here
	 */

	gtk_widget_destroy (dialog);

	running_theme_install = FALSE;
}
