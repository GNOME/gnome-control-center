
#ifndef THEME_COMMON_H
#define THEME_COMMON_H

#include <glib.h>

typedef struct _ThemeInfo ThemeInfo;
struct _ThemeInfo
{
  gchar *path;
  gchar *name;
  gboolean has_gtk;
  gboolean has_keybinding;
};

GList *theme_common_get_list  (void);
void   theme_common_list_free (GList *list);


#endif /* THEME_COMMON_H */
