#include "fcgi-debug.h"

void stream_init(server *srv, stream *s1, stream *s2, int fd1, int fd2, ev_io_cb cb1, ev_io_cb cb2, void* data) {
	UNUSED(srv);
	fd_init(fd1);
	fd_init(fd2);
	s1->other = s2;
	s2->other = s1;
	s1->fd = fd1;
	s2->fd = fd2;
	ev_io_init(&s1->watcher, cb1, fd1, 0);
	ev_io_init(&s2->watcher, cb2, fd2, 0);
	s1->watcher.data = data;
	s2->watcher.data = data;
	s1->buffer = g_string_sized_new(0);
	s2->buffer = g_string_sized_new(0);
}

void stream_close(server *srv, stream *s1, stream *s2) {
	ev_io_stop(srv->loop, &s1->watcher);
	if (s1->fd != -1) {
		shutdown(s1->fd, SHUT_RDWR);
		close(s1->fd);
		s1->fd = -1;
	}
	ev_io_stop(srv->loop, &s2->watcher);
	if (s2->fd != -1) {
		shutdown(s2->fd, SHUT_RDWR);
		close(s2->fd);
		s2->fd = -1;
	}
}

void stream_clean(server *srv, stream *s1, stream *s2) {
	stream_close(srv, s1, s2);
	if (s1->buffer) { g_string_free(s1->buffer, TRUE); s1->buffer = NULL; }
	if (s2->buffer) { g_string_free(s2->buffer, TRUE); s2->buffer = NULL; }
}

void stream_start(server *srv, stream *s) {
	int events = EV_READ;
	if (s->buffer->len > 0) events |= EV_WRITE;
	ev_io_add_events(srv->loop, &s->watcher, events);
}

/* -1: connection got closed, 0: nothing to read, n: read n bytes */
gssize stream_read(server *srv, stream *s, char *buf, gssize bufsize) {
	gssize len;
	while (-1 == (len = read(s->fd, buf, bufsize))) {
		switch (errno) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			/* nothing to read */
			return 0;
		case ECONNRESET:
			stream_close(srv, s, s->other);
			return -1;
		case EINTR: /* try again */
			break;
		default:
			g_message("read error: %s", g_strerror(errno));
			stream_close(srv, s, s->other);
			return -1;
		}
	}
	if (0 == len) { /* connection closed */
		stream_close(srv, s, s->other);
		return -1;
	}
	return len;
}

void stream_append(server *srv, stream *s, char *buf, gssize bufsize) {
	g_string_append_len(s->buffer, buf, bufsize);
	if (s->buffer->len > 0) ev_io_add_events(srv->loop, &s->watcher, EV_WRITE);
	if (s->buffer->len > MAX_STREAM_BUF_SIZE) ev_io_rem_events(srv->loop, &s->other->watcher, EV_READ);
}

/* -1: connection closed, n: wrote n bytes */
gssize stream_write(server *srv, stream *s) {
	gssize len;
	while (-1 == (len = write(s->fd, s->buffer->str, s->buffer->len))) {
		switch (errno) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			/* try again later */
			return 0;
		case ECONNRESET:
		case EPIPE:
			stream_close(srv, s, s->other);
			return -1;
		case EINTR: /* try again */
			break;
		default:
			g_message("read error: %s", g_strerror(errno));
			stream_close(srv, s, s->other);
			return -1;
		}
	}
	return 0;
}
