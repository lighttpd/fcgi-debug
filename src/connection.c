#include "fcgi-debug.h"

static void connection_close(connection *con) {
	ev_io_stop(con->srv->loop, &con->w_server);
	ev_io_stop(con->srv->loop, &con->w_client);
	if (-1 != con->fd_server) {
		shutdown(con->fd_server, SHUT_RDWR);
		close(con->fd_server);
	}
	if (-1 != con->fd_client) {
		shutdown(con->fd_client, SHUT_RDWR);
		close(con->fd_client);
	}
	g_string_free(con->send_client_buf, TRUE);
	g_string_free(con->send_server_buf, TRUE);
	g_slice_free(connection, con);
}

static gboolean connection_connect(connection *con) {
	server *srv = con->srv;
	if (con->client_connected) return TRUE;
	ev_io_start(srv->loop, &srv->w_accept);
	if (-1 == connect(con->fd_client, srv->client.saddr, srv->client.addr_len)) {
		switch (errno) {
		case EALREADY:
		case EINPROGRESS:
		case EINTR:
			ev_io_stop(srv->loop, &srv->w_accept); /* no new connections until we have a new connection to the client */
			ev_io_set_events(srv->loop, &con->w_client, EV_WRITE | EV_READ);
			break;
		default:
			g_warning("couldn't connect: %s", g_strerror(errno));
			connection_close(con);
		}
		return FALSE;
	} else {
		con->client_connected = TRUE;
		ev_io_set_events(srv->loop, &con->w_client, EV_READ);
		ev_io_set_events(srv->loop, &con->w_server, EV_READ);
		return TRUE;
	}
}

static char readbuf[4096];

static void fd_server_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection *con = (connection*) w->data;
	server *srv = con->srv;
	UNUSED(loop);

	if (revents & EV_READ) {
		int l = read(w->fd, readbuf, sizeof(readbuf));
		switch (l) {
		case -1:
			switch (errno) {
			case EAGAIN:
			case EINTR:
				break;
			default:
				g_warning("couldn't read from server: %s", g_strerror(errno));
				connection_close(con);
			}
			return;
		case 0:
			/* end of file */
			connection_close(con);
			return;
		default:
			break;
		}
		g_string_append_len(con->send_client_buf, readbuf, l);
		if (con->send_client_buf->len > 4*4096) ev_io_rem_events(srv->loop, w, EV_READ);
		ev_io_add_events(srv->loop, &con->w_client, EV_WRITE);
	}
	if (revents & EV_WRITE) {
		if (con->send_server_buf->len > 0) {
			int l = write(w->fd, con->send_server_buf->str, con->send_server_buf->len);
			switch (l) {
			case -1:
				switch (errno) {
				case EAGAIN:
				case EINTR:
					break;
				default:
					g_warning("couldn't write to server: %s", g_strerror(errno));
					connection_close(con);
				}
				return;
			}
			g_string_erase(con->send_server_buf, 0, l);
			if (con->send_server_buf->len < 4*4096) ev_io_add_events(srv->loop, &con->w_server, EV_READ);
		}
		if (con->send_server_buf->len == 0) ev_io_rem_events(srv->loop, w, EV_WRITE);
	}
}

static void fd_client_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection *con = (connection*) w->data;
	server *srv = con->srv;
	UNUSED(loop);
	if (!connection_connect(con)) return;

	if (revents & EV_READ) {
		int l = read(w->fd, readbuf, sizeof(readbuf));
		switch (l) {
		case -1:
			switch (errno) {
			case EAGAIN:
			case EINTR:
				break;
			default:
				g_warning("couldn't read from client: %s", g_strerror(errno));
				connection_close(con);
			}
			return;
		case 0:
			/* end of file */
			connection_close(con);
			return;
		default:
			break;
		}
		g_string_append_len(con->send_server_buf, readbuf, l);
		if (con->send_server_buf->len > 4*4096) ev_io_rem_events(srv->loop, w, EV_READ);
		ev_io_add_events(srv->loop, &con->w_server, EV_WRITE);
	}
	if (revents & EV_WRITE) {
		if (con->send_client_buf->len > 0) {
			int l = write(w->fd, con->send_client_buf->str, con->send_client_buf->len);
			switch (l) {
			case -1:
				switch (errno) {
				case EAGAIN:
				case EINTR:
					break;
				default:
					g_warning("couldn't write to client: %s", g_strerror(errno));
					connection_close(con);
				}
				return;
			}
			g_string_erase(con->send_client_buf, 0, l);
			if (con->send_client_buf->len < 4*4096) ev_io_add_events(srv->loop, &con->w_client, EV_READ);
		}
		if (con->send_client_buf->len == 0) ev_io_rem_events(srv->loop, w, EV_WRITE);
	}
}

void connection_new(server *srv, int fd_server) {
	connection *con;
	int fd_client;

	fd_init(fd_server);
	con = g_slice_new0(connection);
	con->srv = srv;
	con->fd_server = fd_server;
	con->fd_client = -1;
	ev_io_init(&con->w_server, fd_server_cb, fd_server, 0);
	con->w_server.data = con;
	ev_io_init(&con->w_client, fd_client_cb, -1, 0);
	con->w_client.data = con;
	con->send_client_buf = g_string_sized_new(0);
	con->send_server_buf = g_string_sized_new(0);

	if (-1 == (fd_client = socket(AF_UNIX, SOCK_STREAM, 0))) {
		g_warning("couldn't create socket: %s", g_strerror(errno));
		connection_close(con);
	}
	fd_init(fd_client);
	con->fd_client = fd_client;
	ev_io_set(&con->w_client, fd_client, EV_WRITE | EV_READ);
	con->client_connected = FALSE;
	connection_connect(con);
}
