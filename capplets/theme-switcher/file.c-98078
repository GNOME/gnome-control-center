#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "da.h"

void 
md(char *s)
{
  if ((!s) || (!*s))
    return;
  mkdir(s, S_IRWXU);
}

int 
exists(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (stat(s, &st) < 0)
    return 0;
  return 1;
}

int 
isfile(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (stat(s, &st) < 0)
    return 0;
  if (st.st_blocks == 0)
    return 0;
  if (S_ISREG(st.st_mode))
    return 1;
  return 0;
}

int 
isdir(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (stat(s, &st) < 0)
    return 0;
  if (S_ISDIR(st.st_mode))
    return 1;
  return 0;
}

int 
ls_compare_func(const void *a, const void *b)
{
  return strcmp(*(char **)a, *(char **)b);
}

char              **
ls(char *dir, int *num)
{
  int                 i, dirlen;
  DIR                *dirp;
  char              **names;
  struct dirent      *dp;

  if ((!dir) || (!*dir))
    return 0;
  dirp = opendir(dir);
  if (!dirp)
    {
      *num = 0;
      return NULL;
    }

  /* count # of entries in dir (worst case) */
  for (dirlen = 0; (dp = readdir(dirp)) != NULL; dirlen++);
  if (!dirlen)
    {
      closedir(dirp);
      *num = dirlen;
      return NULL;
    }

  /* load up the entries, now that we know how many to make */
  names = (char **)malloc(dirlen * sizeof(char *));

  if (!names)
    return NULL;

  rewinddir(dirp);
  for (i = 0; i < dirlen;)
    {
      dp = readdir(dirp);
      if (!dp)
	break;
      names[i] = (char *)malloc(strlen(dp->d_name) + 1);
      if (!names)
	return NULL;
      strcpy(names[i], dp->d_name);
      i++;
    }

  if (i < dirlen)
    dirlen = i;			/* dir got shorter... */
  closedir(dirp);
  *num = dirlen;
  qsort(names, dirlen, sizeof(char *), ls_compare_func);

  return names;
}

void 
freestrlist(char **l, int num)
{
  if (!l)
    return;
  while (num--)
    if (l[num])
      free(l[num]);
  free(l);
}

void 
rm(char *s)
{
  if ((!s) || (!*s))
    return;
  unlink(s);
}

void 
mv(char *s, char *ss)
{
  if ((!s) || (!ss) || (!*s) || (!*ss))
    return;
  rename(s, ss);
}

void 
cp(char *s, char *ss)
{
  int                 i;
  FILE               *f, *ff;
  unsigned char       buf[1];

  if ((!s) || (!ss) || (!*s) || (!*ss))
    return;
  if (!exists(s))
    return;
  i = filesize(s);
  f = fopen(s, "r");
  if (!f)
    return;
  ff = fopen(ss, "w");
  if (!ff)
    {
      fclose(f);
      return;
    }
  while (fread(buf, 1, 1, f))
    fwrite(buf, 1, 1, ff);
  fclose(f);
  fclose(ff);
}

unsigned long 
moddate(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (!stat(s, &st) < 0)
    return 0;
  if (st.st_mtime > st.st_ctime)
    return st.st_mtime;
  else
    return st.st_ctime;
}

int 
filesize(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (stat(s, &st) < 0)
    return 0;
  return (int)st.st_size;
}

void 
cd(char *s)
{
  if ((!s) || (!*s))
    return;
  chdir(s);
}

char               *
cwd(void)
{
  char                s[4096];

  getcwd(s, sizeof(s));
  return strdup(s);
}

int 
permissions(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (!stat(s, &st) < 0)
    return 0;
  return st.st_mode;
}

int 
owner(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (!stat(s, &st) < 0)
    return 0;
  return st.st_uid;
}

int 
group(char *s)
{
  struct stat         st;

  if ((!s) || (!*s))
    return 0;
  if (!stat(s, &st) < 0)
    return 0;
  return st.st_gid;
}

char               *
username(int uid)
{
  struct passwd      *pass;
  char               *result;

  pass = getpwuid(uid);
  if (pass && pass->pw_name)
    result = strdup(pass->pw_name);
  else
    result = NULL;
  endpwent();
  return result;
}

char               *
homedir(int uid)
{
  struct passwd      *pass;
  char               *result;

  result = getenv("HOME");
  if (result)
    return strdup(result);

  pass = getpwuid(uid);
  if (pass && pass->pw_dir)
    result = strdup(pass->pw_dir);
  else
    result = NULL;
  endpwent();
  return result;
}

char               *
usershell(int uid)
{
  struct passwd      *pass;
  char               *result;

  pass = getpwuid(uid);
  if (pass && pass->pw_shell)
    result = strdup(pass->pw_shell);
  else
    result = NULL;
  endpwent();
  return result;
}

char               *
atword(char *s, int num)
{
  int                 cnt, i;

  if (!s)
    return NULL;
  cnt = 0;
  i = 0;

  while (s[i])
    {
      if ((s[i] != ' ') && (s[i] != '\t'))
	{
	  if (i == 0)
	    cnt++;
	  else if ((s[i - 1] == ' ') || (s[i - 1] == '\t'))
	    cnt++;
	  if (cnt == num)
	    return &s[i];
	}
      i++;
    }
  return NULL;
}

char               *
atchar(char *s, char c)
{
  int                 i;

  if (!s)
    return NULL;
  i = 0;
  while (s[i] != 0)
    {
      if (s[i] == c)
	return &s[i];
      i++;
    }
  return NULL;
}

void 
word(char *s, int num, char *wd)
{
  int                 cnt, i;
  char               *start, *finish, *ss, *w;

  if (!s)
    return;
  if (!wd)
    return;
  if (num <= 0)
    {
      *wd = 0;
      return;
    }
  cnt = 0;
  i = 0;
  start = NULL;
  finish = NULL;
  ss = NULL;
  w = wd;

  while (s[i])
    {
      if ((cnt == num) && ((s[i] == ' ') || (s[i] == '\t')))
	{
	  finish = &s[i];
	  break;
	}
      if ((s[i] != ' ') && (s[i] != '\t'))
	{
	  if (i == 0)
	    {
	      cnt++;
	      if (cnt == num)
		start = &s[i];
	    }
	  else if ((s[i - 1] == ' ') || (s[i - 1] == '\t'))
	    {
	      cnt++;
	      if (cnt == num)
		start = &s[i];
	    }
	}
      i++;
    }
  if (cnt == num)
    {
      if ((start) && (finish))
	{
	  for (ss = start; ss < finish; ss++)
	    *wd++ = *ss;
	}
      else if (start)
	{
	  for (ss = start; *ss != 0; ss++)
	    *wd++ = *ss;
	}
      *wd = 0;
    }
  return;
}

int 
canread(char *s)
{
  if ((!s) || (!*s))
    return 0;
  return access(s, R_OK);
}

int 
canwrite(char *s)
{
  if ((!s) || (!*s))
    return 0;
  return access(s, W_OK);
}

int 
canexec(char *s)
{
  if ((!s) || (!*s))
    return 0;
  return access(s, X_OK);
}

char               *
fileof(char *s)
{
  char                ss[1024];
  int                 i, p1, p2;

  i = 0;
  p1 = -1;
  p2 = -1;
  for (i = strlen(s) - 1; i >= 0; i--)
    {
      if ((s[i] == '.') && (p2 < 0) && (p1 < 0))
	p2 = i;
      if ((s[i] == '/') && (p1 < 0))
	p1 = i;
    }
  if (p2 < 0)
    p2 = strlen(s);
  if (p1 < 0)
    p1 = 0;
  for (i = 0; i < (p2 - p1 - 1); i++)
    ss[i] = s[p1 + 1 + i];
  ss[i] = 0;
  return strdup(ss);
}

char               *
fullfileof(char *s)
{
  char                ss[1024];
  int                 i, p1, p2;

  i = 0;
  p1 = -1;
  for (i = strlen(s) - 1; i >= 0; i--)
    {
      if ((s[i] == '/') && (p1 < 0))
	p1 = i;
    }
  p2 = strlen(s);
  for (i = 0; i < (p2 - p1 - 1); i++)
    ss[i] = s[p1 + 1 + i];
  ss[i] = 0;
  return strdup(ss);
}

char               *
noext(char *s)
{
  char                ss[1024];
  int                 i, p1, p2;

  i = 0;
  p1 = -1;
  for (i = strlen(s) - 1; i >= 0; i--)
    {
      if ((s[i] == '/') && (p1 < 0))
	break;
      else if (s[i] == '.')
	{
	  p1 = i;
	  break;
	}
    }
  if (p1 < 0)
    return strdup(s);
  p2 = strlen(s);
  for (i = 0; i < p1; i++)
    ss[i] = s[i];
  ss[i] = 0;
  return strdup(ss);
}

void 
mkdirs(char *s)
{
  char                ss[1024];
  int                 i, ii;

  i = 0;
  ii = 0;
  while (s[i])
    {
      ss[ii++] = s[i];
      ss[ii] = 0;
      if (s[i] == '/')
	{
	  if (!exists(ss))
	    md(ss);
	  else if (!isdir(ss))
	    return;
	}
      i++;
    }
}
