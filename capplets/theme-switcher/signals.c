#include "da.h"

void 
signal_apply_theme(GtkWidget *widget)
{
  GdkEventClient rcevent;
  
  rcevent.type = GDK_CLIENT_EVENT;
  rcevent.window = widget->window;
  rcevent.send_event = TRUE;
  rcevent.message_type = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
  rcevent.data_format = 8;
  gdk_event_send_clientmessage_toall((GdkEvent *)&rcevent);
}
