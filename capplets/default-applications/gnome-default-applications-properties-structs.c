
struct _EditorDescription
{
	gchar *name;
        gchar *executable_name;
        gboolean needs_term;
        gboolean accepts_lineno;
	gboolean in_path;
};

struct _BrowserDescription
{
	gchar *name;
        gchar *executable_name;
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

EditorDescription possible_editors[] =
{
        { "gedit",        	"gedit",  FALSE,  FALSE, FALSE },
        { "Emacs",        	"emacs",  FALSE,  TRUE, FALSE },
        { "Emacs (terminal)",	"emacs",  TRUE,   TRUE, FALSE },
        { "XEmacs",       	"xemacs", FALSE,  TRUE, FALSE },
        { "vi",           	"vi",     TRUE,   TRUE, FALSE },
        { "Go",           	"go",     FALSE,  FALSE, FALSE },
        { "GWP",          	"gwp",    FALSE,  FALSE, FALSE },
        { "Jed",          	"jed",    TRUE,   TRUE, FALSE },
        { "Joe",          	"joe",    TRUE,   TRUE, FALSE },
        { "Pico",         	"pico",   TRUE,   TRUE, FALSE },
        { "vim",          	"vim",    TRUE,   TRUE, FALSE },
        { "gvim",         	"gvim",   FALSE,  TRUE, FALSE },
        { "ed",           	"ed",     TRUE,   FALSE, FALSE },
        { "GMC/CoolEdit", 	"gmc -e", FALSE,  FALSE, FALSE },
	{ "Nedit",        	"nedit",  FALSE,  FALSE, FALSE }
};

BrowserDescription possible_browsers[] =
{
        { "Lynx Text Browser",		"lynx",      TRUE,  FALSE, FALSE },
        { "Links Text Browser" , 	"links",     TRUE,  FALSE, FALSE },
        { "Netscape Communicator", 	"netscape",  FALSE, TRUE,  FALSE },
        { "Mozilla/Netscape 6", 	"mozilla",   FALSE, TRUE,  FALSE },
        { "Galeon", 			"galeon",    FALSE, FALSE, FALSE },
        { "Encompass", 			"encompass", FALSE, FALSE, FALSE },
        { "Konqueror", 			"konqueror", FALSE, FALSE, FALSE }
};

HelpViewDescription possible_help_viewers[] = 
{ 
        { "Gnome Help Browser", "yelp", FALSE, TRUE, FALSE },
        { "Nautilus", "nautilus",       FALSE, TRUE, FALSE }
};

TerminalDescription possible_terminals[] = 
{ 
        { "Gnome Terminal", "gnome-terminal", "-x", FALSE },
        { "Standard XTerminal", "xterm", "-e", FALSE },
        { "NXterm", "nxterm", "-e", FALSE },
        { "RXVT", "rxvt", "-e", FALSE },
        { "ETerm", "Eterm", "-e", FALSE }
};
