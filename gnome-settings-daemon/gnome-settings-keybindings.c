#include <config.h>

#include <string.h>
#include <X11/keysym.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnome/gnome-i18n.h>
#include "gnome-settings-daemon.h"
#include "gnome-settings-keybindings.h"

/* we exclude shift, GDK_CONTROL_MASK and GDK_MOD1_MASK since we know what 
   these modifiers mean 
   these are the mods whose combinations are bound by the keygrabbing code */
#define IGNORED_MODS (0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK  | \
        GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK) 
/* these are the ones we actually use for global keys, we always only check
 * for these set */
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

#define GCONF_BINDING_DIR "/desktop/gnome/keybindings"

typedef struct {
  guint keysym;
  guint state;
  guint keycode;
} Key;

typedef struct {
  char *binding_str;
  char *action;
  char *gconf_key;
  Key   key;
  Key   previous_key;
} Binding;
  
static GSList *binding_list = NULL;

static gint 
compare_bindings (gconstpointer a, gconstpointer b)
{
  Binding *key_a = (Binding*) a;
  char *key_b = (char*) b;

  return strcmp (key_b, key_a->gconf_key);
}

static gboolean
parse_binding (Binding *binding)
{
  g_return_val_if_fail (binding != NULL, FALSE);

  binding->key.keysym = 0;
  binding->key.state = 0;

  if (binding->binding_str == NULL ||
      binding->binding_str[0] == '\0' ||
      strcmp (binding->binding_str, "Disabled") == 0)
          return FALSE;

  gtk_accelerator_parse (binding->binding_str, &binding->key.keysym, &binding->key.state);

  if (binding->key.keysym == 0)
          return FALSE;

  binding->key.keycode = XKeysymToKeycode (GDK_DISPLAY (), binding->key.keysym);

  return TRUE;
}

gboolean 
bindings_get_entry (char *subdir)
{
  GConfValue *value;
  Binding *new_binding;
  GSList *tmp_elem = NULL, *list = NULL, *li;
  char *gconf_key;
  char *action = NULL;
  char *key = NULL;
  
  g_return_val_if_fail (subdir != NULL, FALSE);
  
  /* value = gconf_entry_get_value (entry); */
  gconf_key = g_path_get_basename (subdir);

  if (!gconf_key)
    return FALSE;

  /* Get entries for this binding */
  list = gconf_client_all_entries (gconf_client_get_default (), subdir, NULL);

  for (li = list; li != NULL; li = li->next)
    {
      GConfEntry *entry = li->data;
      char *key_name = g_path_get_basename (gconf_entry_get_key (entry));
      if (strcmp (key_name, "action") == 0)
	{
	  if (!action)
	    {
	      value = gconf_entry_get_value (entry);
	      if (value->type != GCONF_VALUE_STRING)
		return FALSE;
	      action = g_strdup (gconf_value_get_string (value));
	    }
	  else
	    g_warning (_("Key Binding (%s) has its action defined multiple times\n"),
		       gconf_key);
	}
      if (strcmp (key_name, "binding") == 0)
	{
	  if (!key)
	    {
	      value = gconf_entry_get_value (entry);
	      if (value->type != GCONF_VALUE_STRING)
		return FALSE;
	      key = g_strdup (gconf_value_get_string (value));
	    }
	  else
	    g_warning (_("Key Binding (%s) has its binding defined multiple times\n"),
		       gconf_key);
	}
    }
  if (!action || !key)
    { 
      g_warning (_("Key Binding (%s) is incomplete\n"), gconf_key);
      return FALSE;
    }
    
  tmp_elem = g_slist_find_custom (binding_list, gconf_key,
				  compare_bindings);

  if (!tmp_elem)
    new_binding = g_new0 (Binding, 1);
  else
    {
      new_binding = (Binding*) tmp_elem->data;
      g_free (new_binding->binding_str);
      g_free (new_binding->action);
    }
  
  new_binding->binding_str = key;
  new_binding->action = action;
  new_binding->gconf_key = gconf_key;

  new_binding->previous_key.keysym = new_binding->key.keysym;
  new_binding->previous_key.state = new_binding->key.state;
  new_binding->previous_key.keycode = new_binding->key.keycode;

  if (parse_binding (new_binding))
      binding_list = g_slist_append (binding_list, new_binding);
  else
    {
      g_warning (_("Key Binding (%s) is invalid\n"), gconf_key);
      g_free (new_binding->binding_str);
      g_free (new_binding->action);
      return FALSE;
    }
  return TRUE;
}

static gboolean 
key_already_used (Binding *binding)
{
  GSList *li;
  
  for (li = binding_list; li != NULL; li = li->next)
    {
      Binding *tmp_binding =  (Binding*) li->data;

      if (tmp_binding != binding &&  tmp_binding->key.keycode == binding->key.keycode &&
	  tmp_binding->key.state == binding->key.state)
	return TRUE;
    }
  return FALSE;
}

/* inspired from all_combinations from gnome-panel/gnome-panel/global-keys.c */
#define N_BITS 32
static void
do_grab (gboolean grab,
	 Key *key)
{
	int indexes[N_BITS];/*indexes of bits we need to flip*/
	int i, bit, bits_set_cnt;
	int uppervalue;
	guint mask_to_traverse = IGNORED_MODS & ~ key->state;
	  
	bit = 0;
	for (i = 0; i < N_BITS; i++) {
		if (mask_to_traverse & (1<<i))
			indexes[bit++]=i;
	}

	bits_set_cnt = bit;

	uppervalue = 1<<bits_set_cnt;
	for (i = 0; i < uppervalue; i++) {
		int j, result = 0;

		for (j = 0; j < bits_set_cnt; j++) {
			if (i & (1<<j))
				result |= (1<<indexes[j]);
		}
		/* FIXME need to grab for all root windows for the display */
		if (grab) 
		  XGrabKey (GDK_DISPLAY(), key->keycode, (result | key->state),
			    GDK_ROOT_WINDOW(), True, GrabModeAsync, GrabModeAsync);
		else 
		  XUngrabKey(GDK_DISPLAY(), key->keycode, (result | key->state),
			     GDK_ROOT_WINDOW());
	}
}

void
binding_register_keys ()
{
  GSList *li;
  
  gdk_error_trap_push();

  /* Now check for changes and grab new key if not already used */
  for (li = binding_list ; li != NULL; li = li->next)
    {
      Binding *binding = (Binding *) li->data;
      
      if (binding->previous_key.keycode != binding->key.keycode || 
	  binding->previous_key.state != binding->key.state)
        {
          /* Ungrab key if it changed and not clashing with previously set binding */
          if (!key_already_used (binding))
            {
              if (binding->previous_key.keycode)
		do_grab (FALSE, &binding->previous_key);
	      do_grab (TRUE, &binding->key);

	      binding->previous_key.keysym = binding->key.keysym;
	      binding->previous_key.state = binding->key.state;
	      binding->previous_key.keycode = binding->key.keycode;
            }
          else
            g_warning (_("Key Binding (%s) is already in use\n"), binding->binding_str);
        }
    }
  gdk_flush ();
  gdk_error_trap_pop();
}

static void
bindings_callback (GConfEntry *entry)
{
  /* ensure we get binding dir not a sub component */
  gchar** key_elems = g_strsplit (gconf_entry_get_key (entry), "/", 15);
  gchar* binding_entry  = g_strdup_printf ("/%s/%s/%s/%s", key_elems[1], 
					   key_elems[2], key_elems[3], 
					   key_elems[4]);					 
  g_strfreev (key_elems);
  
  bindings_get_entry (binding_entry);
  g_free (binding_entry);
  
  binding_register_keys ();
}

GdkFilterReturn
keybindings_filter (GdkXEvent *gdk_xevent,
		    GdkEvent *event,
		    gpointer data)
{
  XEvent *xevent = (XEvent *)gdk_xevent;
  guint keycode, state;
  GSList *li;

  if(xevent->type != KeyPress)
          return GDK_FILTER_CONTINUE;
        
  keycode = xevent->xkey.keycode;
  state = xevent->xkey.state;
  
  for (li = binding_list; li != NULL; li = li->next)
    {
      Binding *binding = (Binding*) li->data;
      
      if (keycode == binding->key.keycode &&
          (state & USED_MODS) == binding->key.state)
        {
          GError* error = NULL;

          if (!g_spawn_command_line_async (binding->action, &error))
	    {
	      GtkWidget *dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING,
							  GTK_BUTTONS_CLOSE,
							  _("Error while trying to run (%s)\n"\
							  "which is linked to the key (%s)"),
							  binding->action,
							  binding->binding_str);
	      	g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_widget_show (dialog);
	    }
          return GDK_FILTER_REMOVE;
        }
    }
  return GDK_FILTER_CONTINUE;
}

void
gnome_settings_keybindings_init (GConfClient *client)
{
  gnome_settings_daemon_register_callback (GCONF_BINDING_DIR, bindings_callback);
  gdk_window_add_filter (gdk_get_default_root_window (),
			 keybindings_filter,
			 NULL);

}

void
gnome_settings_keybindings_load (GConfClient *client)
{
  GSList *list, *li;

  list = gconf_client_all_dirs (client, GCONF_BINDING_DIR, NULL);

  for (li = list; li != NULL; li = li->next)
    {
      char *subdir = li->data;
      li->data = NULL;
      bindings_get_entry (subdir);
    }
  binding_register_keys ();
}

