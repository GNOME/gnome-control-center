struct _BrowserDescription {
	gchar const *name;	
        gchar const *executable_name;
	gchar const *command;
        gboolean needs_term;
	gboolean in_path;
};
BrowserDescription possible_browsers[] = {
        { N_("Epiphany"), 		"epiphany",	"epiphany %s",		FALSE, FALSE },
        { N_("Galeon"), 		"galeon",	"galeon %s",		FALSE, FALSE },
        { N_("Encompass"), 		"encompass",	"encompass %s",		FALSE, FALSE },
        { N_("Firebird/FireFox"),  "mozilla-firebird",	"mozilla-firebird %s",	FALSE, FALSE },
        { N_("Mozilla/Netscape 6"), 	"mozilla-1.6",	"mozilla-1.6 %s",	FALSE, FALSE },
        { N_("Mozilla/Netscape 6"), 	"mozilla",	"mozilla %s",		FALSE, FALSE },
        { N_("Netscape Communicator"), 	"netscape",	"netscape %s",		FALSE, FALSE },
        { N_("Konqueror"), 		"konqueror",	"konqueror %s",		FALSE, FALSE },
        { N_("W3M Text Browser"),	"w3n",		"w3m %s",		TRUE,  FALSE },
        { N_("Lynx Text Browser"),	"lynx",		"lynx %s",		TRUE,  FALSE },
        { N_("Links Text Browser") , 	"links",	"links %s",		TRUE,  FALSE }
};

struct _MailerDescription {
	gchar const *name;	
        gchar const *executable_name;
	gchar const *command;
        gboolean needs_term;
	gboolean in_path;
};
MailerDescription possible_mailers[] = {
	/* The code in gnome-default-applications-properties.c makes sure
	 * there is only one (the first entry in this list) Evolution entry 
	 * in the list shown to the user
	 */
	{ N_("Evolution Mail Reader"),		"evolution-1.6",	"evolution-1.6 %s",	FALSE, FALSE, },
	{ N_("Evolution Mail Reader"),		"evolution-1.5",	"evolution-1.5 %s",	FALSE, FALSE, },
        { N_("Evolution Mail Reader"),		"evolution-1.4",	"evolution-1.4 %s",	FALSE, FALSE, },
        { N_("Evolution Mail Reader"),		"evolution",		"evolution %s",		FALSE, FALSE, },
	{ N_("Balsa"),        			"balsa",		"balsa -m %s",		FALSE, FALSE },
	{ N_("KMail"),        			"kmail",		"kmail %s",		FALSE, FALSE },
	{ N_("Mozilla Mail"), 			"mozilla",		"mozilla -mail %s",	FALSE, FALSE},
        { N_("Mutt") , 	  			"mutt",			"mutt %s",		TRUE,  FALSE },

};

struct _TerminalDesciption {
	gchar const *name;
        gchar const *exec;
        gchar const *exec_arg;
	gboolean in_path;
};
TerminalDescription possible_terminals[] = { 
        { N_("Gnome Terminal"),		"gnome-terminal",	"-x", FALSE },
        { N_("Standard XTerminal"),	"xterm",		"-e", FALSE },
        { N_("NXterm"),			"nxterm",		"-e", FALSE },
        { N_("RXVT"),			"rxvt",			"-e", FALSE },
        { N_("ETerm"),			"Eterm",		"-e", FALSE }
};
