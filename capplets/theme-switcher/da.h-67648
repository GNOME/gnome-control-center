#include <gnome.h>
#include <gtk-xmhtml/gtk-xmhtml.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

typedef struct _theme_entry
{
  gchar *name;
  gchar *rc;
  gchar *dir;
  gchar *readme;
  gchar *icon;
} ThemeEntry;

void                md(char *s);
int                 exists(char *s);
int                 isfile(char *s);
int                 isdir(char *s);
char              **ls(char *dir, int *num);
void                freestrlist(char **l, int num);
void                rm(char *s);
void                mv(char *s, char *ss);
void                cp(char *s, char *ss);
int                 filesize(char *s);

void
free_theme_list(ThemeEntry *list, gint number);
ThemeEntry *
list_themes(gchar *dir, gint *number);
ThemeEntry *
list_system_themes(gint *number);
ThemeEntry *
list_user_themes(gint *number);
GtkWidget *
make_main(void);
void
update_theme_entries(GtkWidget *disp_list);
void
signal_apply_theme(GtkWidget *widget);
gboolean
edit_file_to_use(gchar *file, gchar *theme, gchar *font);
char *
set_tmp_rc(void);
gboolean
use_theme(gchar *theme, gchar *font);
void
test_theme(gchar *theme, gchar *font);
gchar *
install_theme(gchar *file);
gint
do_demo(int argc, char **argv);
void
send_socket(void);
void
send_reread(void);
void
show_error (const char *string, gboolean fatal);

extern GtkWidget *preview_socket;
extern gint       prog_fd;
extern gchar     *gtkrc_tmp;
