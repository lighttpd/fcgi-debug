#include "debug-fastcgi.h"

#define SWITCH_HANDLE(x) case x: return #x
const char* fcgi_type2string(enum FCGI_Type val) {
	switch (val) {
	SWITCH_HANDLE(FCGI_BEGIN_REQUEST);
	SWITCH_HANDLE(FCGI_ABORT_REQUEST);
	SWITCH_HANDLE(FCGI_END_REQUEST);
	SWITCH_HANDLE(FCGI_PARAMS);
	SWITCH_HANDLE(FCGI_STDIN);
	SWITCH_HANDLE(FCGI_STDOUT);
	SWITCH_HANDLE(FCGI_STDERR);
	SWITCH_HANDLE(FCGI_DATA);
	SWITCH_HANDLE(FCGI_GET_VALUES);
	SWITCH_HANDLE(FCGI_GET_VALUES_RESULT);
	SWITCH_HANDLE(FCGI_UNKNOWN_TYPE);
	default: return "<unknown>";
	}
}

const char* fcgi_flags2string(guint8 flags) {
	switch (flags) {
	case 0: return "none";
	case 1: return "FCGI_KEEP_CONN";
	default: return "<unknown>";
	}
}

const char* fcgi_role2string(enum FCGI_Role role) {
	switch (role) {
	SWITCH_HANDLE(FCGI_RESPONDER);
	SWITCH_HANDLE(FCGI_AUTHORIZER);
	SWITCH_HANDLE(FCGI_FILTER);
	default: return "<unknown>";
	}
}

const char* fcgi_protocol_status2string(enum FCGI_ProtocolStatus val) {
	switch (val) {
	SWITCH_HANDLE(FCGI_REQUEST_COMPLETE);
	SWITCH_HANDLE(FCGI_CANT_MPX_CONN);
	SWITCH_HANDLE(FCGI_OVERLOADED);
	SWITCH_HANDLE(FCGI_UNKNOWN_ROLE);
	default: return "<unknown>";
	}
}

void setup_debug_fastcgi(connection *con) {
	con->ctx_server = fcgi_context_new(TRUE, con->con_id);
	con->da_server = fcgi_context_append;
	con->df_server = fcgi_context_free;
	con->ctx_client = fcgi_context_new(FALSE, con->con_id);
	con->da_client = fcgi_context_append;
	con->df_client = fcgi_context_free;
}
