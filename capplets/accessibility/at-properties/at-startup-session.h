#include <config.h>
#include <gtk/gtk.h>

typedef union {
	guint flags;
	struct {
		guint support:1;
		guint osk:1;
		guint magnifier:1;
		guint screenreader:1;
	} enabled;
} AtStartupState;

void at_startup_state_init (AtStartupState *startup_state);

void at_startup_state_update (AtStartupState *startup_state);
