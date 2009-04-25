#include "fcgi-debug.h"

void log_raw_split(const gchar *head, gboolean from_server, guint con_id, guint req_id, GString *data) {
	const gchar *start = data->str, *end = start+data->len, *i;
	GString *line = g_string_sized_new(0);
	for ( ; start < end; ) {
		i = start;
		g_string_truncate(line, 0);
		for ( ; i < end; i++) {
			guchar c = *i;
			if (c == '\n') {
				g_string_append_len(line, "\\n", 2);
				i++;
				break;
			} else if (c == '\r') {
				g_string_append_len(line, "\\r", 2);
				if (i + 1 < end && i[1] == '\n') {
					i++;
					g_string_append_len(line, "\\n", 2);
				}
				i++;
				break;
			} else if (c < 0x20 || c >= 0x80) {
				static char hex[5] = "\\x00";
				hex[3] = ((c & 0xF) < 10) ? '0' + (c & 0xF) : 'A' + (c & 0xF) - 10;
				c /= 16;
				hex[2] = ((c & 0xF) < 10) ? '0' + (c & 0xF) : 'A' + (c & 0xF) - 10;
				g_string_append_len(line, hex, 4);
			} else {
				g_string_append_c(line, c);
			}
		}
		g_print("%s from %s (%u, %u): %s\n", head, from_server_to_string(from_server), con_id,  req_id, line->str);
		start = i;
	}
	g_string_free(line, TRUE);
}

void log_raw(const gchar *head, gboolean from_server, guint con_id, guint req_id, GString *data) {
	GString *line = g_string_escape(data);
	g_print("%s from %s (%u, %u): %s\n", head, from_server_to_string(from_server), con_id, req_id, line->str);
	g_string_free(line, TRUE);
}
