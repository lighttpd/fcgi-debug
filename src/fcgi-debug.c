
#include "fcgi-debug.h"

static void accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	server *srv = (server*) w->data;
	int fd;
	UNUSED(loop);
	UNUSED(revents);

	if (-1 == (fd = accept(w->fd, NULL, NULL))) {
		g_error("Couldn't accept: %s", g_strerror(errno));
	}

	connection_new(srv, fd);
}

#define CATCH_SIGNAL(loop, cb, n) do {\
	ev_init(&srv->sig_w_##n, cb); \
	ev_signal_set(&srv->sig_w_##n, SIG##n); \
	ev_signal_start(loop, &srv->sig_w_##n); \
	srv->sig_w_##n.data = srv; \
	ev_unref(loop); /* Signal watchers shouldn't keep loop alive */ \
} while (0)

#define UNCATCH_SIGNAL(loop, n) do {\
	ev_ref(loop); \
	ev_signal_stop(loop, &srv->sig_w_##n); \
} while (0)

void server_stop(server *srv) {
	if (srv->tmpfile_name) {
		unlink(srv->tmpfile_name);
		g_free(srv->tmpfile_name);
		srv->tmpfile_name = NULL;
	}
	if (srv->sockfile_name) {
		unlink(srv->sockfile_name);
		g_free(srv->sockfile_name);
		srv->sockfile_name = NULL;
	}
	ev_io_stop(srv->loop, &srv->w_accept);
	close(0);
	if (!srv->exiting) {
		if (-1 != srv->child) kill(srv->child, SIGINT);
	} else {
		if (-1 != srv->child) kill(srv->child, SIGTERM);

		if (!srv->stopped_signals) {
			srv->stopped_signals = TRUE;
			/* reset default behaviour which will kill us next time */
			UNCATCH_SIGNAL(srv->loop, INT);
			UNCATCH_SIGNAL(srv->loop, TERM);
			UNCATCH_SIGNAL(srv->loop, PIPE);
			UNCATCH_SIGNAL(srv->loop, HUP);
		}
	}
}

static void child_cb(struct ev_loop *loop, struct ev_child *w, int revents) {
	server *srv = (server*) w->data;
	UNUSED(revents);

	ev_child_stop(loop, w);
	g_message ("process %d exited with status %d", w->rpid, w->rstatus);
	srv->child = -1;
	server_stop(srv);
}

static void sigint_cb(struct ev_loop *loop, struct ev_signal *w, int revents) {
	server *srv = (server*) w->data;
	UNUSED(loop);
	UNUSED(revents);

	if (!srv->exiting) {
		g_message("Got signal, shutdown");
	} else if (w->signum != SIGINT && w->signum != SIGTERM) {
		return; /* ignore */
	} else {
		g_message("Got second signal, force shutdown");
	}
	server_stop(srv);
	if (w->signum == SIGINT || w->signum == SIGTERM) srv->exiting = TRUE;
}

static void sigpipe_cb(struct ev_loop *loop, struct ev_signal *w, int revents) {
	/* ignore */
	UNUSED(loop); UNUSED(w); UNUSED(revents);
}

void create_tmp_addr(server *srv) {
	gchar *fn = g_strdup("/tmp/fcgi-debug.XXXXXX");
	int fd;

	struct sockaddr_un *sun;
	gsize slen = strlen(fn) + sizeof(".sock") - 1, len = 1 + slen + (gsize) (((struct sockaddr_un *) 0)->sun_path);
	sun = (struct sockaddr_un*) g_malloc0(len);
	sun->sun_family = AF_UNIX;

	if (-1 == (fd = mkstemp(fn))) {
		g_error("Couldn't make a tmpfile name");
	}
	close(fd);
	strcpy(sun->sun_path, fn);
	strcat(sun->sun_path, ".sock");
	srv->sockfile_name = g_strdup(sun->sun_path);
	srv->tmpfile_name = fn;

	srv->client.saddr = (struct sockaddr*) sun;
	srv->client.addr_len = len;
}

int client_bind(server *srv) {
	int s;

	if (-1 == (s = socket(AF_UNIX, SOCK_STREAM, 0))) {
		g_error("Couldn't create socket: %s", g_strerror(errno));
	}

	if (-1 == bind(s, srv->client.saddr, srv->client.addr_len)) {
		g_error("Couldn't bind socket: %s", g_strerror(errno));
	}

	if (-1 == listen(s, 1024)) {
		g_error("Couldn't listen on socket: %s", g_strerror(errno));
	}

	return s;
}

pid_t spawn(char **argv, int s) {
	pid_t child;

	switch (child = fork()) {
	case -1:
		g_error("Fork failed: %s", g_strerror(errno));
		break;
	case 0: /* child */
		setsid();
		move2fd(s, 0);
		execv(argv[1], argv+1);
		g_error("execv failed: %s", g_strerror(errno));
		break;
	default:
		close(s);
		break;
	}

	return child;
}

int main(int argc, char **argv) {
	server *srv;
	int s;
	pid_t ch;
	UNUSED(argc);

	srv = g_slice_new0(server);
	srv->exiting = FALSE;
	srv->child = -1;
	srv->loop = ev_default_loop (0);

	CATCH_SIGNAL(srv->loop, sigint_cb, INT);
	CATCH_SIGNAL(srv->loop, sigint_cb, TERM);
	CATCH_SIGNAL(srv->loop, sigint_cb, HUP);
	CATCH_SIGNAL(srv->loop, sigpipe_cb, PIPE);

	create_tmp_addr(srv);
	s = client_bind(srv);

	ch = spawn(argv, s);

	srv->child = ch;
	ev_child_init(&srv->w_child, child_cb, ch, 0);
	srv->w_child.data = srv;
	ev_child_start(srv->loop, &srv->w_child);

	fd_init(0);
	ev_io_init(&srv->w_accept, accept_cb, 0, EV_READ);
	srv->w_accept.data = srv;
	ev_io_start(srv->loop, &srv->w_accept);

	ev_loop(srv->loop, 0);

	g_message("exit fcgi-debug");
	server_stop(srv);
	return 0;
}
