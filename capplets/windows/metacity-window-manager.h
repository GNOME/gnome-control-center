#ifndef METACITY_WINDOW_MANAGER_H
#define METACITY_WINDOW_MANAGER_H

#include <glib/gerror.h>
#include <glib-object.h>

#include "gnome-window-manager.h"

G_BEGIN_DECLS

#define METACITY_WINDOW_MANAGER_TYPE (metacity_window_manager_get_type ())
#define METACITY_WINDOW_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), METACITY_WINDOW_MANAGER_TYPE, MetacityWindowManager))
#define METACITY_WINDOW_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), METACITY_WINDOW_MANAGER_TYPE, MetacityWindowManagerClass))
#define IS_METACITY_WINDOW_MANAGER(obj) (GTK_TYPE_CHECK_INSTANCE_TYPE ((obj), METACITY_WINDOW_MANAGER_TYPE))
#define IS_METACITY_WINDOW_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), METACITY_WINDOW_MANAGER_TYPE))
#define METACITY_WINDOW_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), METACITY_WINDOW_MANAGER_TYPE, MetacityWindowManagerClass))

typedef struct _MetacityWindowManager MetacityWindowManager;
typedef struct _MetacityWindowManagerClass MetacityWindowManagerClass;
typedef struct _MetacityWindowManagerPrivate MetacityWindowManagerPrivate;

struct _MetacityWindowManager
{
  GnomeWindowManager parent_instance;
  MetacityWindowManagerPrivate *private;
};

struct _MetacityWindowManagerClass
{
  GnomeWindowManagerClass parent_class;
};

GType metacity_window_manager_get_type          (void);

G_END_DECLS

#endif /* METACITY_WINDOW_MANAGER_H */
