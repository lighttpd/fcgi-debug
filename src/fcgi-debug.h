#ifndef FCGI_DEBUG_H
#define FCGI_DEBUG_H

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

#define MAX_STREAM_BUF_SIZE (128*1024)

struct addr;
typedef struct addr addr;

struct server;
typedef struct server server;

struct stream;
typedef struct stream stream;

struct connection;
typedef struct connection connection;

struct addr {
	struct sockaddr *saddr;
	socklen_t addr_len;
};

struct server {
	struct ev_loop *loop;

	guint next_con_id;

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

struct stream {
	stream *other;
	int fd;
	ev_io watcher;
	GString *buffer;
};

typedef void (*DebugAppend)(gpointer ctx, const gchar *buf, gssize buflen);
typedef void (*DebugFree)(gpointer ctx);

struct connection {
	server *srv;
	guint con_id;
	stream s_server, s_client;
	gpointer ctx_server, ctx_client;
	DebugAppend da_server, da_client;
	DebugFree df_server, df_client;
	
	gboolean client_connected;
};


/* tools.c */
void move2fd(int srcfd, int dstfd);
void move2devnull(int fd);
void fd_init(int fd);

void ev_io_add_events(struct ev_loop *loop, ev_io *watcher, int events);
void ev_io_rem_events(struct ev_loop *loop, ev_io *watcher, int events);
void ev_io_set_events(struct ev_loop *loop, ev_io *watcher, int events);

GString* g_string_set_const(GString* s, const gchar *data, gsize len);
GString* g_string_escape(GString *data);

/* connection.c */
void connection_new(server *srv, int fd_server);

/* stream.c */
typedef void (*ev_io_cb)(struct ev_loop *loop, ev_io *w, int revents);

void stream_init(server *srv, stream *s1, stream *s2, int fd1, int fd2, ev_io_cb cb1, ev_io_cb cb2, void* data);
void stream_close(server *srv, stream *s1, stream *s2);
void stream_clean(server *srv, stream *s1, stream *s2);
void stream_start(server *srv, stream *s);

gssize stream_read(server *srv, stream *s, char *buf, gssize bufsize);
void stream_append(server *srv, stream *s, char *buf, gssize bufsize);
gssize stream_write(server *srv, stream *s);

/* log.c */
void log_raw(const gchar *head, gboolean from_server, guint con_id, GString *data);
void log_raw_split(const gchar *head, gboolean from_server, guint con_id, GString *data);

/* debug-fastcgi.c */
void setup_debug_fastcgi(connection *con);

static inline const char* from_server_to_string(gboolean from_server) {
	return from_server ? "webserver" : "application";
}

#endif