
#ifndef THEME_COMMON_H
#define THEME_COMMON_H

#include <glib.h>

typedef struct _ThemeInfo ThemeInfo;
struct _ThemeInfo
{
  gchar *path;
  gchar *name;
  guint has_gtk : 1;
  guint has_keybinding : 1;
  guint user_writable : 1;
};

void   theme_common_init                  (void);
GList *theme_common_get_list              (void);
void   theme_common_register_theme_change (GFunc    func,
					   gpointer data);



#endif /* THEME_COMMON_H */
