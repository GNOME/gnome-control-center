#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome-settings-daemon/gnome-settings-client.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "activate-settings-daemon.h"

static void popup_error_message (void)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
				   GTK_BUTTONS_OK, _("Unable to start the settings manager 'gnome-settings-daemon'.\n"
				   "Without the GNOME settings manager running, some preferences may not take effect. This could "
				   "indicate a problem with Bonobo, or a non-GNOME (e.g. KDE) settings manager may already "
				   "be active and conflicting with the GNOME settings manager."));

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* Returns FALSE if activation failed, else TRUE */
gboolean 
activate_settings_daemon (void)
{
  DBusGConnection *connection = NULL;
  DBusGProxy *proxy = NULL;
  GError *error = NULL;

  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (connection == NULL)
    {
      popup_error_message ();
      g_error_free (error);
      return FALSE;
    }
    
  proxy = dbus_g_proxy_new_for_name (connection,
                                     "org.gnome.SettingsDaemon",
                                     "/org/gnome/SettingsDaemon",
                                     "org.gnome.SettingsDaemon");

  if (proxy == NULL)
    {
      popup_error_message ();
      return FALSE;
    }

  if (!org_gnome_SettingsDaemon_awake(proxy, &error))
    {
      popup_error_message ();
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}
