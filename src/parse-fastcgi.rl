
#include "debug-fastcgi.h"

fcgi_context* fcgi_context_new(gboolean from_server, guint con_id) {
	fcgi_context *ctx = g_slice_new0(fcgi_context);
	ctx->buffer = g_string_sized_new(0);
	ctx->from_server = from_server;
	ctx->con_id = con_id;

	ctx->s_params = g_string_sized_new(0);

	return ctx;
}

void fcgi_context_free(gpointer _ctx) {
	fcgi_context* ctx = (fcgi_context*) _ctx;
	g_string_free(ctx->buffer, TRUE);
	g_string_free(ctx->s_params, TRUE);
	g_slice_free(fcgi_context, ctx);
}

#define USE_STREAM(s, p, pe) do {\
	g_string_append_len(ctx->s, (gchar*) p, pe - p); \
	p = (guint8*) ctx->s->str; \
	pe = p + ctx->s->len; \
} while(0)

#define EAT_STREAM(s, p) do {\
	g_string_erase(ctx->s, 0, p - (guint8*) ctx->s->str); \
	p = (guint8*) ctx->s->str; \
	pe = p + ctx->s->len; \
} while(0)

static gboolean get_key_value_pair_len(guint8 **_p, guint8 *pe, guint *_len1, guint *_len2) {
	guint8 *p = *_p;
	guint len1, len2;
	/* need at least 2 bytes */
	if (p + 2 >= pe) return FALSE;
	len1 = *p++;
	if (len1 & 0x80) {
		/* need at least 3+1 bytes */
		if (p + 4 >= pe) return FALSE;
		len1 = ((len1 & 0x7F) << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
		p += 3;
	}
	len2 = *p++;
	if (len2 & 0x80) {
		/* need at least 3 bytes */
		if (p + 3 >= pe) return FALSE;
		len2 = ((len2 & 0x7F) << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
		p += 3;
	}
	*_len1 = len1;
	*_len2 = len2;
	*_p = p;
	return TRUE;
}

void fcgi_packet_parse(fcgi_context *ctx, guint8 *p, guint8 *pe) {
	guint8 *eof = pe;
	GString tmp1, tmp2;
	UNUSED(eof);

	switch (ctx->FCGI_Record.type) {
	case FCGI_BEGIN_REQUEST: {
		guint role;
		guint8 flags;
		if (ctx->FCGI_Record.contentLength != 8) {
			g_print("wrong FCGI_BEGIN_REQUEST size from %s (%u): %u\n",
				ctx->from_server ? "server" : "client", ctx->con_id, (guint) ctx->FCGI_Record.contentLength);
			return;
		}
		role = (p[0] << 8) | p[1];
		flags = p[2];
		g_print("begin request from %s (%u): role: %s, flags: %s\n",
			ctx->from_server ? "server" : "client", ctx->con_id,
			fcgi_role2string(role),
			fcgi_flags2string(flags)
		);
		break;
	}
	case FCGI_ABORT_REQUEST: {
		if (ctx->FCGI_Record.contentLength) {
			g_print("wrong FCGI_ABORT_REQUEST size from %s (%u): %u\n",
				ctx->from_server ? "server" : "client", ctx->con_id, (guint) ctx->FCGI_Record.contentLength);
			return;
		}
		g_print("abort request from %s (%u)\n",
			ctx->from_server ? "server" : "client", ctx->con_id);
		break;
	}
	case FCGI_END_REQUEST: {
		guint appStatus;
		guint8 protocolStatus;
		if (ctx->FCGI_Record.contentLength != 8) {
			g_print("wrong FCGI_END_REQUEST size from %s (%u): %u\n",
				ctx->from_server ? "server" : "client", ctx->con_id, (guint) ctx->FCGI_Record.contentLength);
			return;
		}
		appStatus = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
		protocolStatus = p[4];
		g_print("end request from %s (%u): applicationStatus: %u, protocolStatus: %s\n",
			ctx->from_server ? "server" : "client", ctx->con_id,
			appStatus,
			fcgi_protocol_status2string(protocolStatus)
		);
		break;
	}
	case FCGI_PARAMS: {
		guint len1, len2;
		GString *s1, *s2;
		if (!ctx->FCGI_Record.contentLength) {
			g_print("params end from %s (%u)%s\n",
				ctx->from_server ? "server" : "client", ctx->con_id, ctx->s_params->len ? " (unexpected)" : "");
			return;
		}
		USE_STREAM(s_params, p, pe);
		while (get_key_value_pair_len(&p, pe, &len1, &len2)) {
			if (p + len1 + len2 > pe) {
				return;
			}
			s1 = g_string_escape(g_string_set_const(&tmp1, (gchar*) p, len1));
			s2 = g_string_escape(g_string_set_const(&tmp2, (gchar*) p+len1, len2));
			g_print("param from %s (%u): '%s' = '%s'\n",
				ctx->from_server ? "server" : "client", ctx->con_id,
				s1->str, s2->str);
			g_string_free(s1, TRUE);
			g_string_free(s2, TRUE);
			p += len1 + len2;
			EAT_STREAM(s_params, p);
		}
		break;
	}
	case FCGI_STDIN:
		if (!ctx->FCGI_Record.contentLength) {
			g_print("stdin closed from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		log_raw_split("stdin", ctx->from_server, ctx->con_id, g_string_set_const(&tmp1, (gchar*) p, pe - p));
		break;
	case FCGI_STDOUT:
		if (!ctx->FCGI_Record.contentLength) {
			g_print("stdout closed from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		log_raw_split("stdout", ctx->from_server, ctx->con_id, g_string_set_const(&tmp1, (gchar*) p, pe - p));
		break;
	case FCGI_STDERR:
		if (!ctx->FCGI_Record.contentLength) {
			g_print("stderr closed from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		log_raw_split("stderr", ctx->from_server, ctx->con_id, g_string_set_const(&tmp1, (gchar*) p, pe - p));
		break;
	case FCGI_DATA:
		if (!ctx->FCGI_Record.contentLength) {
			g_print("data closed from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		log_raw_split("data", ctx->from_server, ctx->con_id, g_string_set_const(&tmp1, (gchar*) p, pe - p));
		break;
	
	case FCGI_GET_VALUES: {
		guint len1, len2;
		GString *s1, *s2;
		if (!ctx->FCGI_Record.contentLength) {
			g_print("empty get values from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		while (get_key_value_pair_len(&p, pe, &len1, &len2)) {
			if (p + len1 + len2 > pe) {
				return;
			}
			s1 = g_string_escape(g_string_set_const(&tmp1, (gchar*) p, len1));
			s2 = g_string_escape(g_string_set_const(&tmp2, (gchar*) p+len1, len2));
			if (len2) {
				g_print("get values from %s (%u): '%s' = '%s'?\n",
					ctx->from_server ? "server" : "client", ctx->con_id,
					s1->str, s2->str);
			} else {
				g_print("get values from %s (%u): '%s'\n",
					ctx->from_server ? "server" : "client", ctx->con_id,
					s1->str);
			}
			g_string_free(s1, TRUE);
			g_string_free(s2, TRUE);
			p += len1 + len2;
		}
		if (p != pe) {
			g_print("unexpected end of get values from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
		}
		break;
	}
	
	case FCGI_GET_VALUES_RESULT: {
		guint len1, len2;
		GString *s1, *s2;
		if (!ctx->FCGI_Record.contentLength) {
			g_print("empty get values result from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
			return;
		}
		while (get_key_value_pair_len(&p, pe, &len1, &len2)) {
			if (p + len1 + len2 > pe) {
				return;
			}
			s1 = g_string_escape(g_string_set_const(&tmp1, (gchar*) p, len1));
			s2 = g_string_escape(g_string_set_const(&tmp2, (gchar*) p+len1, len2));
			g_print("get values result from %s (%u): '%s' = '%s'\n",
				ctx->from_server ? "server" : "client", ctx->con_id,
				s1->str, s2->str);
			g_string_free(s1, TRUE);
			g_string_free(s2, TRUE);
			p += len1 + len2;
		}
		if (p != pe) {
			g_print("unexpected end of get values result from %s (%u)\n",
				ctx->from_server ? "server" : "client", ctx->con_id);
		}
		break;
	}
	
	case FCGI_UNKNOWN_TYPE:
		if (ctx->FCGI_Record.contentLength != 8) {
			g_print("wrong FCGI_UNKNOWN_TYPE size from %s (%u): %u\n",
				ctx->from_server ? "server" : "client", ctx->con_id, (guint) ctx->FCGI_Record.contentLength);
			return;
		}
		g_print("unknown type %u from %s (%u)\n", (guint) p[0],
			ctx->from_server ? "server" : "client", ctx->con_id);
		break;
	
	default:
		g_print("packet from %s (%u): type: %s, id: 0x%x, contentLength: 0x%x\n",
			ctx->from_server ? "server" : "client", ctx->con_id,
			fcgi_type2string(ctx->FCGI_Record.type),
			(guint) ctx->FCGI_Record.requestID,
			(guint) ctx->FCGI_Record.contentLength
			);
		log_raw("packet data", ctx->from_server, ctx->con_id, g_string_set_const(&tmp1, (gchar*) p, pe - p));
		break;
	}
}

void fcgi_context_append(gpointer _ctx, const gchar* buf, gssize buflen) {
	fcgi_context *ctx = (fcgi_context*) _ctx;
	guint8* data;
	guint total_len;

	if (ctx->error) return;
	g_string_append_len(ctx->buffer, buf, buflen);

	for (;;) {
		data = (guint8*) ctx->buffer->str;
	
		if (ctx->buffer->len < FCGI_HEADER_LEN) return;
	
		ctx->FCGI_Record.version = data[0];
		ctx->FCGI_Record.type = data[1];
		ctx->FCGI_Record.requestID = (data[2] << 8) | (data[3]);
		ctx->FCGI_Record.contentLength = (data[4] << 8) | (data[5]);
		ctx->FCGI_Record.paddingLength = data[6];
		/* ignore data[7] */
	
		total_len = FCGI_HEADER_LEN + ctx->FCGI_Record.contentLength + ctx->FCGI_Record.paddingLength;
		if (ctx->buffer->len < total_len) return;
	
		fcgi_packet_parse(ctx, (guint8*) (FCGI_HEADER_LEN + ctx->buffer->str), (guint8*) (ctx->FCGI_Record.contentLength + FCGI_HEADER_LEN + ctx->buffer->str));
		g_string_erase(ctx->buffer, 0, total_len);
	}
}
