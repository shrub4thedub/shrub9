/*
 * Plumber integration for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#ifndef PLUMB_H
#define PLUMB_H

#include <X11/Xlib.h>

#define PLUMB_MAX_TEXT 1024
#define PLUMB_MAX_ATTR 512
#define PLUMB_MAX_DATA 8192

typedef struct PlumbMsg PlumbMsg;
typedef struct PlumbAttr PlumbAttr;

struct PlumbAttr {
	char *name;
	char *value;
	PlumbAttr *next;
};

struct PlumbMsg {
	char *src;		/* source application */
	char *dst;		/* destination port */
	char *wdir;		/* working directory */
	char *type;		/* data type */
	PlumbAttr *attr;	/* attributes */
	int ndata;		/* length of data */
	char *data;		/* the data itself */
};

/* Global plumber state */
extern int plumb_enabled;
extern int plumb_fd;

/* Function prototypes */
int plumb_init(void);
void plumb_cleanup(void);
int plumb_send_text(const char *text, Client *c);
char* plumb_get_selection(Client *c, int x, int y);
int plumb_handle_selection(Client *c, int x, int y);
PlumbMsg* plumb_msg_new(void);
void plumb_msg_free(PlumbMsg *m);
void plumb_attr_add(PlumbMsg *m, const char *name, const char *value);
int plumb_msg_send(PlumbMsg *m);
int plumb_open_port(const char *port, int mode);
void plumb_close_port(int fd);

/* Selection handling */
char* plumb_get_window_selection(Window w);
int plumb_is_text_selected(Client *c, int x, int y);

/* Plan 9 plumber message format helpers */
char* plumb_format_message(const char *src, const char *dst, const char *wdir, 
                          const char *type, const char *data, PlumbAttr *attr);
int plumb_parse_message(const char *msg, PlumbMsg *pm);

/* Default plumber paths */
#define DEFAULT_PLUMB_SEND_PATH "/mnt/plumb/send"
#define DEFAULT_PLUMB_RULES_PATH "/mnt/plumb/rules"

/* Standard plumber ports */
#define PLUMB_EDIT_PORT "edit"
#define PLUMB_SEND_PORT "send"
#define PLUMB_WEB_PORT "web"

#endif /* PLUMB_H */