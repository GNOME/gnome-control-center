#include "da.h"
#include <errno.h>

gchar *
install_theme(gchar *file)
{
  gchar                s[4096];
  gchar                th[4096];
  FILE                *f;
  guchar               buf[1024];
  gchar               *theme_dir;
  gchar               *home;
  
  if (isdir(file))
    return FALSE;

  theme_dir = gtk_rc_get_theme_dir();
  if (geteuid() == 0)
    g_snprintf(th, sizeof(th), "%s/", theme_dir);
  else
    {
      home = g_get_home_dir();
      if (!home)
	{
	  g_free(theme_dir);
	  return g_strdup(_("Home directory doesn't exist!\n"));
	}
      g_snprintf(th, sizeof(th), "%s/.themes/", home);
    }
  g_free(theme_dir);

  if (!isdir(th))
    md(th);
  
  if (!isfile(file))
    return g_strdup(_("Theme does not exist"));
    
  f = fopen(file, "r");
  if (f)
    {
      fread(buf, 1, 1000, f);
      fclose(f);
      if ((buf[0] == 31) && (buf[1] == 139))
	{
	  /*gzipped tarball */
	  /*sprintf(s,"gzip -d -c < %s | tar -xf - -C %s",Theme_Tar_Ball,Theme_Path); */
	  g_snprintf(s, sizeof(s),
		     "gzip -d -c < %s | (cd %s ; tar -xf -)", 
		     file, th);
	}
      else if ((buf[257] == 'u') && (buf[258] == 's') && (buf[259] == 't') &&
	       (buf[260] == 'a') && (buf[261] == 'r'))
	{
	  /*vanilla tarball */
	  /*sprintf(s,"tar -xf - -C %s < %s",Theme_Path,Theme_Tar_Ball); */
	  g_snprintf(s, sizeof(s), 
		     "(cd %s && tar -xf %s", 
		     th, file);
	} else
	  s[0] = '\0';
      
      if (*s)
	{
	  gint status = system(s);
	  if (status < 0)
	    return g_strdup(g_strerror (errno));
	  else if (status != 0)
	    return g_strdup_printf(_("Command '%s' failed"), s);
	  else
	    return NULL;
	}
      else
	return g_strdup(_("Unknown file format"));
    }

  return FALSE;
}
