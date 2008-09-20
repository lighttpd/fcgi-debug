#include "fcgi-debug.h"

static void connection_close(connection *con) {
	stream_close(con->srv, &con->s_server, &con->s_client);
	g_slice_free(connection, con);
}

static gboolean connection_connect(connection *con) {
	server *srv = con->srv;
	if (con->client_connected) return TRUE;
	ev_io_start(srv->loop, &srv->w_accept);
	if (-1 == connect(con->s_client.fd, srv->client.saddr, srv->client.addr_len)) {
		switch (errno) {
		case EALREADY:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
		case EINPROGRESS:
		case EINTR:
			ev_io_stop(srv->loop, &srv->w_accept); /* no new connections until we have a new connection to the client */
			ev_io_set_events(srv->loop, &con->s_client.watcher, EV_WRITE | EV_READ);
			break;
		default:
			g_warning("couldn't connect (%u): %s", con->con_id, g_strerror(errno));
			connection_close(con);
		}
		return FALSE;
	} else {
		con->client_connected = TRUE;
		stream_start(srv, &con->s_server);
		stream_start(srv, &con->s_client);
		return TRUE;
	}
}

static char readbuf[4096];

static void fd_server_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection *con = (connection*) w->data;
	server *srv = con->srv;
	GString s;
	UNUSED(loop);

	if (revents & EV_READ) {
		gssize len = stream_read(srv, &con->s_server, readbuf, sizeof(readbuf));
		if (len == -1) {
			g_print("connection closed (%u)\n", con->con_id);
			connection_close(con);
			return;
		}
		log_raw("raw from server", con->con_id, g_string_set_const(&s, readbuf, len));
		stream_append(srv, &con->s_client, readbuf, len);
	}
	if (revents & EV_WRITE) {
		if (-1 == stream_write(srv, &con->s_server)) {
			g_print("connection closed (%u)\n", con->con_id);
			connection_close(con);
			return;
		}
	}
}

static void fd_client_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection *con = (connection*) w->data;
	server *srv = con->srv;
	GString s;
	UNUSED(loop);
	if (!connection_connect(con)) return;

	if (revents & EV_READ) {
		gssize len = stream_read(srv, &con->s_client, readbuf, sizeof(readbuf));
		if (len == -1) {
			g_print("connection closed (%u)\n", con->con_id);
			connection_close(con);
			return;
		}
		log_raw("raw from client", con->con_id, g_string_set_const(&s, readbuf, len));
		stream_append(srv, &con->s_server, readbuf, len);
	}
	if (revents & EV_WRITE) {
		if (-1 == stream_write(srv, &con->s_client)) {
			g_print("connection closed (%u)\n", con->con_id);
			connection_close(con);
			return;
		}
	}
}

void connection_new(server *srv, int fd_server) {
	connection *con;
	int fd_client;

	fd_init(fd_server);

	if (-1 == (fd_client = socket(AF_UNIX, SOCK_STREAM, 0))) {
		g_warning("couldn't create socket: %s", g_strerror(errno));
		shutdown(fd_server, SHUT_RDWR);
		close(fd_server);
		return;
	}
	con = g_slice_new0(connection);
	con->srv = srv;
	con->con_id = srv->next_con_id++;
	con->client_connected = FALSE;
	g_print("new connection (%u)\n", con->con_id);
	stream_init(srv, &con->s_server, &con->s_client, fd_server, fd_client, fd_server_cb, fd_client_cb, con);
	connection_connect(con);
}
