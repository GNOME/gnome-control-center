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
unsigned long       moddate(char *s);
int                 filesize(char *s);
void                cd(char *s);
char               *cwd(void);
int                 permissions(char *s);
int                 owner(char *s);
int                 group(char *s);
char               *username(int uid);
char               *homedir(int uid);
char               *usershell(int uid);
char               *atword(char *s, int num);
char               *atchar(char *s, char c);
void                word(char *s, int num, char *wd);
int                 canread(char *s);
int                 canwrite(char *s);
int                 canexec(char *s);
char               *fileof(char *s);
char               *fullfileof(char *s);
char               *noext(char *s);
void                mkdirs(char *s);

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
click_entry(GtkWidget *widget, gpointer data);
void
delete_entry(GtkWidget *widget, gpointer data);
void
update_theme_entries(GtkWidget *disp_list);
void
signal_apply_theme(GtkWidget *widget);
void
click_preview(GtkWidget *widget, gpointer data);
void
click_try(GtkWidget *widget, gpointer data);
void
click_help(GtkWidget *widget, gpointer data);
void
click_ok(GtkWidget *widget, gpointer data);
void
click_revert(GtkWidget *widget, gpointer data);
void
edit_file_to_use(gchar *file, gchar *theme);
void 
set_tmp_rc(void);
void
use_theme(gchar *theme);
void
test_theme(gchar *theme);
void
click_update(GtkWidget *widget, gpointer data);
gchar *
install_theme(gchar *file);
void
do_demo(int argc, char **argv);
void
send_socket(void);
void
send_reread(void);


extern GtkWidget *preview_socket;
extern gint       prog_fd;
extern gchar      gtkrc_tmp[1024];
