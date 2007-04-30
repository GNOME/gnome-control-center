#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>

#define WID(x) (glade_xml_get_widget (data->xml, x))

typedef struct {
  int argc;
  char **argv;
  GladeXML *xml;
} AppearanceData;
