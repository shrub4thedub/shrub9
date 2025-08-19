/*
 * Acme integration for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#ifndef ACME_H
#define ACME_H

#include <X11/Xlib.h>

/* Acme window types */
typedef enum {
	ACME_UNKNOWN = 0,
	ACME_MAIN,      /* Main acme window */
	ACME_COLUMN,    /* Column window */
	ACME_BODY,      /* Text body window */
	ACME_TAG        /* Tag line window */
} AcmeWindowType;

/* Acme window detection patterns */
#define ACME_CLASS_NAME "acme"
#define ACME_INSTANCE_NAME "acme"

/* Acme window name patterns */
#define ACME_MAIN_PATTERN "acme"
#define ACME_COLUMN_PATTERN " Del Snarf"
#define ACME_TAG_PATTERN_END " Del Snarf | Look Edit"

/* Acme-specific client data */
struct AcmeData {
	AcmeWindowType type;
	Client *main_window;      /* Reference to main acme window */
	Client *parent_column;    /* For body/tag windows, their column */
	int column_id;           /* Column identifier */
	int no_borders;          /* Override border settings */
	int no_titlebars;        /* Override titlebar settings */
	int smart_focus;         /* Don't steal focus on updates */
};

/* Global acme state */
extern int acme_integration_enabled;
extern Client *acme_main_window;
extern int acme_column_counter;

/* Function prototypes */
int acme_init(void);
void acme_cleanup(void);
int acme_is_acme_window(Client *c);
AcmeWindowType acme_detect_window_type(Client *c);
void acme_setup_window(Client *c);
void acme_handle_new_window(Client *c);
void acme_handle_destroy_window(Client *c);
int acme_should_have_borders(Client *c);
int acme_should_have_titlebar(Client *c);
int acme_should_steal_focus(Client *c);
Client* acme_find_main_window(void);
int acme_get_preferred_workspace(Client *c);
void acme_update_window_relationships(Client *c);

/* Acme window detection helpers */
int acme_match_class(Client *c);
int acme_match_window_name(Client *c);
AcmeWindowType acme_parse_window_name(const char *name);

/* Acme-specific behavior overrides */
int acme_override_border_width(Client *c);
int acme_override_focus_behavior(Client *c);
int acme_override_workspace_placement(Client *c);

/* Debug helpers */
void acme_debug_window_info(Client *c);
const char* acme_window_type_string(AcmeWindowType type);

#endif /* ACME_H */