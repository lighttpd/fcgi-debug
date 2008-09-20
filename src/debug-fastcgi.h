#ifndef FCGI_DEBUG_DEBUG_FASTCGI_H
#define FCGI_DEBUG_DEBUG_FASTCGI_H

struct fcgi_context;
typedef struct fcgi_context fcgi_context;

#include "fcgi-debug.h"

struct fcgi_context {
	GString *buffer;
	GString *s_params;
	gboolean error;

	struct {
		guint8 version;
		guint8 type;
		guint16 requestID;
		guint16 contentLength;
		guint8 paddingLength;
	} FCGI_Record;

	gboolean from_server;
	guint con_id;
};

#define FCGI_HEADER_LEN  8

enum FCGI_Type {
	FCGI_BEGIN_REQUEST     = 1,
	FCGI_ABORT_REQUEST     = 2,
	FCGI_END_REQUEST       = 3,
	FCGI_PARAMS            = 4,
	FCGI_STDIN             = 5,
	FCGI_STDOUT            = 6,
	FCGI_STDERR            = 7,
	FCGI_DATA              = 8,
	FCGI_GET_VALUES        = 9,
	FCGI_GET_VALUES_RESULT = 10,
	FCGI_UNKNOWN_TYPE      = 11
};
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

enum FCGI_Flags {
	FCGI_KEEP_CONN  = 1
};

enum FCGI_Role {
	FCGI_RESPONDER  = 1,
	FCGI_AUTHORIZER = 2,
	FCGI_FILTER     = 3
};

enum FCGI_ProtocolStatus {
	FCGI_REQUEST_COMPLETE = 0,
	FCGI_CANT_MPX_CONN    = 1,
	FCGI_OVERLOADED       = 2,
	FCGI_UNKNOWN_ROLE     = 3
};

fcgi_context* fcgi_context_new(gboolean from_server, guint con_id);
void fcgi_context_free(gpointer _ctx);
void fcgi_context_append(gpointer _ctx, const gchar* buf, gssize buflen);

const char* fcgi_type2string(enum FCGI_Type val);
const char* fcgi_flags2string(guint8 flags);
const char* fcgi_role2string(enum FCGI_Role role);
const char* fcgi_protocol_status2string(enum FCGI_ProtocolStatus val);

#endif
