#include <gnome.h>
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
  int row;
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
GList*
list_themes(gchar *dir);
GList*
list_system_themes(void);
GList*
list_user_themes(void);
GtkWidget *
make_main(void);
void
update_theme_entries(GtkWidget *disp_list);
void
signal_apply_theme(GtkWidget *widget);
void
edit_file_to_use(gchar *file, gchar *theme, gchar *font);
void 
set_tmp_rc(void);
void
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


extern GtkWidget *preview_socket;
extern gint       prog_fd;
extern gchar      gtkrc_tmp[1024];
