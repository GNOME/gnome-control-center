struct _BrowserDescription
{
	gchar *name;	
        gchar *executable_name;
	gchar *command;
        gboolean needs_term;
        gboolean nremote;
	gboolean in_path;
};

struct _HelpViewDescription
{
	gchar *name;
        gchar *executable_name;
        gboolean needs_term;
        gboolean accepts_urls;
	gboolean in_path;
};

struct _TerminalDesciption
{
	gchar *name;
        gchar *exec;
        gchar *exec_arg;
	gboolean in_path;
};

BrowserDescription possible_browsers[] =
{
        { "Lynx Text Browser",		"lynx",      "lynx %s",      TRUE,  FALSE, FALSE },
        { "Links Text Browser" , 	"links",     "links %s",     TRUE,  FALSE, FALSE },
        { "Netscape Communicator", 	"netscape",  "netscape %s",  FALSE, TRUE,  FALSE },
        { "Mozilla/Netscape 6", 	"mozilla",   "mozilla %s",   FALSE, TRUE,  FALSE },
        { "Galeon", 			"galeon",    "galeon %s",    FALSE, FALSE, FALSE },
        { "Encompass", 			"encompass", "encompass %s", FALSE, FALSE, FALSE },
        { "Konqueror", 			"konqueror", "konqueror %s", FALSE, FALSE, FALSE }
};

HelpViewDescription possible_help_viewers[] = 
{ 
        { "Yelp Gnome Help Browser", "yelp",  FALSE, TRUE, FALSE },
        { "Gnome Help Browser", "gnome-help", FALSE, TRUE, FALSE },
        { "Nautilus", "nautilus",             FALSE, TRUE, FALSE }
};

TerminalDescription possible_terminals[] = 
{ 
        { "Gnome Terminal", "gnome-terminal", "-x", FALSE },
        { "Standard XTerminal", "xterm", "-e", FALSE },
        { "NXterm", "nxterm", "-e", FALSE },
        { "RXVT", "rxvt", "-e", FALSE },
        { "ETerm", "Eterm", "-e", FALSE }
};
