#include "da.h"

void
install_theme(gchar *file)
{
  gchar                s[4096];
  gchar                th[4096];
  FILE                *f;
  gint                 i;
  guchar               buf[1024];
  gchar               *theme_dir;
  gchar               *home;
  
  if (isdir(file))
    {
      return;
    }
  theme_dir = gtk_rc_get_theme_dir();
  if (geteuid() == 0)
    g_snprintf(th, sizeof(th), "%s/", theme_dir);
  else
    {
      home = getenv("HOME");
      if (!home)
	{
	  g_free(theme_dir);
	  return;
	}
      g_snprintf(th, sizeof(th), "%s/.gtk/themes/", home);
    }
  g_free(theme_dir);

  if (!isdir(th))
    md(th);
  
  if (isfile(file))
    {
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
			 "(cd %s ; tar -xf %s", 
			 file, th);
	    } else
	      s[0] = '\0';

	  if(*s) {
	    system(s);
	    wait(&i);
	  }
	  return;
	}
    }
  return;
}
