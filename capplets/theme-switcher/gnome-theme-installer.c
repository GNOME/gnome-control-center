
#include <config.h>

#include <string.h>
#include <libwindow-settings/gnome-wm-manager.h>
#include "gnome-theme-installer.h"
#include <gtk/gtk.h>
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
	gchar *command;
	gchar *path = data;

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'gzip -d -c < \"%s\" | tar xf - -C \"%s/.themes\"'",
				   path, g_get_home_dir ());
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0)
		gnome_vfs_unlink (path);
	g_free (command);
	g_free (path);

	return FALSE;
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
	gchar *command;
	gchar *path = data;

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'bzip2 -d -c < \"%s\" | tar xf - -C \"%s/.themes\"'",
				   path, g_get_home_dir ());
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0)
		gnome_vfs_unlink (path);
	g_free (command);
	g_free (path);

	return FALSE;
}

static void
transfer_done_cb (GtkWidget *dlg, gchar *path)
{
	int len = strlen (path);
	if (path && len > 7 && !strcmp (path + len - 7, ".tar.gz"))
		g_idle_add (transfer_done_targz_idle_cb, path);
	if (path && len > 8 && !strcmp (path + len - 8, ".tar.bz2"))
		g_idle_add (transfer_done_tarbz2_idle_cb, path);
	gtk_widget_destroy (dlg);
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
	
	if (response_id == GTK_RESPONSE_HELP) {
		capplet_help (GTK_WINDOW (widget),
			"wgoscustdesk.xml",
			"goscustdesk-12");
		return;
	}

	if (response_id == 0) {
		raw = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (WID ("install_theme_picker")))));
		if (raw == NULL || strlen (raw) <= 0)
			return;

		if (strncmp (raw, "http://", 7) && strncmp (raw, "ftp://", 6) && *raw != '/')
			filename = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (WID ("install_theme_picker")), TRUE);
		else
			filename = g_strdup (raw);
		if (filename == NULL)
			return;

		src_uri = gnome_vfs_uri_new (filename);
		base = gnome_vfs_uri_extract_short_name (src_uri);
		src = g_list_append (NULL, src_uri);
		path = g_build_filename (g_get_home_dir (), ".themes",
				         base, NULL);
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

	dialog = glade_xml_new (GLADEDIR "/theme-install.glade", NULL, NULL);
	widget = WID ("install_dialog");
	
	g_signal_connect (G_OBJECT (widget), "response",
		G_CALLBACK (install_dialog_response), dialog);
	gtk_window_set_transient_for (GTK_WINDOW (widget), parent);
	gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER_ON_PARENT);
	if (filename)
		gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("install_theme_picker")), filename);

	while (gtk_dialog_run (GTK_DIALOG (widget)) == GTK_RESPONSE_HELP)
		;

	gtk_widget_destroy (widget);
	g_object_unref (G_OBJECT (dialog));

	running_theme_install = FALSE;
}

