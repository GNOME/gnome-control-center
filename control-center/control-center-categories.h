#include <libgnome/gnome-desktop-item.h>

typedef struct ControlCenterCategory_ ControlCenterCategory;

typedef struct {
  GnomeDesktopItem *desktop_entry;
  char *icon;
  const char *title;
  const char *comment;
  char *name;
  ControlCenterCategory *category;
  gpointer user_data;
} ControlCenterEntry;

struct ControlCenterCategory_ {
  int count;
  ControlCenterEntry **entries;
  GnomeDesktopItem *directory_entry;
  const char *title;
  char *name;
  gpointer user_data;
  gboolean real_category;
};

typedef struct {
  int count;
  ControlCenterCategory **categories;
  GnomeDesktopItem *directory_entry;
  const char *title;
} ControlCenterInformation;

ControlCenterInformation *control_center_get_categories (const gchar *prefsuri); /* Of type Category * */
