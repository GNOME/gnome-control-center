#include <stdio.h>

#include "gnome-settings-daemon.h"
#include "gnome-settings-xsettings.h"
#include "xsettings-manager.h"

extern XSettingsManager *manager;


typedef struct _TranslationEntry TranslationEntry;
typedef void (* TranslationFunc) (TranslationEntry *trans,
                                  GConfValue       *value);
struct _TranslationEntry
{
  const char *gconf_key;
  const char *xsetting_name;
  
  GConfValueType gconf_type;
  TranslationFunc translate;
};


static void
translate_int_int (TranslationEntry *trans,
                   GConfValue       *value)
{
  g_assert (value->type == trans->gconf_type);
  
  g_print ("setting %s %d\n", 
	   trans->xsetting_name,
	   gconf_value_get_int (value));
  xsettings_manager_set_int (manager, trans->xsetting_name,
                             gconf_value_get_int (value));
}

static void
translate_string_string (TranslationEntry *trans,
                         GConfValue       *value)
{
  g_assert (value->type == trans->gconf_type);

  g_print ("setting %s %s\n", 
	   trans->xsetting_name,
	   gconf_value_get_string (value));
  xsettings_manager_set_string (manager,
                                trans->xsetting_name,
                                gconf_value_get_string (value));
}

static TranslationEntry translations [] = {
  { "/desktop/gnome/peripherals/mouse/double_click", "Net/DoubleClickTime", GCONF_VALUE_INT,    
    translate_int_int },
  { "/desktop/gnome/gtk-color-palette", "Gtk/ColorPalette", GCONF_VALUE_STRING,
    translate_string_string },
  { "/desktop/gnome/gtk-toolbar-style", "Gtk/ToolbarStyle", GCONF_VALUE_STRING,
    translate_string_string },
  { "/desktop/gnome/gtk-toolbar-icon-size", "Gtk/ToolbarIconSize", GCONF_VALUE_STRING,
    translate_string_string },
  { "/desktop/gnome/interface/gtk_theme", "Net/ThemeName", GCONF_VALUE_STRING,
    translate_string_string },
  { "/desktop/gnome/interface/font_name", "Gtk/FontName", GCONF_VALUE_STRING,
    translate_string_string }
};

static TranslationEntry*
find_translation_entry (const char *gconf_key)
{
  int i;

  i = 0;
  while (i < G_N_ELEMENTS (translations))
    {
      if (strcmp (translations[i].gconf_key, gconf_key) == 0)
        return &translations[i];

      ++i;
    }

  return NULL;
}

static const gchar* 
type_to_string (GConfValueType type)
{
  switch (type)
    {
    case GCONF_VALUE_INT:
      return "int";
      break;
    case GCONF_VALUE_STRING:
      return "string";
      break;
    case GCONF_VALUE_FLOAT:
      return "float";
      break;
    case GCONF_VALUE_BOOL:
      return "bool";
      break;
    case GCONF_VALUE_SCHEMA:
      return "schema";
      break;
    case GCONF_VALUE_LIST:
      return "list";
      break;
    case GCONF_VALUE_PAIR:
      return "pair";
      break;
    case GCONF_VALUE_INVALID:
      return "*invalid*";
      break;
    default:
      g_assert_not_reached();
      return NULL; /* for warnings */
      break;
    }
}

static void
process_value (TranslationEntry *trans,
               GConfValue       *val)
{  
  if (val == NULL)
    {
      xsettings_manager_delete_setting (manager, trans->xsetting_name);
    }
  else
    {
      if (val->type == trans->gconf_type)
        {
          (* trans->translate) (trans, val);
        }
      else
        {
          g_warning ("GConf key %s set to type %s but its expected type was %s\n",
                     trans->gconf_key,
                     type_to_string (val->type),
                     type_to_string (trans->gconf_type));
        }
    }
}

static void
xsettings_callback (GConfEntry *entry)
{
  TranslationEntry *trans;
  trans = find_translation_entry (entry->key);

  if (trans == NULL)
    return;

  process_value (trans, entry->value);
  
  xsettings_manager_notify (manager);
}

void
gnome_settings_xsettings_init (GConfClient *client)
{
  gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/mouse", xsettings_callback);
  gnome_settings_daemon_register_callback ("/desktop/gtk", xsettings_callback);
  gnome_settings_daemon_register_callback ("/desktop/gnome/interface", xsettings_callback);
}

void
gnome_settings_xsettings_load (GConfClient *client)
{
  int i;

  i = 0;
  while (i < G_N_ELEMENTS (translations))
    {
      GConfValue *val;
      GError *err;

      err = NULL;
      val = gconf_client_get (client,
                              translations[i].gconf_key,
                              &err);

      if (err != NULL)
        {
          fprintf (stderr, "Error getting value for %s: %s\n",
                   translations[i].gconf_key, err->message);
          g_error_free (err);
        }
      else
        {
          process_value (&translations[i], val);
        }
      
      ++i;
    }

  xsettings_manager_notify (manager);
}
