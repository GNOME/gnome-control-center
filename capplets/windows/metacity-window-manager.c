#include "metacity-window-manager.h"

static GnomeWindowManagerClass *parent_class;

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
metacity_window_manager_init (MetacityWindowManager *metacity_window_manager, MetacityWindowManagerClass *class)
{
	metacity_window_manager->p = g_new0 (MetacityWindowManagerPrivate, 1);
}

static void
metacity_window_manager_finalize (GObject *object) 
{
	MetacityWindowManager *metacity_window_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_METACITY_WINDOW_MANAGER (object));

	metacity_window_manager = METACITY_WINDOW_MANAGER (object);

	g_free (metacity_window_manager->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
metacity_window_manager_class_init (MetacityWindowManagerClass *class) 
{
	GObjectClass *object_class;
	GnomeWindowManagerClass *wm_class;

	object_class = G_OBJECT_CLASS (class);
	wm_class = GNOME_WINDOW_MANAGER_CLASS (class);

	object_class->finalize = metacity_window_manager_finalize;

	wm_class->set_theme               = metacity_set_theme;
	wm_class->get_theme_list          = metacity_get_theme_list;
	wm_class->set_font                = metacity_set_font;
	wm_class->get_focus_follows_mouse = metacity_get_focus_follows_mouse;
	wm_class->set_focus_follows_mouse = metacity_set_focus_follows_mouse;

	parent_class = g_type_class_peek_parent (class);
}

GType
metacity_window_manager_get_type (void)
{
	static GType metacity_window_manager_type = 0;

	if (!metacity_window_manager_type) {
		static GTypeInfo metacity_window_manager_info = {
			sizeof (MetacityWindowManagerClass),
			NULL, /* GBaseInitFunc */
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) metacity_window_manager_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (MetacityWindowManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) metacity_window_manager_init,
			NULL
		};

		metacity_window_manager_type = 
			g_type_register_static (gnome_window_manager_get_type (), 
						"MetacityWindowManager",
						&metacity_window_manager_info, 0);
	}

	return metacity_window_manager_type;
}


