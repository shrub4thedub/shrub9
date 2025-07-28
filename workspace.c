/*
 * Workspace management for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"
#include "workspace.h"
#include "config.h"

Workspace workspaces[MAX_WORKSPACES];
int current_workspace = 0;
int workspace_count = 1;

/* Helper function to validate client exists in global list */
static int
client_exists_in_global_list(Client *target)
{
	Client *c;
	
	if (!target)
		return 0;
		
	for (c = clients; c; c = c->next) {
		if (c == target)
			return 1;
	}
	return 0;
}

void
workspace_init(int count)
{
	int i;
	
	fprintf(stderr, "workspace_init: initializing with count=%d\n", count);
	
	if (count > MAX_WORKSPACES)
		count = MAX_WORKSPACES;
	if (count < 1)
		count = 1;
		
	workspace_count = count;
	fprintf(stderr, "workspace_init: final workspace_count=%d\n", workspace_count);
	
	for (i = 0; i < MAX_WORKSPACES; i++) {
		workspaces[i].id = i;
		workspaces[i].clients = NULL;
		workspaces[i].current_client = NULL;
		workspaces[i].visible = (i == 0) ? 1 : 0;
	}
	
	current_workspace = 0;
}

void
workspace_switch(int ws)
{
	int old_ws;
	
	if (ws < 0 || ws >= workspace_count || ws == current_workspace)
		return;
	
	old_ws = current_workspace;
	
	fprintf(stderr, "workspace_switch: switching from workspace %d to %d\n", old_ws, ws);
	
	/* Set flag to prevent workspace removal during switching */
	workspace_switching = 1;
	
	workspace_hide_all_clients(old_ws);
	workspaces[old_ws].visible = 0;
	
	current_workspace = ws;
	workspace_show_all_clients(ws);
	workspaces[current_workspace].visible = 1;
	
	/* Rebuild menu since hidden clients may have changed */
	rebuild_menu();
	
	if (workspaces[ws].current_client) {
		active(workspaces[ws].current_client);
	} else {
		/* Set current to NULL without calling nofocus() to avoid grab issues */
		if (current)
			setactive(current, 0);
		current = NULL;
	}
	
	/* Note: Don't clear workspace_switching flag here - it will be cleared
	   when all pending unmap events have been processed to prevent premature
	   client removal during workspace switches */
	
	fprintf(stderr, "workspace_switch: completed switch to workspace %d\n", ws);
	workspace_debug_dump();
}

void
workspace_add_client(Client *c, int ws)
{
	Client **head;
	
	fprintf(stderr, "workspace_add_client: ENTRY - c=%p ws=%d workspace_count=%d\n", (void*)c, ws, workspace_count);
	
	if (!c || ws < 0 || ws >= workspace_count) {
		fprintf(stderr, "workspace_add_client: FAILED validation - c=%p ws=%d workspace_count=%d\n", (void*)c, ws, workspace_count);
		return;
	}
	
	/* Note: Skip validation for now as it may be causing issues */
	
	fprintf(stderr, "workspace_add_client: adding client %p (window=0x%lx) to workspace %d\n", 
		(void*)c, c->window, ws);
	
	/* Remove from current workspace if already assigned */
	if (c->workspace >= 0) {
		fprintf(stderr, "workspace_add_client: removing from current workspace %d\n", c->workspace);
		workspace_remove_client(c);
	}
		
	c->workspace = ws;
	fprintf(stderr, "workspace_add_client: assigned c->workspace = %d\n", c->workspace);
	head = &workspaces[ws].clients;
	
	c->workspace_next = *head;
	if (*head)
		(*head)->workspace_prev = c;
	c->workspace_prev = NULL;
	*head = c;
	
	fprintf(stderr, "workspace_add_client: after list insertion, c->workspace = %d\n", c->workspace);
	
	/* CRITICAL: If adding to non-current workspace, need to handle visibility */
	if (ws != current_workspace && c->state == WithdrawnState) {
		fprintf(stderr, "workspace_add_client: client added to non-current workspace %d (current=%d)\n", ws, current_workspace);
		/* Note: Visibility will be handled in manage.c after the window is mapped */
	}
	
	fprintf(stderr, "workspace_add_client: client %p successfully added to workspace %d\n", 
		(void*)c, ws);
	fprintf(stderr, "workspace_add_client: VERIFICATION - c->workspace is now %d\n", c->workspace);
	
	/* Verify the client is actually in the workspace list */
	{
		Client *verify;
		int found = 0;
		for (verify = workspaces[ws].clients; verify; verify = verify->workspace_next) {
			if (verify == c) {
				found = 1;
				break;
			}
		}
		fprintf(stderr, "workspace_add_client: client %p found in workspace %d list: %s\n", 
			(void*)c, ws, found ? "YES" : "NO");
	}
}

void
workspace_remove_client(Client *c)
{
	int ws;
	
	if (!c)
		return;
	
	/* Add stack trace to see who called this */
	fprintf(stderr, "workspace_remove_client: CALLED for client %p (window=0x%lx)\n", (void*)c, c->window);
	fprintf(stderr, "workspace_remove_client: STACK TRACE - check who called this!\n");
	
	/* Note: Don't validate global list here since this might be called during client destruction */
		
	ws = c->workspace;
	fprintf(stderr, "workspace_remove_client: removing client %p (window=0x%lx) from workspace %d\n", 
		(void*)c, c->window, ws);
		
	if (ws < 0 || ws >= workspace_count) {
		/* Client not assigned to any workspace, just clear fields */
		fprintf(stderr, "workspace_remove_client: client not in valid workspace, just clearing fields\n");
		c->workspace = -1;
		c->workspace_next = NULL;
		c->workspace_prev = NULL;
		return;
	}
	
	if (workspaces[ws].current_client == c)
		workspaces[ws].current_client = NULL;
	
	if (c->workspace_prev)
		c->workspace_prev->workspace_next = c->workspace_next;
	else
		workspaces[ws].clients = c->workspace_next;
		
	if (c->workspace_next)
		c->workspace_next->workspace_prev = c->workspace_prev;
	
	c->workspace_next = NULL;
	c->workspace_prev = NULL;
	c->workspace = -1;
	
	fprintf(stderr, "workspace_remove_client: client %p successfully removed from workspace %d\n", 
		(void*)c, ws);
	fprintf(stderr, "workspace_remove_client: VERIFICATION - c->workspace is now %d\n", c->workspace);
}

void
workspace_move_client(Client *c, int ws)
{
	int old_ws;
	
	fprintf(stderr, "workspace_move_client: ENTRY - moving client %p from workspace %d to %d\n", 
		(void*)c, c->workspace, ws);
	
	if (!c || ws < 0 || ws >= workspace_count || c->workspace == ws) {
		fprintf(stderr, "workspace_move_client: EARLY RETURN - c=%p ws=%d workspace_count=%d c->workspace=%d\n", 
			(void*)c, ws, workspace_count, c ? c->workspace : -999);
		return;
	}
	
	/* Validate client exists in global list */
	if (!client_exists_in_global_list(c)) {
		fprintf(stderr, "workspace_move_client: client %p not in global list\n", (void*)c);
		return;
	}
	
	old_ws = c->workspace;
	fprintf(stderr, "workspace_move_client: proceeding with move from %d to %d\n", old_ws, ws);
		
	workspace_remove_client(c);
	workspace_add_client(c, ws);
	
	/* Verify the move succeeded */
	if (c->workspace != ws) {
		fprintf(stderr, "workspace_move_client: move failed, rolling back\n");
		/* Try to restore to original workspace */
		if (old_ws >= 0 && old_ws < workspace_count) {
			workspace_add_client(c, old_ws);
		}
		return;
	}
	
	/* Ensure proper visibility after move */
	if (ws == current_workspace) {
		/* Moving to current workspace - make sure it's visible */
		if (c->state != WithdrawnState) {
			XMapWindow(dpy, c->window);
			XMapRaised(dpy, c->parent);
			setwstate(c, NormalState);
		}
	} else {
		/* Moving to non-current workspace - make sure it's hidden */
		if (c->state != WithdrawnState) {
			/* Set workspace switching protection to prevent withdrawal on UnmapNotify */
			workspace_switching = 1;
			pending_workspace_unmaps += 2;  /* parent + window */
			fprintf(stderr, "workspace_move_client: setting protection for unmap events (pending=%d)\n", pending_workspace_unmaps);
			
			XUnmapWindow(dpy, c->parent);
			XUnmapWindow(dpy, c->window);
		}
		/* If this was the current window, clear the global current */
		if (c == current) {
			setactive(current, 0);
			current = NULL;
		}
	}
}

int
workspace_get_current(void)
{
	return current_workspace;
}

void
workspace_show_all_clients(int ws)
{
	Client *c;
	int count = 0;
	
	if (ws < 0 || ws >= workspace_count)
		return;
	
	fprintf(stderr, "workspace_show_all_clients: showing clients in workspace %d\n", ws);
		
	for (c = workspaces[ws].clients; c; c = c->workspace_next) {
		count++;
		fprintf(stderr, "  client %d: %p (window=0x%lx, state=%d)\n", 
			count, (void*)c, c->window, c->state);
		
		/* Validate client exists in global list */
		if (!client_exists_in_global_list(c)) {
			fprintf(stderr, "workspace_show_all_clients: invalid client %p in workspace %d\n", (void*)c, ws);
			continue;
		}
		
		/* Show all clients that were hidden by workspace switching */
		if (c->state != WithdrawnState) {
			/* Only map windows that should be visible (not in IconicState) */
			if (c->state != IconicState) {
				XMapWindow(dpy, c->window);
				XMapRaised(dpy, c->parent);
				setwstate(c, NormalState);
				fprintf(stderr, "  mapped client %p\n", (void*)c);
			} else {
				/* Keep hidden windows hidden - don't map them */
				fprintf(stderr, "  keeping client %p hidden (IconicState)\n", (void*)c);
			}
		}
	}
	workspaces[ws].visible = 1;
	fprintf(stderr, "workspace_show_all_clients: showed %d clients in workspace %d\n", count, ws);
}

void
workspace_hide_all_clients(int ws)
{
	Client *c;
	int count = 0;
	
	if (ws < 0 || ws >= workspace_count)
		return;
	
	fprintf(stderr, "workspace_hide_all_clients: hiding clients in workspace %d\n", ws);
		
	for (c = workspaces[ws].clients; c; c = c->workspace_next) {
		count++;
		fprintf(stderr, "  hiding client %d: %p (window=0x%lx, state=%d)\n", 
			count, (void*)c, c->window, c->state);
		
		/* Validate client exists in global list */
		if (!client_exists_in_global_list(c)) {
			fprintf(stderr, "workspace_hide_all_clients: invalid client %p in workspace %d\n", (void*)c, ws);
			continue;
		}
		
		if (c->state != WithdrawnState) {
			/* Simply hide the window - workspace switching flag prevents removal */
			XUnmapWindow(dpy, c->parent);
			XUnmapWindow(dpy, c->window);
			/* Track this as a workspace switch unmap - we expect 2 UnmapNotify events per client
			   (one for parent, one for window), so increment by 2 */
			pending_workspace_unmaps += 2;
			fprintf(stderr, "workspace_hide: unmapped client %p (switching flag protects from removal, pending=%d)\n", (void*)c, pending_workspace_unmaps);
		}
	}
	workspaces[ws].visible = 0;
	fprintf(stderr, "workspace_hide_all_clients: hid %d clients in workspace %d\n", count, ws);
}

Client*
workspace_get_next_client(int ws)
{
	if (ws < 0 || ws >= workspace_count)
		return NULL;
		
	return workspaces[ws].clients;
}

void
workspace_debug_dump(void)
{
	int i;
	Client *c;
	int count;
	
	fprintf(stderr, "\n=== WORKSPACE DEBUG DUMP ===\n");
	fprintf(stderr, "Current workspace: %d, Workspace count: %d\n", current_workspace, workspace_count);
	
	for (i = 0; i < workspace_count; i++) {
		count = 0;
		fprintf(stderr, "Workspace %d (visible=%d):\n", i, workspaces[i].visible);
		for (c = workspaces[i].clients; c; c = c->workspace_next) {
			count++;
			fprintf(stderr, "  %d: client %p window=0x%lx state=%d workspace=%d\n", 
				count, (void*)c, c->window, c->state, c->workspace);
		}
		if (count == 0) {
			fprintf(stderr, "  (no clients)\n");
		}
	}
	
	/* Also dump global clients list */
	count = 0;
	fprintf(stderr, "Global clients list:\n");
	for (c = clients; c; c = c->next) {
		count++;
		fprintf(stderr, "  %d: client %p window=0x%lx state=%d workspace=%d\n", 
			count, (void*)c, c->window, c->state, c->workspace);
	}
	if (count == 0) {
		fprintf(stderr, "  (no clients in global list)\n");
	}
	fprintf(stderr, "=== END WORKSPACE DEBUG DUMP ===\n\n");
}

void
workspace_check_switching_state(void)
{
	/* Safety mechanism: if workspace_switching has been set for too long,
	   or if pending_workspace_unmaps seems stuck, reset the state */
	if (workspace_switching && pending_workspace_unmaps == 0) {
		fprintf(stderr, "workspace_check_switching_state: clearing stuck workspace_switching flag\n");
		workspace_switching = 0;
	}
}

void
workspace_cleanup(void)
{
	int i;
	
	for (i = 0; i < MAX_WORKSPACES; i++) {
		workspaces[i].clients = NULL;
		workspaces[i].current_client = NULL;
		workspaces[i].visible = 0;
	}
	current_workspace = 0;
	workspace_count = 1;
	workspace_switching = 0;
	pending_workspace_unmaps = 0;
}