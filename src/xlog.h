#ifndef XLOG_H__
#define XLOG_H__

#include <stdint.h>
#include <stdbool.h>

#include <sys/uio.h>

#include "constants.h"

typedef uint32_t log_magic_t;

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

extern int parse_file(const char *data, size_t size);

#endif /* XLOG_H__ */
