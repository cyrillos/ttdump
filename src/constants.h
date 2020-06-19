#ifndef CONSTANTS_H__
#define CONSTANTS_H__

#include <stdlib.h>
#include <stdint.h>

#include "msgpuck/msgpuck.h"

#define SIGNATURE_SNAP			"SNAP"
#define SIGNATURE_XLOG			"XLOG"
#define SIGNATURE_VY_XLOG		"VYLOG"
#define SIGNATURE_VY_RUN		"RUN"
#define SIGNATURE_VY_INDEX		"INDEX"

enum {
	XROW_HEADER_IOVMAX		= 1,
	XROW_BODY_IOVMAX		= 2,
	XROW_IOVMAX			= XROW_HEADER_IOVMAX + XROW_BODY_IOVMAX,
	XROW_HEADER_LEN_MAX		= 52,
	XROW_BODY_LEN_MAX		= 256,
	IPROTO_HEADER_LEN		= 28,

	/** 7 = sizeof(iproto_body_bin). */
	IPROTO_SELECT_HEADER_LEN	= IPROTO_HEADER_LEN + 7,
};

enum iproto_key {
	IPROTO_REQUEST_TYPE		= 0x00,
	IPROTO_SYNC			= 0x01,

	IPROTO_REPLICA_ID		= 0x02,
	IPROTO_LSN			= 0x03,
	IPROTO_TIMESTAMP		= 0x04,
	IPROTO_SCHEMA_VERSION		= 0x05,
	IPROTO_SERVER_VERSION		= 0x06,
	IPROTO_GROUP_ID			= 0x07,
	IPROTO_TSN			= 0x08,
	IPROTO_FLAGS			= 0x09,
	IPROTO_SPACE_ID			= 0x10,
	IPROTO_INDEX_ID			= 0x11,
	IPROTO_LIMIT			= 0x12,
	IPROTO_OFFSET			= 0x13,
	IPROTO_ITERATOR			= 0x14,
	IPROTO_INDEX_BASE		= 0x15,

	IPROTO_KEY			= 0x20,
	IPROTO_TUPLE			= 0x21,
	IPROTO_FUNCTION_NAME		= 0x22,
	IPROTO_USER_NAME		= 0x23,

	IPROTO_INSTANCE_UUID		= 0x24,
	IPROTO_CLUSTER_UUID		= 0x25,
	IPROTO_VCLOCK			= 0x26,

	IPROTO_EXPR			= 0x27,
	IPROTO_OPS			= 0x28,
	IPROTO_BALLOT			= 0x29,
	IPROTO_TUPLE_META		= 0x2a,
	IPROTO_OPTIONS			= 0x2b,

	IPROTO_DATA			= 0x30,
	IPROTO_ERROR_24			= 0x31,
	IPROTO_METADATA			= 0x32,
	IPROTO_BIND_METADATA		= 0x33,
	IPROTO_BIND_COUNT		= 0x34,

	IPROTO_SQL_TEXT			= 0x40,
	IPROTO_SQL_BIND			= 0x41,
	IPROTO_SQL_INFO			= 0x42,
	IPROTO_STMT_ID			= 0x43,

	IPROTO_REPLICA_ANON		= 0x50,
	IPROTO_ID_FILTER		= 0x51,
	IPROTO_ERROR			= 0x52,
	IPROTO_KEY_MAX
};

enum {
	/** Maximal iproto package body length (2GiB) */
	IPROTO_BODY_LEN_MAX = 2147483648UL,
	/* Maximal length of text handshake (greeting) */
	IPROTO_GREETING_SIZE = 128,
	/** marker + len + prev crc32 + cur crc32 + (padding) */
	XLOG_FIXHEADER_SIZE = 19
};

enum {
	/** Set for the last xrow in a transaction. */
	IPROTO_FLAG_COMMIT = 0x01,
};

/**
 * IPROTO command codes
 */
enum iproto_type {
	/** Acknowledgement that request or command is successful */
	IPROTO_OK		= 0,

	/** SELECT request */
	IPROTO_SELECT		= 1,
	/** INSERT request */
	IPROTO_INSERT		= 2,
	/** REPLACE request */
	IPROTO_REPLACE		= 3,
	/** UPDATE request */
	IPROTO_UPDATE		= 4,
	/** DELETE request */
	IPROTO_DELETE		= 5,
	/** CALL request - wraps result into [tuple, tuple, ...] format */
	IPROTO_CALL_16		= 6,
	/** AUTH request */
	IPROTO_AUTH		= 7,
	/** EVAL request */
	IPROTO_EVAL		= 8,
	/** UPSERT request */
	IPROTO_UPSERT		= 9,
	/** CALL request - returns arbitrary MessagePack */
	IPROTO_CALL		= 10,
	/** Execute an SQL statement. */
	IPROTO_EXECUTE		= 11,
	/** No operation. Treated as DML, used to bump LSN. */
	IPROTO_NOP		= 12,
	/** Prepare SQL statement. */
	IPROTO_PREPARE		= 13,
	/** The maximum typecode used for box.stat() */
	IPROTO_TYPE_STAT_MAX,

	/** PING request */
	IPROTO_PING		= 64,
	/** Replication JOIN command */
	IPROTO_JOIN		= 65,
	/** Replication SUBSCRIBE command */
	IPROTO_SUBSCRIBE	= 66,
	/** DEPRECATED: use IPROTO_VOTE instead */
	IPROTO_VOTE_DEPRECATED	= 67,
	/** Vote request command for master election */
	IPROTO_VOTE		= 68,
	/** Anonymous replication FETCH SNAPSHOT. */
	IPROTO_FETCH_SNAPSHOT	= 69,
	/** REGISTER request to leave anonymous replication. */
	IPROTO_REGISTER		= 70,

	/** Vinyl run info stored in .index file */
	VY_INDEX_RUN_INFO	= 100,
	/** Vinyl page info stored in .index file */
	VY_INDEX_PAGE_INFO	= 101,
	/** Vinyl row index stored in .run file */
	VY_RUN_ROW_INDEX	= 102,

	/** Non-final response type. */
	IPROTO_CHUNK		= 128,
	IPROTO_TYPE_MAX,

	/**
	 * Error codes = (IPROTO_TYPE_ERROR | ER_XXX from errcode.h)
	 */
	IPROTO_TYPE_ERROR = 1 << 15
};

extern const unsigned char iproto_key_type[IPROTO_KEY_MAX];
extern const char *iproto_type_strs[IPROTO_TYPE_MAX];
extern const uint64_t iproto_body_key_map[IPROTO_TYPE_STAT_MAX];
extern const char *iproto_key_strs[IPROTO_KEY_MAX];

#endif /* CONSTANTS_H__ */
