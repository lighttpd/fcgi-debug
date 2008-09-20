
#include <ev.h>
#include <glib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define UNUSED(x) ( (void)(x) )

struct addr;
typedef struct addr addr;

struct server;
typedef struct server server;

struct connection;
typedef struct connection connection;

struct addr {
	struct sockaddr *saddr;
	socklen_t addr_len;
};

struct server {
	struct ev_loop *loop;

	ev_signal
		sig_w_INT,
		sig_w_TERM,
		sig_w_PIPE,
		sig_w_HUP
		;

	ev_child w_child;
	ev_io w_accept;
	int child;

	gchar *tmpfile_name, *sockfile_name;
	addr client;

	gboolean exiting, stopped_signals;
};

struct connection {
	server *srv;
	int fd_server, fd_client;
	ev_io w_server, w_client;
	gboolean client_connected;

	GString *send_client_buf, *send_server_buf;
};


/* tools.c */
void move2fd(int srcfd, int dstfd);
void move2devnull(int fd);
void fd_init(int fd);

void ev_io_add_events(struct ev_loop *loop, ev_io *watcher, int events);
void ev_io_rem_events(struct ev_loop *loop, ev_io *watcher, int events);
void ev_io_set_events(struct ev_loop *loop, ev_io *watcher, int events);

/* connection.c */
void connection_new(server *srv, int fd_server);
