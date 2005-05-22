#include <glib.h>

typedef union {
	guint flags;
	struct {
		guint support:1;
		guint osk:1;
		guint magnifier:1;
		guint screenreader:1;
		guint osk_installed:1;
		guint magnifier_installed:1;
		guint screenreader_installed:1;
	} enabled;
} AtStartupState;

void at_startup_state_init (AtStartupState *startup_state);

void at_startup_state_update (AtStartupState *startup_state);
