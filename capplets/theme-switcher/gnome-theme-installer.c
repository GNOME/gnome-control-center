
#include <config.h>

#include <string.h>
#include <libwindow-settings/gnome-wm-manager.h>
#include "gnome-theme-installer.h"
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gnome-theme-info.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"
#include "gnome-theme-installer.h"

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
			return THEME_ICON;
		}
		
		pattern = g_pattern_spec_new ("*[X-GNOME-Metatheme]*");
		if (g_pattern_match_string(pattern,file_contents)) {
			return THEME_GNOME;
		}
	}
	
	filename = g_strdup_printf ("%s/gtk-2.0/gtkrc",dir);
	src_uri = gnome_vfs_uri_new (filename);
	if (gnome_vfs_uri_exists(src_uri)) {
		return THEME_GTK;
	}
	
	filename = g_strdup_printf ("%s/metacity-1/metacity-theme-1.xml",dir);
	src_uri = gnome_vfs_uri_new (filename);
	if (gnome_vfs_uri_exists (src_uri)) {
		return THEME_METACITY;
	}
	
	
	filename = g_strdup_printf ("%s/configure.in",dir);
	src_uri = gnome_vfs_uri_new (filename);
	if (gnome_vfs_uri_exists (src_uri)) {
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
	gchar *command, *filename;
	theme_properties *theme_props = data;
		
	/* this should be something more clever and nonblocking */
	filename = g_shell_quote(theme_props->filename);
	command = g_strdup_printf ("sh -c 'cd \"%s\"; /bin/gzip -d -c < \"%s\" | /bin/tar xf - '",
				    theme_props->target_tmp_dir, filename);
	g_free(filename);
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0) {
		g_free (command);
		return TRUE;
	} else {	
		g_free (command);
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
	gchar *command, *filename;
	theme_properties *theme_props = data;
	
	filename = g_shell_quote(theme_props->filename);
	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'cd \"%s\"; /usr/bin/bzip2 -d -c < \"%s\" | /bin/tar xf - '",
				   theme_props->target_tmp_dir, filename);
	g_free (filename);
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0) {
		g_free (command);
		return TRUE;
	} else {
		g_free (command);
		return FALSE;
	}
}

static void
transfer_done_cb (GtkWidget *dlg, gchar *path)
{
	GtkWidget *dialog;
	int len = strlen (path);
	gchar *command,**dir, *first_line, *filename;
	int status,theme_type;
	theme_properties *theme_props;
	GnomeVFSURI *theme_source_dir, *theme_dest_dir;
	
	gtk_widget_destroy (dlg);
	
	theme_props = g_new(theme_properties,1);
	
	theme_props->target_tmp_dir = g_strdup_printf ("%s/.themes/.theme-%u", 
				 			g_get_home_dir(), 
							g_random_int());
		
	
	if (path && len > 7 && ( (!strcmp (path + len - 7, ".tar.gz")) || (!strcmp (path + len - 4, ".tgz")) )) {
		filename = g_shell_quote (path);
		command = g_strdup_printf ("sh -c '/bin/gzip -d -c < \"%s\" | /bin/tar ft -  | head -1'",
					    filename);
		theme_props->filetype=TARGZ;
		g_free (filename);
	} else if (path && len > 8 && !strcmp (path + len - 8, ".tar.bz2")) {
		filename = g_shell_quote (path);
		command = g_strdup_printf ("sh -c '/usr/bin/bzip2 -d -c < \"%s\" | /bin/tar ft - | head -1'",
					    filename);
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
		return;	
	}
	
	
	if ((gnome_vfs_make_directory(theme_props->target_tmp_dir,0700)) != GNOME_VFS_OK) {
		GtkWidget *dialog;
				
		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Failed to create temporary directory"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);			
		return;	
	}
	
	/* Uncompress the file in the temp directory */
	theme_props->filename=g_strdup(path);
	
	if (theme_props->filetype == TARBZ ) {
		if (!g_file_test ("/usr/bin/bzip2", G_FILE_TEST_EXISTS)) {
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Can not install theme. \nThe bzip2 utility is not installed."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			gnome_vfs_unlink(path);			
			return;	
		}
		
		if (!transfer_done_tarbz2_idle_cb(theme_props)) {
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
				   			GTK_BUTTONS_OK,
							_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);	
			cleanup_tmp_dir (theme_props);				
			return;	
		}
	}
	
	if (theme_props->filetype == TARGZ ) {
		if (!g_file_test ("/bin/gzip", G_FILE_TEST_EXISTS)) {
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Can not install themes. \nThe gzip utility is not installed."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);			
			gnome_vfs_unlink(path);
			return;	
		}
		if (!transfer_done_targz_idle_cb(theme_props)) {
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
				   			GTK_BUTTONS_OK,
							_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);		
			cleanup_tmp_dir (theme_props);				
			return;	
		}	
	}
	
	/* What type of theme it is ? */	
	if (g_spawn_command_line_sync (command, &first_line, NULL, &status, NULL) && status == 0) {
		dir = g_strsplit(g_strchomp(first_line),"/",0);
		theme_props->theme_tmp_dir=g_strdup(g_build_filename(theme_props->target_tmp_dir,dir[0],NULL));
		
		theme_type = file_theme_type(theme_props->theme_tmp_dir);
		gnome_vfs_unlink (theme_props->filename); 
		if (theme_type == THEME_ICON) {
			theme_props->target_dir=g_strdup_printf("%s/.icons/%s",g_get_home_dir(),dir[0]);	
			theme_props->user_message=g_strdup_printf(_("Icon Theme %s correctly installed.\nYou can select it in the theme details."),dir[0]);
		} else if (theme_type == THEME_GNOME) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
			theme_props->user_message=g_strdup_printf(_("Gnome Theme %s correctly installed"),dir[0]);
		} else if (theme_type == THEME_METACITY) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
			theme_props->user_message=g_strdup_printf(_("Windows Border Theme %s correctly installed.\nYou can select it in the theme details."),dir[0]);
		} else if (theme_type == THEME_GTK) {
			theme_props->target_dir = g_strdup_printf("%s/.themes/%s",g_get_home_dir(),dir[0]);
			theme_props->user_message=g_strdup_printf(_("Controls Theme %s correctly installed.\nYou can select it in the theme details."),dir[0]);
		} else if (theme_type == THEME_ENGINE) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("The theme is an engine. You need to compile the theme."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);		
			cleanup_tmp_dir(theme_props);			
			return;	
		} else {
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("The file format is invalid"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);			
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
			GtkWidget *dialog;
				
			dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Installation Failed"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir(theme_props);
			return;	
		} else {
			GtkWidget *dialog;
						
			dialog = gtk_message_dialog_new (NULL,
			  	       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_INFO,
				       GTK_BUTTONS_OK,
				       theme_props->user_message );
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			cleanup_tmp_dir (theme_props);			
			return;	
		}
		
	}
	g_free(theme_props);
}

static void
install_dialog_response (GtkWidget *widget, int response_id, gpointer data)
{
	GladeXML *dialog = data;
	GtkWidget *dlg;
	gchar *filename, *path, *base;
	GList *src, *target;
	GnomeVFSURI *src_uri;
	const gchar *raw;
	gboolean icon_theme;
	gchar *temppath;

	if (response_id == GTK_RESPONSE_HELP) {
		capplet_help (GTK_WINDOW (widget),
			"user-guide.xml",
			"goscustdesk-12");
		return;
	}

	if (response_id == 0) {
		icon_theme = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "icon_theme"));
		raw = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (WID ("install_theme_picker")))));
		if (raw == NULL || strlen (raw) <= 0)	{
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

		if (strncmp (raw, "http://", 7) && strncmp (raw, "ftp://", 6) && *raw != '/')
			filename = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (WID ("install_theme_picker")), TRUE);
		else
			filename = g_strdup (raw);
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
			return;
                } 
		
		while(TRUE) {
		  	gchar *file_tmp;
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
				gnome_vfs_unlink(path);		
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
			return;
		}
		g_free(temppath);



		target = g_list_append (NULL, gnome_vfs_uri_new (path));

		dlg = file_transfer_dialog_new ();
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
}

void
gnome_theme_installer_run (GtkWidget *parent, gchar *filename)
{
	static gboolean running_theme_install = FALSE;
	GladeXML *dialog;
	GtkWidget *widget;

	if (running_theme_install)
		return;

	running_theme_install = TRUE;

	if (!g_file_test ("/bin/tar", G_FILE_TEST_EXISTS)) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Cannot install theme.\nThe tar program is not installed on your system."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	dialog = glade_xml_new (GLADEDIR "/theme-install.glade", NULL, NULL);
	widget = WID ("install_dialog");

	g_signal_connect (G_OBJECT (widget), "response", G_CALLBACK (install_dialog_response), dialog);
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (parent));
	gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER_ON_PARENT);
	if (filename)
		gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("install_theme_picker")), filename);

	while (gtk_dialog_run (GTK_DIALOG (widget)) == GTK_RESPONSE_HELP)
		;

	gtk_widget_destroy (widget);
	g_object_unref (G_OBJECT (dialog));

	running_theme_install = FALSE;
}
