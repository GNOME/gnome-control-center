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
        { N_("Epiphany"), 			"epiphany",    "epiphany %s",    FALSE, FALSE, FALSE },
        { N_("Galeon"), 			"galeon",    "galeon %s",    FALSE, FALSE, FALSE },
        { N_("Encompass"), 			"encompass", "encompass %s", FALSE, FALSE, FALSE },
        { N_("Mozilla/Netscape 6"), 	"mozilla",   "mozilla %s",   FALSE, TRUE,  FALSE },
        { N_("Netscape Communicator"), 	"netscape",  "netscape %s",  FALSE, TRUE,  FALSE },
        { N_("Konqueror"), 			"konqueror", "konqueror %s", FALSE, FALSE, FALSE },
        { N_("Lynx Text Browser"),		"lynx",      "lynx %s",      TRUE,  FALSE, FALSE },
        { N_("Links Text Browser") , 	"links",     "links %s",     TRUE,  FALSE, FALSE }
};

MailerDescription possible_mailers[] =
{
	/* The code in gnome-default-applications-properties.c makes sure
	 * there is only one (the first entry in this list) Evolution entry 
	 * in the list shown to the user
	 */
        { N_("Evolution Mail Reader"),		"evolution-1.4",      "evolution-1.4 %s",      FALSE,  FALSE, },
        { N_("Evolution Mail Reader"),		"evolution",      "evolution %s",      FALSE,  FALSE, },
	{ N_("Balsa"),        "balsa",    "balsa --compose=%s", FALSE, FALSE },
	{ N_("KMail"),        "kmail",    "kmail %s", FALSE, FALSE },
	{ N_("Mozilla Mail"), "mozilla",  "mozilla -mail %s",   FALSE, FALSE},
        { N_("Mutt") , 	  "mutt",     "mutt %s",            TRUE, FALSE },

};

TerminalDescription possible_terminals[] = 
{ 
        { N_("Gnome Terminal"), "gnome-terminal", "-x", FALSE },
        { N_("Standard XTerminal"), "xterm", "-e", FALSE },
        { N_("NXterm"), "nxterm", "-e", FALSE },
        { N_("RXVT"), "rxvt", "-e", FALSE },
        { N_("ETerm"), "Eterm", "-e", FALSE }
};
