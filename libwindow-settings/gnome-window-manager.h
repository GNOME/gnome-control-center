#ifndef GNOME_WINDOW_MANAGER_H
#define GNOME_WINDOW_MANAGER_H

#include <glib-object.h>

#include <libgnome/gnome-desktop-item.h>

typedef GObject * (* GnomeWindowManagerNewFunc) (void);

G_BEGIN_DECLS

#define GNOME_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gnome_window_manager_get_type (), GnomeWindowManager)
#define GNOME_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gnome_window_manager_get_type (), GnomeWindowManagerClass)
#define IS_GNOME_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gnome_window_manager_get_type ())
#define GNOME_WINDOW_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), gnome_window_manager_get_type, GnomeWindowManagerClass))

typedef struct _GnomeWindowManager GnomeWindowManager;
typedef struct _GnomeWindowManagerClass GnomeWindowManagerClass;

typedef struct _GnomeWindowManagerPrivate GnomeWindowManagerPrivate;

struct _GnomeWindowManager
{
  GObject parent;

  GnomeWindowManagerPrivate *p;
};

struct _GnomeWindowManagerClass
{
  GObjectClass klass;

  void         (*set_theme)               (const char *theme_name);
  GList *      (*get_theme_list)          (void);
  void         (*set_font)                (const char *font);
  gboolean     (*get_focus_follows_mouse) (void);
  void         (*set_focus_follows_mouse) (gboolean focus_follows_mouse);

};


GObject *    gnome_window_manager_new                      (GnomeDesktopItem *item);
GType        gnome_window_manager_get_type                 (void);

const char * gnome_window_manager_get_name                 (GnomeWindowManager *wm);
void         gnome_window_manager_set_theme                (GnomeWindowManager *wm, const char *theme_name);
GList *      gnome_window_manager_get_theme_list           (GnomeWindowManager *wm);
void         gnome_window_manager_set_font                 (GnomeWindowManager *wm, const char *font);
gboolean     gnome_window_manager_get_focus_follows_mouse  (GnomeWindowManager *wm);
void         gnome_window_manager_set_focus_follows_mouse  (GnomeWindowManager *wm, gboolean focus_follows_mouse);

G_END_DECLS

#endif /* GNOME_WINDOW_MANAGER_H */
