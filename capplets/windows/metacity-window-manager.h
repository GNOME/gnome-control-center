#ifndef METACITY_WINDOW_MANAGER_H
#define METACITY_WINDOW_MANAGER_H

#include <glib-object.h>
#include "gnome-window-manager.h"

#define METACITY_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, metacity_window_manager_get_type (), MetacityWindowManager)
#define METACITY_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, metacity_window_manager_get_type (), MetacityWindowManagerClass)
#define IS_METACITY_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, metacity_window_manager_get_type ())

typedef struct _MetacityWindowManager MetacityWindowManager;
typedef struct _MetacityWindowManagerClass MetacityWindowManagerClass;

typedef struct _MetacityWindowManagerPrivate MetacityWindowManagerPrivate;

struct _MetacityWindowManager
{
	GnomeWindowManager parent;
	MetacityWindowManagerPrivate *p;
};

struct _MetacityWindowManagerClass
{
	GnomeWindowManagerClass klass;
};

GType      metacity_window_manager_get_type             (void);

#endif
