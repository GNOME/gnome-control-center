#ifndef GNOME_WINDOW_MANAGER_H
#define GNOME_WINDOW_MANAGER_H

#include <glib/gerror.h>
#include <glib-object.h>

#include <libgnome/gnome-desktop-item.h>

typedef GObject * (* GnomeWindowManagerNewFunc) (void);

G_BEGIN_DECLS

#define GNOME_WINDOW_MANAGER_TYPE (gnome_window_manager_get_type ())
#define GNOME_WINDOW_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_WINDOW_MANAGER_TYPE, GnomeWindowManager))
#define GNOME_WINDOW_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_WINDOW_MANAGER_TYPE, GnomeWindowManagerClass))
#define IS_GNOME_WINDOW_MANAGER(obj) (GTK_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_WINDOW_MANAGER_TYPE))
#define IS_GNOME_WINDOW_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_WINDOW_MANAGER_TYPE))
#define GNOME_WINDOW_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_WINDOW_MANAGER_TYPE, GnomeWindowManagerClass))

#define GNOME_WINDOW_MANAGER_ERROR gnome_window_manager_error_quark ()

typedef struct _GnomeWindowManager GnomeWindowManager;
typedef struct _GnomeWindowManagerClass GnomeWindowManagerClass;
typedef struct _GnomeWindowManagerPrivate GnomeWindowManagerPrivate;

struct _GnomeWindowManager
{
  GObject parent_instance;
};

struct _GnomeWindowManagerClass
{
  GObjectClass parent_class;

  void     (*set_theme)               (const char *theme_name);
  GList *  (*get_theme_list)          (void);
  void     (*set_font)                (const char *font);
  gboolean (*get_focus_follows_mouse) (void);
  void     (*set_focus_follows_mouse) (gboolean focus_follows_mouse);
};

GObject *gnome_window_manager_new                     (GnomeDesktopItem *item);

GType    gnome_window_manager_get_type                (void);
void     gnome_window_manager_set_theme               (const char *theme_name);
GList *  gnome_window_manager_get_theme_list          (void);
void     gnome_window_manager_set_font                (const char *font);
gboolean gnome_window_manager_get_focus_follows_mouse (void);
void     gnome_window_manager_set_focus_follows_mouse (gboolean focus_follows_mouse);

G_END_DECLS

#endif /* GNOME_WINDOW_MANAGER_H */
