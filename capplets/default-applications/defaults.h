/* -*- MODE: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

typedef struct _BrowserDescription BrowserDescription;
typedef struct _EditorDescription EditorDescription;
typedef struct _HelpViewDescription HelpViewDescription;
typedef struct _TerminalDesciption TerminalDescription;

struct _EditorDescription
{
        gchar *name;
        gchar *executable_name;
        gboolean needs_term;
        gchar *execution_type;
        gboolean accepts_lineno;
        gboolean use_name;
};

struct _BrowserDescription
{
        gchar *name;
        gchar *executable_name;
        gboolean needs_term;
        gboolean nremote;
        gboolean use_name;
};

struct _HelpViewDescription
{
        gchar *name;
        gchar *executable_name;
        gboolean needs_term;
        gboolean allows_urls;
        gboolean use_name;
};

struct _TerminalDesciption
{
        gchar *name;
        gchar *executable_name;
        gchar *exec_app;
        gboolean use_name;
};

