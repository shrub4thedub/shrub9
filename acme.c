/*
 * Acme integration for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"
#include "config.h"
#include "acme.h"

/* Global acme state */
int acme_integration_enabled = 0;
Client *acme_main_window = NULL;
int acme_column_counter = 0;

int
acme_init(void)
{
	if (!config.acme_integration) {
		acme_integration_enabled = 0;
		return 0;
	}
	
	acme_integration_enabled = 1;
	acme_main_window = NULL;
	acme_column_counter = 0;
	
	fprintf(stderr, "acme: integration enabled\n");
	return 1;
}

void
acme_cleanup(void)
{
	acme_integration_enabled = 0;
	acme_main_window = NULL;
	acme_column_counter = 0;
}

int
acme_match_class(Client *c)
{
	if (!c || !c->class)
		return 0;
	
	return (strcmp(c->class, ACME_CLASS_NAME) == 0);
}

int
acme_match_window_name(Client *c)
{
	if (!c || !c->name)
		return 0;
	
	/* Check for various acme window name patterns */
	if (strstr(c->name, ACME_COLUMN_PATTERN) != NULL)
		return 1;
	if (strstr(c->name, ACME_TAG_PATTERN_END) != NULL)
		return 1;
	if (strcmp(c->name, ACME_MAIN_PATTERN) == 0)
		return 1;
	
	return 0;
}

int
acme_is_acme_window(Client *c)
{
	if (!acme_integration_enabled || !c)
		return 0;
	
	return acme_match_class(c) || acme_match_window_name(c);
}

AcmeWindowType
acme_parse_window_name(const char *name)
{
	if (!name)
		return ACME_UNKNOWN;
	
	/* Main acme window */
	if (strcmp(name, ACME_MAIN_PATTERN) == 0)
		return ACME_MAIN;
	
	/* Column window - has " Del Snarf" but not the full tag pattern */
	if (strstr(name, ACME_COLUMN_PATTERN) != NULL && 
	    strstr(name, ACME_TAG_PATTERN_END) == NULL)
		return ACME_COLUMN;
	
	/* Tag window - ends with " Del Snarf | Look Edit" */
	if (strstr(name, ACME_TAG_PATTERN_END) != NULL)
		return ACME_TAG;
	
	/* Body window - acme class but doesn't match other patterns */
	return ACME_BODY;
}

AcmeWindowType
acme_detect_window_type(Client *c)
{
	if (!acme_is_acme_window(c))
		return ACME_UNKNOWN;
	
	/* First check by window name */
	if (c->name) {
		AcmeWindowType type = acme_parse_window_name(c->name);
		if (type != ACME_UNKNOWN)
			return type;
	}
	
	/* Fallback to class-based detection */
	if (acme_match_class(c))
		return ACME_BODY; /* Most acme windows are body windows */
	
	return ACME_UNKNOWN;
}

void
acme_setup_window(Client *c)
{
	AcmeData *acme_data;
	
	if (!acme_is_acme_window(c) || !c->acme_data)
		return;
	
	acme_data = c->acme_data;
	acme_data->type = acme_detect_window_type(c);
	
	/* Set acme-specific behaviors based on type */
	switch (acme_data->type) {
	case ACME_MAIN:
		acme_data->no_borders = config.acme_no_borders;
		acme_data->no_titlebars = config.acme_no_titlebars;
		acme_data->smart_focus = config.acme_smart_focus;
		acme_main_window = c;
		break;
		
	case ACME_COLUMN:
		acme_data->no_borders = config.acme_no_borders;
		acme_data->no_titlebars = config.acme_no_titlebars;
		acme_data->smart_focus = config.acme_smart_focus;
		acme_data->main_window = acme_main_window;
		acme_data->column_id = ++acme_column_counter;
		break;
		
	case ACME_TAG:
	case ACME_BODY:
		acme_data->no_borders = config.acme_no_borders;
		acme_data->no_titlebars = config.acme_no_titlebars;
		acme_data->smart_focus = config.acme_smart_focus;
		acme_data->main_window = acme_main_window;
		break;
		
	default:
		break;
	}
	
	if (debug)
		acme_debug_window_info(c);
}

void
acme_handle_new_window(Client *c)
{
	if (!acme_integration_enabled || !c)
		return;
	
	if (acme_is_acme_window(c)) {
		/* Allocate acme-specific data */
		if ((c->acme_data = malloc(sizeof(AcmeData))) != NULL) {
			memset(c->acme_data, 0, sizeof(AcmeData));
			acme_setup_window(c);
		}
	}
}

void
acme_handle_destroy_window(Client *c)
{
	if (!c || !c->acme_data)
		return;
	
	/* Clean up main window reference */
	if (c == acme_main_window)
		acme_main_window = NULL;
	
	/* Free acme-specific data */
	free(c->acme_data);
	c->acme_data = NULL;
}

int
acme_should_have_borders(Client *c)
{
	if (!acme_integration_enabled || !c || !c->acme_data)
		return 1; /* Default to having borders */
	
	return !c->acme_data->no_borders;
}

int
acme_should_have_titlebar(Client *c)
{
	if (!acme_integration_enabled || !c || !c->acme_data)
		return config.show_titlebars; /* Use default config */
	
	return !c->acme_data->no_titlebars && config.show_titlebars;
}

int
acme_should_steal_focus(Client *c)
{
	if (!acme_integration_enabled || !c || !c->acme_data)
		return 1; /* Default behavior */
	
	return !c->acme_data->smart_focus;
}

Client*
acme_find_main_window(void)
{
	Client *c;
	
	/* Check cached reference first */
	if (acme_main_window && acme_is_acme_window(acme_main_window))
		return acme_main_window;
	
	/* Search for main window */
	for (c = clients; c; c = c->next) {
		if (c->acme_data && c->acme_data->type == ACME_MAIN) {
			acme_main_window = c;
			return c;
		}
	}
	
	acme_main_window = NULL;
	return NULL;
}

int
acme_get_preferred_workspace(Client *c)
{
	Client *main;
	
	if (!acme_integration_enabled || !c || !c->acme_data)
		return -1; /* No preference */
	
	/* Try to place new acme windows in same workspace as main window */
	if ((main = acme_find_main_window()) != NULL)
		return main->workspace;
	
	return -1; /* No preference */
}

void
acme_update_window_relationships(Client *c)
{
	if (!acme_integration_enabled || !c || !c->acme_data)
		return;
	
	/* Update main window reference */
	if (c->acme_data->type == ACME_MAIN)
		acme_main_window = c;
	else
		c->acme_data->main_window = acme_find_main_window();
}

int
acme_override_border_width(Client *c)
{
	if (!acme_should_have_borders(c))
		return 0;
	
	return -1; /* Use default */
}

int
acme_override_focus_behavior(Client *c)
{
	return acme_should_steal_focus(c);
}

int
acme_override_workspace_placement(Client *c)
{
	return acme_get_preferred_workspace(c);
}

const char*
acme_window_type_string(AcmeWindowType type)
{
	switch (type) {
	case ACME_MAIN:   return "main";
	case ACME_COLUMN: return "column";
	case ACME_TAG:    return "tag";
	case ACME_BODY:   return "body";
	default:          return "unknown";
	}
}

void
acme_debug_window_info(Client *c)
{
	if (!c || !c->acme_data)
		return;
	
	fprintf(stderr, "acme: window %p (%s) type=%s borders=%s titlebars=%s focus=%s\n",
	        (void*)c, c->name ? c->name : "unnamed",
	        acme_window_type_string(c->acme_data->type),
	        c->acme_data->no_borders ? "off" : "on",
	        c->acme_data->no_titlebars ? "off" : "on",
	        c->acme_data->smart_focus ? "smart" : "normal");
}