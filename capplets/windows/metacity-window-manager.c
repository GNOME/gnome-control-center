#include "metacity-window-manager.h"

static GnomeWindowManagerClass *parent_class = NULL;

struct _MetacityWindowManagerPrivate {
};

/* this function is called when the shared lib is loaded */
GObject *
window_manager_new (void)
{
  GObject *wm;

  wm = g_object_new (metacity_window_manager_get_type (), NULL);

  return wm;
}

static void     
metacity_set_theme (const char *theme_name)
{

}

static GList *  
metacity_get_theme_list (void)
{
  return NULL;
}

static void     
metacity_set_font (const char *font)
{

}

static gboolean 
metacity_get_focus_follows_mouse (void)
{
  return FALSE;
}

static void     
metacity_set_focus_follows_mouse (gboolean focus_follows_mouse)
{

}

static void
finalize (GObject *object)
{
	MetacityWindowManager *wm;

	wm = METACITY_WINDOW_MANAGER (object);
	if (wm->private == NULL) {
	  return;
	}

	g_free (wm->private);
	wm->private = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (MetacityWindowManagerClass *klass)

{
  GObjectClass *object_class;
  GnomeWindowManagerClass *wm_class;

  object_class = G_OBJECT_CLASS (klass);
  wm_class = GNOME_WINDOW_MANAGER_CLASS (klass);

  object_class->finalize = finalize;

  wm_class->set_theme               = metacity_set_theme;
  wm_class->get_theme_list          = metacity_get_theme_list;
  wm_class->set_font                = metacity_set_font;
  wm_class->get_focus_follows_mouse = metacity_get_focus_follows_mouse;
  wm_class->set_focus_follows_mouse = metacity_set_focus_follows_mouse;

  parent_class = g_type_class_peek_parent (klass);
}

static void
init (MetacityWindowManager *wm)
{
  wm->private = g_new (MetacityWindowManagerPrivate, 1);
}

/* API */
GType
metacity_window_manager_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		GTypeInfo info = {
			sizeof (MetacityWindowManagerClass),
			NULL, NULL, (GClassInitFunc) class_init, NULL, NULL,
			sizeof (MetacityWindowManager), 0, (GInstanceInitFunc) init,
		};

		type = g_type_register_static (METACITY_WINDOW_MANAGER_TYPE, "MetacityWindowManager", &info, 0);
	}

	return type;
}

