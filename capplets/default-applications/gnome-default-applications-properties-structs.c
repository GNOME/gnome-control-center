struct _BrowserDescription
{
	gchar *name;	
        gchar *executable_name;
	gchar *command;
        gboolean needs_term;
        gboolean nremote;
	gboolean in_path;
};

struct _MailerDescription
{
	gchar *name;	
        gchar *executable_name;
	gchar *command;
        gboolean needs_term;
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
        { "Epiphany", 			"epiphany",    "epiphany %s",    FALSE, FALSE, FALSE },
        { "Galeon", 			"galeon",    "galeon %s",    FALSE, FALSE, FALSE },
        { "Encompass", 			"encompass", "encompass %s", FALSE, FALSE, FALSE },
        { "Mozilla/Netscape 6", 	"mozilla",   "mozilla %s",   FALSE, TRUE,  FALSE },
        { "Netscape Communicator", 	"netscape",  "netscape %s",  FALSE, TRUE,  FALSE },
        { "Konqueror", 			"konqueror", "konqueror %s", FALSE, FALSE, FALSE },
        { "Lynx Text Browser",		"lynx",      "lynx %s",      TRUE,  FALSE, FALSE },
        { "Links Text Browser" , 	"links",     "links %s",     TRUE,  FALSE, FALSE }
};

MailerDescription possible_mailers[] =
{
	/* The code in gnome-default-applications-properties.c makes sure
	 * there is only one (the first entry in this list) Evolution entry 
	 * in the list shown to the user
	 */
        { "Evolution Mail Reader",		"evolution-1.4",      "evolution-1.4 %s",      FALSE,  FALSE, },
        { "Evolution Mail Reader",		"evolution",      "evolution %s",      FALSE,  FALSE, },
	{ "Balsa",        "balsa",    "balsa --compose=%s", FALSE, FALSE },
	{ "KMail",        "kmail",    "kmail %s", FALSE, FALSE },
	{ "Mozilla Mail", "mozilla",  "mozilla -mail %s",   FALSE, FALSE},
        { "Mutt" , 	  "mutt",     "mutt %s",            TRUE, FALSE },

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
