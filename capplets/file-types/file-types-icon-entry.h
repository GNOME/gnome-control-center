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

#ifndef NAUTILUS_MIME_TYPE_ICON_ENTRY_H
#define NAUTILUS_MIME_TYPE_ICON_ENTRY_H

#include <gtk/gtkframe.h>
#include <gtk/gtkvbox.h>
#include <libgnomeui/gnome-file-entry.h>


G_BEGIN_DECLS


#define NAUTILUS_TYPE_MIME_ICON_ENTRY	 	 (nautilus_mime_type_icon_entry_get_type ())
#define NAUTILUS_MIME_ICON_ENTRY(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_MIME_ICON_ENTRY, NautilusMimeIconEntry))
#define NAUTILUS_MIME_ICON_ENTRY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MIME_ICON_ENTRY, NautilusMimeIconEntryClass))
#define NAUTILUS_MIME_IS_ICON_ENTRY(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_MIME_ICON_ENTRY))
#define NAUTILUS_MIME_IS_ICON_ENTRY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_MIME_ICON_ENTRY))


typedef struct _NautilusMimeIconEntry      NautilusMimeIconEntry;
typedef struct _NautilusMimeIconEntryClass NautilusMimeIconEntryClass;

struct _NautilusMimeIconEntry {
	GtkVBox vbox;
	
	GtkWidget *fentry;
	GtkWidget *frame;
	
	GtkWidget *pick_dialog;
	gchar *pick_dialog_dir;
};

struct _NautilusMimeIconEntryClass {
	GtkVBoxClass parent_class;
};


GType      nautilus_mime_type_icon_entry_get_type    (void);
GtkWidget *nautilus_mime_type_icon_entry_new         (const gchar *history_id,
					 const gchar *browse_dialog_title);

/*by default gnome_pixmap entry sets the default directory to the
  gnome pixmap directory, this will set it to a subdirectory of that,
  or one would use the file_entry functions for any other path*/
void       nautilus_mime_type_icon_entry_set_pixmap_subdir(NautilusMimeIconEntry *ientry,
					      const gchar *subdir);
void       nautilus_mime_type_icon_entry_set_icon(NautilusMimeIconEntry *ientry,
				     const gchar *filename);
GtkWidget *nautilus_mime_type_icon_entry_gnome_file_entry(NautilusMimeIconEntry *ientry);
GtkWidget *nautilus_mime_type_icon_entry_gnome_entry (NautilusMimeIconEntry *ientry);
GtkWidget *nautilus_mime_type_icon_entry_gtk_entry   (NautilusMimeIconEntry *ientry);

/*only return a file if it was possible to load it with imlib*/
gchar      *nautilus_mime_type_icon_entry_get_full_filename	(NautilusMimeIconEntry *ientry);
gchar      *nautilus_mime_type_icon_entry_get_relative_filename	(NautilusMimeIconEntry *ientry);
void	    nautilus_mime_type_show_icon_selection 	(NautilusMimeIconEntry * ientry);

G_END_DECLS

#endif
