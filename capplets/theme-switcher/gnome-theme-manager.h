#ifndef __GNOME_THEME_MANAGER_H__
#define __GNOME_THEME_MANAGER_H__

#include <gtk/gtk.h>
#include <glade/glade.h>


#define GTK_THEME_KEY      "/desktop/gnome/interface/gtk_theme"
#define ICON_THEME_KEY     "/desktop/gnome/interface/icon_theme"
#define FONT_KEY	   "/desktop/gnome/interface/font_name"

#define METACITY_THEME_DIR "/apps/metacity/general"
#define METACITY_THEME_KEY METACITY_THEME_DIR "/theme"

extern const char *meta_theme_default_name; 
extern const char *gtk_theme_default_name; 
extern const char *window_theme_default_name; 
extern const char *icon_theme_default_name; 


/* Drag and drop info */
enum
{
  TARGET_URI_LIST,
  TARGET_NS_URL
};

/* model info */
enum
{
  THEME_NAME_COLUMN,
  THEME_ID_COLUMN,
  THEME_FLAG_COLUMN,
  N_COLUMNS
};

enum
{
  THEME_FLAG_DEFAULT = 1 << 0,
  THEME_FLAG_CUSTOM  = 1 << 1
};

extern GtkTargetEntry drop_types[];
extern gint n_drop_types;


/* Prototypes */
GladeXML *gnome_theme_manager_get_theme_dialog          (void);
gint      gnome_theme_manager_sort_func                 (const gchar      *a_str,
							 const gchar      *b_str,
							 guint             a_flag,
							 guint             b_flag);
void      gnome_theme_manager_show_manage_themes        (GtkWidget        *button,
							 gpointer          data);
void      gnome_theme_manager_icon_show_manage_themes        (GtkWidget        *button,
							 gpointer          data);
void      gnome_theme_manager_window_show_manage_themes (GtkWidget        *button,
							 gpointer          data);
gboolean  gnome_theme_manager_drag_motion_cb            (GtkWidget        *widget,
							 GdkDragContext   *context,
							 gint              x,
							 gint              y,
							 guint             time,
							 gpointer          data);
void      gnome_theme_manager_drag_leave_cb             (GtkWidget        *widget,
							 GdkDragContext   *context,
							 guint             time,
							 gpointer          data);
void      gnome_theme_manager_drag_data_received_cb     (GtkWidget        *widget,
							 GdkDragContext   *context,
							 gint              x,
							 gint              y,
							 GtkSelectionData *selection_data,
							 guint             info,
							 guint             time,
							 gpointer          data);




#endif /* __GNOME_THEME_MANAGER_H__ */
