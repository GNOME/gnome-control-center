
#include <config.h>

#include <string.h>
#include <libwindow-settings/gnome-wm-manager.h>
#include "gnome-theme-installer.h"
#include "gnome-theme-details.h"
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib.h>

#include "gnome-theme-info.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"
#include "gnome-theme-installer.h"
#include "gnome-theme-manager.h"

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
	TARBZ
};

typedef struct {
	gint theme_type;
	gint filetype;
	gchar *filename;
	gchar *target_dir;
	gchar *theme_tmp_dir;
	gchar *target_tmp_dir;
	gchar *user_message;
} theme_properties;


static void
cleanup_tmp_dir(theme_properties *theme_props)
{
	GList *list;
	
	if (gnome_vfs_remove_directory (theme_props->target_tmp_dir) == GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY) {
		list = g_list_prepend (NULL, gnome_vfs_uri_new (theme_props->target_tmp_dir));
		gnome_vfs_xfer_delete_list (list, GNOME_VFS_XFER_RECURSIVE,
					GNOME_VFS_XFER_ERROR_MODE_ABORT, NULL, NULL);
		gnome_vfs_uri_list_free(list);
	}
}

static int
file_theme_type(gchar *dir)
{
	gchar *file_contents;
	gchar *filename = NULL;
	gint file_size;
	GPatternSpec *pattern;
	char *uri;
	GnomeVFSURI *src_uri;
	
	filename = g_strdup_printf ("%s/index.theme",dir);
	src_uri = gnome_vfs_uri_new (filename);
	if (gnome_vfs_uri_exists(src_uri)) {
		uri = gnome_vfs_get_uri_from_local_path (filename);
		gnome_vfs_read_entire_file (uri,&file_size,&file_contents);
		
		pattern = g_pattern_spec_new ("*[Icon Theme]*");
		if (g_pattern_match_string(pattern,file_contents)) {
			g_free (filename);
			gnome_vfs_uri_unref (src_uri);
			return THEME_ICON;
		}
		
		pattern = g_pattern_spec_new ("*[X-GNOME-Metatheme]*");
		if (g_pattern_match_string(pattern,file_contents)) {
			g_free (filename);
			gnome_vfs_uri_unref (src_uri);
			return THEME_GNOME;
		}
	}
	g_free (filename);	
	gnome_vfs_uri_unref (src_uri);

	filename = g_strdup_printf ("%s/gtk-2.0/gtkrc",dir);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	if (gnome_vfs_uri_exists(src_uri)) {
		gnome_vfs_uri_unref (src_uri);
		return THEME_GTK;
	}
	
	filename = g_strdup_printf ("%s/metacity-1/metacity-theme-1.xml",dir);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	if (gnome_vfs_uri_exists (src_uri)) {
		gnome_vfs_uri_unref (src_uri);
		return THEME_METACITY;
	}
	
	
	filename = g_strdup_printf ("%s/configure.in",dir);
	src_uri = gnome_vfs_uri_new (filename);
	g_free (filename);
	if (gnome_vfs_uri_exists (src_uri)) {
		gnome_vfs_uri_unref (src_uri);
		return THEME_ENGINE;
	}
	
	return THEME_INVALID;
}

static void
transfer_cancel_cb (GtkWidget *dlg, gchar *path)
{
	gnome_vfs_unlink (path);
	g_free (path);
	gtk_widget_destroy (dlg);
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
transfer_done_targz_idle_cb (gpointer data)
{
	int status;
	gchar *command, *filename, *gzip, *tar;
	theme_properties *theme_props = data;
		
	if (!(gzip = g_find_program_in_path("gzip"))) {
		return FALSE;
	}
	if (!(tar = g_find_program_in_path("tar"))) {
		g_free(gzip);
		return FALSE;
	}
	/* this should be something more clever and nonblocking */
	filename = g_shell_quote(theme_props->filename);
	command = g_strdup_printf ("sh -c 'cd \"%s\"; %s -d -c < \"%s\" | %s xf - '",
				    theme_props->target_tmp_dir, gzip, filename, tar);
	g_free(filename);
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0) {
		g_free (command);
		g_free(gzip);
		g_free(tar);
		return TRUE;
	} else {	
		g_free (command);
		g_free(gzip);
		g_free(tar);
		return FALSE;
	}
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
transfer_done_tarbz2_idle_cb (gpointer data)
{
	int status;
	gchar *command, *filename, *bzip2, *tar;
	theme_properties *theme_props = data;
	
	if (!(bzip2 = g_find_program_in_path("bzip2"))) {
		return FALSE;
	}
	if (!(tar = g_find_program_in_path("tar"))) {
		g_free(bzip2);
		return FALSE;
	}
	filename = g_shell_quote(theme_props->filename);
	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'cd \"%s\"; %s -d -c < \"%s\" | %s xf - '",
				   theme_props->target_tmp_dir, bzip2, filename, tar);
	g_free (filename);
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0) {
		g_free(bzip2);
		g_free(tar);
		g_free (command);
		return TRUE;
	} else {
		g_free(bzip2);
		g_free(tar);
		g_free (command);
		return FALSE;
	}
}

static void
transfer_done_cb (GtkWidget *dlg, gchar *path)
{
	GtkWidget *dialog, *apply_button;
	int len = strlen (path);
	gchar *command,**dir, *first_line, *filename, *gzip, *bzip2, *tar;
	int status,theme_type;
	theme_properties *theme_props;
	GnomeVFSURI *theme_source_dir, *theme_dest_dir;

	gtk_widget_destroy (dlg);
	
	theme_props = g_new0 (theme_properties,1);
	
	theme_props->target_tmp_dir = g_strdup_printf ("%s/.themes/.theme-%u", 
				 			g_get_home_dir(), 
							g_random_int());
		
	
 	gzip = g_find_program_in_path("gzip");
 	bzip2 = g_find_program_in_path("bzip2");
 	tar = g_find_program_in_path("tar");
  	
 	if (tar && gzip && path && len > 7 && ( (!strcmp (path + len - 7, ".tar.gz")) || (!strcmp (path + len - 4, ".tgz")) )) {
  		filename = g_shell_quote (path);
 		command = g_strdup_printf ("sh -c '%s -d -c < \"%s\" | %s ft -  | head -n 1'",
 					    gzip, filename, tar);
  		theme_props->filetype=TARGZ;
  		g_free (filename);
 	} else if (tar && bzip2 && path && len > 8 && !strcmp (path + len - 8, ".tar.bz2")) {
  		filename = g_shell_quote (path);
 		command = g_strdup_printf ("sh -c '%s -d -c < \"%s\" | %s ft - | head -n 1'",
 					    bzip2, filename, tar);
		theme_props->filetype=TARBZ;
		g_free (filename);
	} else {
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("This theme is not in a supported format."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);		
		gnome_vfs_unlink(path);		
		g_free (theme_props->target_tmp_dir);
		g_free (theme_props);
		g_free(gzip);
		g_free(bzip2);
		g_free(tar);
		return;	
	}
	
	
	if ((gnome_vfs_make_directory(theme_props->target_tmp_dir,0700)) != GNOME_VFS_OK) {
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Failed to create temporary directory"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);			
		g_free (command);
		g_free (theme_props->target_tmp_dir);
		g_free (theme_props);
		g_free(gzip);
		g_free(bzip2);
		g_free(tar);
		return;	
	}
	
	/* Uncompress the file in the temp directory */
	theme_props->filename=g_strdup(path);
	
	if (theme_props->filetype == TARBZ ) {
		if (!bzip2) {
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Can not install theme. \nThe bzip2 utility is not installed."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			gnome_vfs_unlink(path);			
			g_free (command);
			g_free (theme_props->target_tmp_dir);
			g_free (theme_props->filename);
			g_free (theme_props);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		}
		
		if (!transfer_done_tarbz2_idle_cb(theme_props)) {
			dialog = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
				   			GTK_BUTTONS_OK,
							_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);	
			cleanup_tmp_dir (theme_props);
			g_free (theme_props->target_tmp_dir);
			g_free (theme_props->filename);
			g_free (theme_props);
			g_free (command);
 			g_free(gzip);
 			g_free(bzip2);
 			g_free(tar);
			return;	
		}
	}
	
	if (theme_props->filetype == TARGZ ) {
		if (!gzip) {
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Can not install themes. \nThe gzip utility is not installed."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			gnome_vfs_unlink(path);
			g_free (command);
			g_free (theme_props->target_tmp_dir);
			g_free (theme_props->filename);
			g_free (theme_props);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		}
		if (!transfer_done_targz_idle_cb(theme_props)) {
			dialog = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
				   			GTK_BUTTONS_OK,
							_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir (theme_props);
			g_free (theme_props->target_tmp_dir);
			g_free (theme_props->filename);
			g_free (theme_props);
			g_free (command);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		}	
	}
	
	/* What type of theme it is ? */	
	if (g_spawn_command_line_sync (command, &first_line, NULL, &status, NULL) && status == 0) {
		dir = g_strsplit(g_strchomp(first_line),"/",0);
		theme_props->theme_tmp_dir=g_build_filename(theme_props->target_tmp_dir,dir[0],NULL);
		
		theme_type = file_theme_type(theme_props->theme_tmp_dir);
		gnome_vfs_unlink (theme_props->filename); 
		if (theme_type == THEME_ICON) {
			theme_props->target_dir=g_strdup_printf("%s/.icons/%s",g_get_home_dir(),dir[0]);	
		} else if (theme_type == THEME_GNOME) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
			theme_props->user_message=g_strdup_printf(_("GNOME Theme %s correctly installed"),dir[0]);
		} else if (theme_type == THEME_METACITY) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
		} else if (theme_type == THEME_GTK) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
		} else if (theme_type == THEME_ENGINE) {
			dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("The theme is an engine. You need to compile the theme."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir(theme_props);
			g_free (theme_props->target_tmp_dir);
			g_free (theme_props->filename);
			g_free (theme_props->theme_tmp_dir);
			g_free (theme_props);
			g_free (command);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		} else {
			dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("The file format is invalid"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
                        g_free (theme_props->target_tmp_dir);
                        g_free (theme_props->filename);
                        g_free (theme_props->theme_tmp_dir);
                        g_free (theme_props);
			g_free (command);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		}
		/* Move the Dir to the target dir */
		theme_source_dir = gnome_vfs_uri_new (theme_props->theme_tmp_dir);
		theme_dest_dir = gnome_vfs_uri_new (theme_props->target_dir);
		
		if (gnome_vfs_xfer_uri (theme_source_dir,theme_dest_dir,
								GNOME_VFS_XFER_DELETE_ITEMS | GNOME_VFS_XFER_RECURSIVE | GNOME_VFS_XFER_REMOVESOURCE,
								GNOME_VFS_XFER_ERROR_MODE_ABORT,
								GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
								NULL,NULL) != GNOME_VFS_OK) {
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir(theme_props);
                        g_free (theme_props->target_tmp_dir);
                        g_free (theme_props->filename);
                        g_free (theme_props->theme_tmp_dir);
			g_free (theme_props->target_dir);
			g_free (theme_props->user_message);
                        g_free (theme_props);
			g_free (command);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		} else {
			/* Ask to apply theme (if we can) */
			if (theme_type == THEME_GTK || theme_type == THEME_METACITY || theme_type == THEME_ICON)
			{
				/* TODO: currently cannot apply "gnome themes" */
				theme_props->user_message=g_strdup_printf(_("<span weight=\"bold\" size=\"larger\">The theme \"%s\" has been installed.</span>\n\nWould you like to apply it now, or keep your current theme?"),dir[0]);
				dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, theme_props->user_message );

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
					GConfClient * gconf_client;
					gconf_client = gconf_client_get_default ();
					switch (theme_type)
					{
						case THEME_GTK: gconf_client_set_string (gconf_client, GTK_THEME_KEY, dir[0], NULL); break;
						case THEME_METACITY: gconf_client_set_string (gconf_client, METACITY_THEME_KEY, dir[0], NULL); break;
						case THEME_ICON: gconf_client_set_string (gconf_client, ICON_THEME_KEY, dir[0], NULL); break;
					}

					g_object_unref (gconf_client);
				}
			} else
			{
				dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_INFO,
				       GTK_BUTTONS_OK,
				       theme_props->user_message );
				gtk_dialog_run (GTK_DIALOG (dialog));
			}
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir (theme_props);			
                        g_free (theme_props->target_tmp_dir);
                        g_free (theme_props->filename);
                        g_free (theme_props->theme_tmp_dir);
			g_free (theme_props->target_dir);
			g_free (theme_props->user_message);
                        g_free (theme_props);
			g_free (command);
			g_free(gzip);
			g_free(bzip2);
			g_free(tar);
			return;	
		}
		
	}
	g_free (command);
	g_free (theme_props->target_tmp_dir);
	g_free (theme_props->filename);
	g_free (theme_props);
	g_free(gzip);
	g_free(bzip2);
	g_free(tar);
}

void
gnome_theme_install_from_uri (gchar * theme_filename, GtkWindow *parent)
{
	GtkWidget *dlg;
	gchar *filename, *path, *base;
	GList *src, *target;
	GnomeVFSURI *src_uri;
	gboolean icon_theme = FALSE;
	gchar *temppath;


		if (theme_filename == NULL || strlen (theme_filename) <= 0)	{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("No theme file location specified to install"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			return;
		}

		filename = g_strdup (theme_filename);
		
		if (filename == NULL)	{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("The theme file location specified to install is invalid"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			return;
		}

		src_uri = gnome_vfs_uri_new (filename);
		base = gnome_vfs_uri_extract_short_name (src_uri);
		src = g_list_append (NULL, src_uri);
		if (icon_theme)
			path = g_build_filename (g_get_home_dir (), ".icons", NULL);
		else
			path = g_build_filename (g_get_home_dir (), ".themes", NULL);

		if (access (path, X_OK | W_OK) != 0) {
                        GtkWidget *dialog;

                        dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                                          GTK_MESSAGE_ERROR,
                                                          GTK_BUTTONS_OK,
                                                          _("Insufficient permissions to install the theme in:\n%s"), path);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			g_free (path);
			return;
                } 
		
		while(TRUE) {
		  	gchar *file_tmp;
			GtkWidget *dialog;
		  	int len = strlen (base);
		  
		  	if (base && len > 7 && ( (!strcmp (base + len - 7, ".tar.gz")) || (!strcmp (base + len - 4, ".tgz")) ))
		    		file_tmp = g_strdup_printf("gnome-theme-%d.tar.gz", rand ());
		  	else if (base && len > 8 && !strcmp (base + len - 8, ".tar.bz2"))
		    		file_tmp = g_strdup_printf("gnome-theme-%d.tar.bz2", rand ());
		  	else {
				dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("The file format is invalid."));
				gtk_dialog_run (GTK_DIALOG (dialog));
				gtk_widget_destroy (dialog);		
				g_free (path);		
		    		return;
			}
		  
		    	path = g_build_filename (g_get_home_dir (), ".themes", file_tmp, NULL);
		  
		  	g_free(file_tmp);
		  	if (!gnome_vfs_uri_exists (gnome_vfs_uri_new (path)))
		    		break;
		}
		
		/* To avoid the copy of /root/.themes to /root/.themes/.themes
		 * which causes an infinite loop. The user asks to transfer the all
		 * contents of a folder, to a folder under itseld. So ignore the
		 * situation.
		 */
		temppath = g_build_filename (filename, ".themes", NULL);
		if (!strcmp(temppath, path))	{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("%s is the path where the theme files will be installed. This can not be selected as the source location"), filename);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			
			g_free (base);
			g_free (filename);
			g_free(temppath);
			g_free (path);
			return;
		}
		g_free(temppath);



		target = g_list_append (NULL, gnome_vfs_uri_new (path));

		dlg = file_transfer_dialog_new_with_parent (parent);
		file_transfer_dialog_wrap_async_xfer (FILE_TRANSFER_DIALOG (dlg),
						      src, target,
						      GNOME_VFS_XFER_RECURSIVE,
						      GNOME_VFS_XFER_ERROR_MODE_QUERY,
						      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
						      GNOME_VFS_PRIORITY_DEFAULT);
		gnome_vfs_uri_list_unref (src);
		gnome_vfs_uri_list_unref (target);
		g_free (base);
		g_free (filename);
		g_signal_connect (G_OBJECT (dlg), "cancel",
				  G_CALLBACK (transfer_cancel_cb), path);
		g_signal_connect (G_OBJECT (dlg), "done",
				  G_CALLBACK (transfer_done_cb), path);
		gtk_widget_show (dlg);
}

void
gnome_theme_installer_run_cb (GtkWidget *button,
				   GtkWindow *parent_window)
{
  gnome_theme_installer_run (parent_window, NULL);
}


void
gnome_theme_installer_run (GtkWindow *parent, gchar *filename)
{
	static gboolean running_theme_install = FALSE;
	GtkWidget *dialog;
	static char old_folder[1024] = "";
	gchar *filename_selected, *folder;

	if (filename == NULL)
		filename = old_folder;

	if (running_theme_install)
		return;

	running_theme_install = TRUE;

	dialog = gtk_file_chooser_dialog_new ("Select Theme", parent, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

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

	gnome_theme_details_reread_themes_from_disk();


	gtk_widget_destroy (dialog);

	running_theme_install = FALSE;
}
