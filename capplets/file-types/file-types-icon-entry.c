/* NautilusMimeIconEntry widget - Combo box with "Browse" button for files and
 *			   A pick button which can display a list of icons
 *			   in a current directory, the browse button displays
 *			   same dialog as pixmap-entry
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: George Lebl <jirka@5z.com>
 * icon selection based on original dentry-edit code which was:
 *	Written by: Havoc Pennington, based on code by John Ellis.
 */
#include <config.h>

#include "file-types-icon-entry.h"

#include <gdk_imlib.h>
#include <gnome.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkscrolledwindow.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>



static void nautilus_mime_type_icon_entry_class_init (GnomeIconEntryClass *class);
static void nautilus_mime_type_icon_entry_init       (NautilusMimeIconEntry      *ientry);

static GtkVBoxClass *parent_class;

static GtkTargetEntry drop_types[] = { { "text/uri-list", 0, 0 } };

GType
nautilus_mime_type_icon_entry_get_type (void)
{
	static GType icon_entry_type = 0;

	if (!icon_entry_type) {
		GtkTypeInfo icon_entry_info = {
			"NautilusMimeIconEntry",
			sizeof (NautilusMimeIconEntry),
			sizeof (GnomeIconEntryClass),
			(GtkClassInitFunc) nautilus_mime_type_icon_entry_class_init,
			(GtkObjectInitFunc) nautilus_mime_type_icon_entry_init,
			NULL,
			NULL
		};

		icon_entry_type = gtk_type_unique (gtk_vbox_get_type (),
						   &icon_entry_info);
	}

	return icon_entry_type;
}

static void
nautilus_mime_type_icon_entry_class_init (GnomeIconEntryClass *class)
{
	parent_class = gtk_type_class (gtk_hbox_get_type ());
}

static void
entry_changed(GtkWidget *widget, NautilusMimeIconEntry *ientry)
{
	gchar *t;
	GtkWidget *child;
	int w,h;

	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));

	t = gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(ientry->fentry),
					   FALSE);

	child = GTK_BIN(ientry->frame)->child;
	
	if(GNOME_IS_PIXMAP (child)) {
		gnome_pixmap_load_file (GNOME_PIXMAP(child), t);
	} else {
		if (child != NULL) {
			gtk_widget_destroy (child);
		}
		
		child = gnome_pixmap_new_from_file (t);
		gtk_widget_show (child);
		gtk_container_add (GTK_CONTAINER(ientry->frame), child);

		if(!GTK_WIDGET_NO_WINDOW(child)) {
			gtk_drag_source_set (child,
					     GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
					     drop_types, 1,
					     GDK_ACTION_COPY);
		}
	}
	
	/*gtk_drag_source_set (ientry->frame,
			     GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
			     drop_types, 1,
			     GDK_ACTION_COPY);
	*/			     
}

static void
entry_activated(GtkWidget *widget, NautilusMimeIconEntry *ientry)
{
	struct stat buf;
	GnomeIconSelection * gis;
	gchar *filename;
	GtkButton *OK_button;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_ENTRY (widget));
	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));

	filename = gtk_entry_get_text (GTK_ENTRY (widget));

	if (!filename)
		return;

	stat (filename, &buf);
	if (S_ISDIR (buf.st_mode)) {
		gis = gtk_object_get_user_data(GTK_OBJECT(ientry));
		gnome_icon_selection_clear (gis, TRUE);
		gnome_icon_selection_add_directory (gis, filename);
/*		if (gis->file_list) */
			gnome_icon_selection_show_icons(gis);
	} else {
		/* FIXME: This is a hack to act exactly like we've clicked the
		 * OK button. This should be structured more cleanly.
		 */
		OK_button = GTK_BUTTON (GNOME_DIALOG (ientry->pick_dialog)->buttons->data);
		gtk_button_clicked (OK_button);
	}
}

static void
setup_preview(GtkWidget *widget)
{
	gchar *p;
	GList *l;
	GtkWidget *pp = NULL;
	int w,h;
	GtkWidget *frame;
	GtkFileSelection *fs;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));

	frame = gtk_object_get_data(GTK_OBJECT(widget),"frame");
	fs = gtk_object_get_data(GTK_OBJECT(frame),"fs");

	if((l = gtk_container_children(GTK_CONTAINER(frame))) != NULL) {
		pp = l->data;
		g_list_free(l);
	}

	if(pp)
		gtk_widget_destroy(pp);
	
	p = gtk_file_selection_get_filename(fs);
	if(!p || !g_file_test (p,G_FILE_TEST_IS_SYMLINK|G_FILE_TEST_IS_REGULAR))
		return;

	pp = gnome_pixmap_new_from_file (p);
	gtk_widget_show(pp);
	gtk_container_add(GTK_CONTAINER(frame),pp);
}

static void
ientry_destroy(NautilusMimeIconEntry *ientry)
{
	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));

	if(ientry->fentry)
		gtk_widget_unref (ientry->fentry);
	if(ientry->pick_dialog)
		gtk_widget_destroy(ientry->pick_dialog);
	g_free(ientry->pick_dialog_dir);
}


static void
browse_clicked (GnomeFileEntry *fentry, NautilusMimeIconEntry *ientry)
{
	GtkWidget *w;
	GtkWidget *hbox;

	GtkFileSelection *fs;
		
	g_return_if_fail (fentry != NULL);
	g_return_if_fail (GNOME_IS_FILE_ENTRY (fentry));
	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));

	if(!fentry->fsw)
		return;
	fs = GTK_FILE_SELECTION(fentry->fsw);
	
	hbox = fs->file_list;
	do {
		hbox = hbox->parent;
		if(!hbox) {
			g_warning(_("Can't find an hbox, using a normal file "
				    "selection"));
			return;
		}
	} while(!GTK_IS_HBOX(hbox));

	w = gtk_frame_new(_("Preview"));
	gtk_widget_show(w);
	gtk_box_pack_end(GTK_BOX(hbox),w,FALSE,FALSE,0);
	gtk_widget_set_usize(w,110,110);
	gtk_object_set_data(GTK_OBJECT(w),"fs",fs);
	
	gtk_object_set_data(GTK_OBJECT(fs->file_list),"frame",w);
	gtk_signal_connect(GTK_OBJECT(fs->file_list),"select_row",
			   GTK_SIGNAL_FUNC(setup_preview),NULL);
	gtk_object_set_data(GTK_OBJECT(fs->selection_entry),"frame",w);
	gtk_signal_connect_while_alive(GTK_OBJECT(fs->selection_entry),
				       "changed",
				       GTK_SIGNAL_FUNC(setup_preview),NULL,
				       GTK_OBJECT(fs));				       				      
}

static void
cancel_pressed (GtkButton * button, NautilusMimeIconEntry * icon_entry)
{
	GnomeIconSelection * gis;

	g_return_if_fail (icon_entry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (icon_entry));

	gis =  gtk_object_get_user_data(GTK_OBJECT(icon_entry));
	gnome_icon_selection_stop_loading(gis);
}


void
nautilus_mime_type_show_icon_selection (NautilusMimeIconEntry *icon_entry)
{
	GnomeFileEntry *fe;
	gchar *p;
	gchar *curfile;
	GtkWidget *tl;

	g_return_if_fail (icon_entry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (icon_entry));

	fe = GNOME_FILE_ENTRY (icon_entry->fentry);
	p = gnome_file_entry_get_full_path (fe, FALSE);
	curfile = nautilus_mime_type_icon_entry_get_full_filename (icon_entry);

	/* Are we part of a modal window?  If so, we need to be modal too. */
	tl = gtk_widget_get_toplevel (GTK_WIDGET (icon_entry->frame));
	
	if (!p) {
		if (fe->default_path) {
			p = g_strdup (fe->default_path);
		} else {
			/* get around the g_free/free issue */
			gchar *cwd = g_get_current_dir ();
			p = g_strdup (cwd);
			g_free (cwd);
		}
		gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (icon_entry->fentry))),
				    p);
	}

	/* figure out the directory */
	if (!g_file_test (p, G_FILE_TEST_IS_DIR)) {
		gchar *d;
		d = g_dirname (p);
		g_free (p);
		p = d;
		if (!g_file_test (p, G_FILE_TEST_IS_DIR)) {
			g_free (p);
			if (fe->default_path) {
				p = g_strdup (fe->default_path);
			} else {
				/*get around the g_free/free issue*/
				gchar *cwd = g_get_current_dir ();
				p = g_strdup (cwd);
				free(cwd);
			}
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (icon_entry->fentry))),
				    p);
			g_return_if_fail (g_file_test (p,G_FILE_TEST_IS_DIR));
		}
	}
	

	if (icon_entry->pick_dialog == NULL || icon_entry->pick_dialog_dir == NULL ||
	    strcmp(p, icon_entry->pick_dialog_dir) != 0) {
		GtkWidget *iconsel;
		
		if (icon_entry->pick_dialog) {
			gtk_container_remove (GTK_CONTAINER (icon_entry->fentry->parent), icon_entry->fentry);
			gtk_widget_destroy (icon_entry->pick_dialog);
		}
		
		g_free(icon_entry->pick_dialog_dir);
		icon_entry->pick_dialog_dir = NULL;

		icon_entry->pick_dialog_dir = p;
		icon_entry->pick_dialog = 
			gnome_dialog_new("",
					 GNOME_STOCK_BUTTON_OK,
					 GNOME_STOCK_BUTTON_CANCEL,
					 NULL);
		if (GTK_WINDOW (tl)->modal) {
			gtk_window_set_modal (GTK_WINDOW (icon_entry->pick_dialog), TRUE);
			gnome_dialog_set_parent (GNOME_DIALOG (icon_entry->pick_dialog), GTK_WINDOW (tl)); 
		}
		gnome_dialog_close_hides(GNOME_DIALOG(icon_entry->pick_dialog), TRUE);
		gnome_dialog_set_close  (GNOME_DIALOG(icon_entry->pick_dialog), TRUE);

		gtk_window_set_policy(GTK_WINDOW(icon_entry->pick_dialog), 
				      TRUE, TRUE, TRUE);

		iconsel = gnome_icon_selection_new();

		gtk_object_set_user_data(GTK_OBJECT(icon_entry), iconsel);

		gnome_icon_selection_add_directory (GNOME_ICON_SELECTION(iconsel), icon_entry->pick_dialog_dir);

		gtk_window_set_title (GTK_WINDOW (icon_entry->pick_dialog), _("Select an icon"));

		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (icon_entry->pick_dialog)->vbox),
				    icon_entry->fentry, FALSE, FALSE, 0);
		
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(icon_entry->pick_dialog)->vbox),
				   iconsel, TRUE, TRUE, 0);

		gtk_widget_show_all(icon_entry->pick_dialog);
		
		gnome_icon_selection_show_icons(GNOME_ICON_SELECTION(iconsel));

		if(curfile)
			gnome_icon_selection_select_icon(GNOME_ICON_SELECTION(iconsel), 
							 g_basename(curfile));

		/* FIXME:
		 * OK button is handled by caller, Cancel button is handled here.
		 * This could be cleaned up further.
		 */
		gnome_dialog_button_connect(GNOME_DIALOG(icon_entry->pick_dialog), 
					    1, /* Cancel button */
					    GTK_SIGNAL_FUNC(cancel_pressed),
					    icon_entry);

	} else {
		GnomeIconSelection *gis =
			gtk_object_get_user_data(GTK_OBJECT(icon_entry));
		if(!GTK_WIDGET_VISIBLE(icon_entry->pick_dialog))
			gtk_widget_show(icon_entry->pick_dialog);
		if(gis) {
			gnome_icon_selection_show_icons(gis);
		}
	}
}

gchar *
nautilus_mime_type_icon_entry_get_relative_filename (NautilusMimeIconEntry *ientry)
{
  	char *filename;
  	char *result;
  	char **path_parts;

	result = NULL;
	filename = nautilus_mime_type_icon_entry_get_full_filename (NAUTILUS_MIME_ICON_ENTRY (ientry));
	if (filename != NULL) {
		path_parts = g_strsplit (filename, "/share/pixmaps/", 0);
		g_free (filename);

		if (path_parts[1] != NULL) {
			result = g_strdup (path_parts[1]);
		}

		g_strfreev (path_parts);
	}

	return result;
}

static void
nautilus_mime_type_icon_entry_init (NautilusMimeIconEntry *ientry)
{
	GtkWidget *w;
	gchar *p;

	gtk_box_set_spacing (GTK_BOX (ientry), 4);
	
	gtk_signal_connect(GTK_OBJECT(ientry),"destroy",
			   GTK_SIGNAL_FUNC(ientry_destroy), NULL);
	
	ientry->pick_dialog = NULL;
	ientry->pick_dialog_dir = NULL;
	
	w = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(w);
	gtk_box_pack_start (GTK_BOX (ientry), w, TRUE, TRUE, 0);
	ientry->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (ientry->frame), GTK_SHADOW_IN);
	gtk_drag_dest_set (GTK_WIDGET (ientry->frame),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, 1, GDK_ACTION_COPY);

	/*60x60 is just larger then default 48x48, though icon sizes
	  are supposed to be selectable I guess*/
	gtk_widget_set_usize (ientry->frame,60,60);
	gtk_container_add (GTK_CONTAINER (w), ientry->frame);
	gtk_widget_show (ientry->frame);

	ientry->fentry = gnome_file_entry_new (NULL,NULL);
	gnome_file_entry_set_modal (GNOME_FILE_ENTRY (ientry->fentry), TRUE);
	gtk_widget_ref (ientry->fentry);
	gtk_signal_connect_after(GTK_OBJECT(ientry->fentry),"browse_clicked",
				 GTK_SIGNAL_FUNC(browse_clicked),
				 ientry);

	gtk_widget_show (ientry->fentry);
	
	p = gnome_pixmap_file (".");
	gnome_file_entry_set_default_path (GNOME_FILE_ENTRY(ientry->fentry), p);
	g_free (p);
	
	w = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(ientry->fentry));
/*	gtk_signal_connect_while_alive(GTK_OBJECT(w), "changed",
				       GTK_SIGNAL_FUNC(entry_changed),
				       ientry, GTK_OBJECT(ientry));*/
	gtk_signal_connect_while_alive(GTK_OBJECT(w), "activate",
				       GTK_SIGNAL_FUNC(entry_activated),
				       ientry, GTK_OBJECT(ientry));
	
	
	/*just in case there is a default that is an image*/
	entry_changed(w,ientry);
}

/**
 * nautilus_mime_type_icon_entry_new:
 * @history_id: the id given to #gnome_entry_new
 * @browse_dialog_title: title of the browse dialog and icon selection dialog
 *
 * Description: Creates a new icon entry widget
 *
 * Returns: Returns the new object
 **/
GtkWidget *
nautilus_mime_type_icon_entry_new (const gchar *history_id, const gchar *browse_dialog_title)
{
	NautilusMimeIconEntry *ientry;
	GtkWidget *gentry;

	ientry = gtk_type_new (nautilus_mime_type_icon_entry_get_type ());
	
        /* Keep in sync with gnome_entry_new() - or better yet, 
           add a _construct() method once we are in development
           branch. 
        */

	gentry = gnome_file_entry_gnome_entry(GNOME_FILE_ENTRY(ientry->fentry));

	gnome_entry_set_history_id (GNOME_ENTRY (gentry), history_id);
	/* gnome_entry_load_history (GNOME_ENTRY (gentry)); */
	gnome_file_entry_set_title (GNOME_FILE_ENTRY(ientry->fentry),
				    browse_dialog_title);
	
	return GTK_WIDGET (ientry);
}

/**
 * nautilus_mime_type_icon_entry_gnome_file_entry:
 * @ientry: the NautilusMimeIconEntry to work with
 *
 * Description: Get the GnomeFileEntry widget that's part of the entry
 *
 * Returns: Returns GnomeFileEntry widget
 **/
GtkWidget *
nautilus_mime_type_icon_entry_gnome_file_entry (NautilusMimeIconEntry *ientry)
{
	g_return_val_if_fail (ientry != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry), NULL);

	return ientry->fentry;
}

/**
 * nautilus_mime_type_icon_entry_gnome_entry:
 * @ientry: the NautilusMimeIconEntry to work with
 *
 * Description: Get the GnomeEntry widget that's part of the entry
 *
 * Returns: Returns GnomeEntry widget
 **/
GtkWidget *
nautilus_mime_type_icon_entry_gnome_entry (NautilusMimeIconEntry *ientry)
{
	g_return_val_if_fail (ientry != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry), NULL);

	return gnome_file_entry_gnome_entry(GNOME_FILE_ENTRY(ientry->fentry));
}

/**
 * nautilus_mime_type_icon_entry_gtk_entry:
 * @ientry: the NautilusMimeIconEntry to work with
 *
 * Description: Get the GtkEntry widget that's part of the entry
 *
 * Returns: Returns GtkEntry widget
 **/
GtkWidget *
nautilus_mime_type_icon_entry_gtk_entry (NautilusMimeIconEntry *ientry)
{
	g_return_val_if_fail (ientry != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry), NULL);

	return gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (ientry->fentry));
}

/**
 * nautilus_mime_type_icon_entry_set_pixmap_subdir:
 * @ientry: the NautilusMimeIconEntry to work with
 * @subdir: subdirectory
 *
 * Description: Sets the subdirectory below gnome's default
 * pixmap directory to use as the default path for the file
 * entry.
 *
 * Returns:
 **/
void
nautilus_mime_type_icon_entry_set_pixmap_subdir(NautilusMimeIconEntry *ientry, const gchar *subdir)
{
	gchar *p;
	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));
	
	if(!subdir)
		subdir = ".";

	p = gnome_pixmap_file (subdir);
	gnome_file_entry_set_default_path(GNOME_FILE_ENTRY(ientry->fentry),p);
	g_free(p);
}

/**
 * nautilus_mime_type_icon_entry_set_icon:
 * @ientry: the NautilusMimeIconEntry to work with
 * @filename: a filename
 * 
 * Description: Sets the icon of NautilusMimeIconEntry to be the one pointed to by
 * @filename (in the current subdirectory).
 *
 * Returns:
 **/
void
nautilus_mime_type_icon_entry_set_icon (NautilusMimeIconEntry *ientry, const gchar *filename)
{
	g_return_if_fail (ientry != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry));
	
	if(!filename) {
		filename = "";
	}

	gtk_entry_set_text (GTK_ENTRY (nautilus_mime_type_icon_entry_gtk_entry (ientry)), filename);
	entry_changed (NULL, ientry);
}

/**
 * nautilus_mime_type_icon_entry_get_full_filename:
 * @ientry: the NautilusMimeIconEntry to work with
 *
 * Description: Gets the file name of the image if it was possible
 * to load it into the preview. That is, it will only return a filename
 * if the image exists and it was possible to load it as an image.
 *
 * Returns: a newly allocated string with the path or %NULL if it
 * couldn't load the file
 **/
gchar *
nautilus_mime_type_icon_entry_get_full_filename (NautilusMimeIconEntry *ientry)
{
	GtkWidget *child;

	g_return_val_if_fail (ientry != NULL,NULL);
	g_return_val_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (ientry),NULL);

	child = GTK_BIN(ientry->frame)->child;
	
	/* this happens if it doesn't exist or isn't an image */
	if (!GNOME_IS_PIXMAP (child)) {
		return NULL;
	}
	
	return gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(ientry->fentry),
					      TRUE);
}
