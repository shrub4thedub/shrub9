/*
 * Workspace management for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#ifndef WORKSPACE_H
#define WORKSPACE_H


#define MAX_WORKSPACES 10

typedef struct Workspace Workspace;

struct Workspace {
	int id;
	Client *clients;
	Client *current_client;
	int visible;
};

extern Workspace workspaces[MAX_WORKSPACES];
extern int current_workspace;
extern int workspace_count;

/* Function prototypes */
void workspace_init(int count);
void workspace_switch(int ws);
void workspace_add_client(Client *c, int ws);
void workspace_remove_client(Client *c);
void workspace_move_client(Client *c, int ws);
int workspace_get_current(void);
void workspace_show_all_clients(int ws);
void workspace_hide_all_clients(int ws);
Client* workspace_get_next_client(int ws);
void workspace_debug_dump(void);
void workspace_check_switching_state(void);
void workspace_cleanup(void);

#endif /* WORKSPACE_H */