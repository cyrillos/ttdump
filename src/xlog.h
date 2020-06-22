#ifndef XLOG_H__
#define XLOG_H__

#include <stdint.h>
#include <stdbool.h>

#include <sys/uio.h>

#include <zstd.h>

#include "constants.h"

typedef uint32_t log_magic_t;

enum {
	XLOG_META_INSTANCE_UUID_KEY,
	XLOG_META_INSTANCE_INSTANCE_UUID_KEY_V12,
	XLOG_META_XLOG_META_VCLOCK_KEY,
	XLOG_META_VERSION_KEY,
	XLOG_META_PREV_VCLOCK_KEY,

	XLOG_META_MAX,
};

typedef struct {
	ZSTD_DCtx	*zdctx;

	char		meta_values[XLOG_META_MAX][128];

	const char	*path;
	const char	*data;
	const char	*end;
	size_t		size;

	const char	*meta;
	const char	*meta_end;
	int		file_type;
} xlog_ctx_t;

static inline void xlog_ctx_create(xlog_ctx_t *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->zdctx = ZSTD_createDCtx();
	ctx->file_type = WAL_TYPE_MAX;
}

static inline void xlog_ctx_destroy(xlog_ctx_t *ctx)
{
	if (ctx->zdctx)
		ZSTD_freeDCtx(ctx->zdctx);
}

struct xrow_header {
	uint32_t	type;
	uint32_t	replica_id;
	uint32_t	group_id;
	uint64_t	sync;
	int64_t		lsn;
	double		tm;
	int64_t		tsn;
	bool		is_commit;
	int		bodycnt;
	uint32_t	schema_version;
	struct iovec	body[XROW_BODY_IOVMAX];
};

struct xlog_fixheader {
	log_magic_t	magic;
	uint32_t	crc32p;
	uint32_t	crc32c;
	uint32_t	len;
};

extern int parse_file(xlog_ctx_t *ctx);

#endif /* XLOG_H__ */
