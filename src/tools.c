#include "fcgi-debug.h"

/* move a fd to another and close the old one */
void move2fd(int srcfd, int dstfd) {
	if (srcfd != dstfd) {
		close(dstfd);
		dup2(srcfd, dstfd);
		close(srcfd);
	}
}

/* replace an fd with /dev/null */
void move2devnull(int fd) {
	move2fd(open("/dev/null", O_RDWR), fd);
}

void fd_init(int fd) {
#ifdef _WIN32
	int i = 1;
#endif
#ifdef FD_CLOEXEC
	/* close fd on exec (cgi) */
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
#ifdef O_NONBLOCK
	fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#elif defined _WIN32
	ioctlsocket(fd, FIONBIO, &i);
#endif
}

void ev_io_add_events(struct ev_loop *loop, ev_io *watcher, int events) {
	if ((watcher->events & events) == events) return;
	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, watcher->events | events);
	ev_io_start(loop, watcher);
}

void ev_io_rem_events(struct ev_loop *loop, ev_io *watcher, int events) {
	if (0 == (watcher->events & events)) return;
	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, watcher->events & ~events);
	ev_io_start(loop, watcher);
}

void ev_io_set_events(struct ev_loop *loop, ev_io *watcher, int events) {
	if (events == (watcher->events & (EV_READ | EV_WRITE))) return;
	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, (watcher->events & ~(EV_READ | EV_WRITE)) | events);
	ev_io_start(loop, watcher);
}

GString* g_string_set_const(GString* s, const gchar *data, gsize len) {
	s->str = (gchar*) data;
	s->len = len;
	return s;
}

GString* g_string_escape(GString *data) {
	GString *s = g_string_sized_new(0);
	const gchar *start = data->str, *end = start+data->len, *i;
	for (i = start; i < end; i++) {
		guchar c = *i;
		if (c == '\n') {
			g_string_append_len(s, "\\n", 2);
		} else if (c == '\r') {
			g_string_append_len(s, "\\r", 2);
		} else if (c < 0x20 || c >= 0x80) {
			static char hex[5] = "\\x00";
			hex[3] = ((c & 0xF) < 10) ? '0' + (c & 0xF) : 'A' + (c & 0xF) - 10;
			c /= 16;
			hex[2] = ((c & 0xF) < 10) ? '0' + (c & 0xF) : 'A' + (c & 0xF) - 10;
			g_string_append_len(s, hex, 4);
		} else {
			g_string_append_c(s, c);
		}
	}
	return s;
}
