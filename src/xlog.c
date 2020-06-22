#include <string.h>
#include <zstd.h>

#include "xlog.h"
#include "load.h"
#include "emit.h"
#include "log.h"

static char *wal_signatures[] = {
	[WAL_TYPE_SNAP]		= "SNAP",
	[WAL_TYPE_XLOG]		= "XLOG",
	[WAL_TYPE_VY_XLOG]	= "VYLOG",
	[WAL_TYPE_VY_RUN]	= "RUN",
	[WAL_TYPE_VY_INDEX]	= "INDEX",
};

static const log_magic_t row_marker = mp_bswap_u32(0xd5ba0bab);
static const log_magic_t zrow_marker = mp_bswap_u32(0xd5ba0bba);
static const log_magic_t eof_marker = mp_bswap_u32(0xd510aded);

int xrow_header_decode(struct xrow_header *header, const char **pos,
		       const char *end, bool end_is_exact)
{
	memset(header, 0, sizeof(struct xrow_header));

	const char * const start = *pos;
	const char *tmp = *pos;

	if (mp_check(&tmp, end) != 0) {
error:
		pr_err("packet header\n");
		return -1;
	}

	if (mp_typeof(**pos) != MP_MAP)
		goto error;

	bool has_tsn = false;
	uint32_t flags = 0;

	uint32_t size = mp_decode_map(pos);
	for (uint32_t i = 0; i < size; i++) {
		if (mp_typeof(**pos) != MP_UINT)
			goto error;

		uint64_t key = mp_decode_uint(pos);
		if (key >= IPROTO_KEY_MAX ||
		    iproto_key_type[key] != mp_typeof(**pos))
			goto error;

		switch (key) {
		case IPROTO_REQUEST_TYPE:
			header->type = mp_decode_uint(pos);
			break;
		case IPROTO_SYNC:
			header->sync = mp_decode_uint(pos);
			break;
		case IPROTO_REPLICA_ID:
			header->replica_id = mp_decode_uint(pos);
			break;
		case IPROTO_GROUP_ID:
			header->group_id = mp_decode_uint(pos);
			break;
		case IPROTO_LSN:
			header->lsn = mp_decode_uint(pos);
			break;
		case IPROTO_TIMESTAMP:
			header->tm = mp_decode_double(pos);
			break;
		case IPROTO_SCHEMA_VERSION:
			header->schema_version = mp_decode_uint(pos);
			break;
		case IPROTO_TSN:
			has_tsn = true;
			header->tsn = mp_decode_uint(pos);
			break;
		case IPROTO_FLAGS:
			flags = mp_decode_uint(pos);
			header->is_commit = flags & IPROTO_FLAG_COMMIT;
			break;
		default:
			/* unknown header */
			pr_info("unknown key %lld\n", (long long)key);
			mp_next(pos);
		}
	}

	assert(*pos <= end);

	if (!has_tsn) {
		/*
		 * Transaction id is not set so it is a single statement
		 * transaction.
		 */
		header->is_commit = true;
	}

	/* Restore transaction id from lsn and transaction serial number. */
	header->tsn = header->lsn - header->tsn;

	/* Nop requests aren't supposed to have a body. */
	if (*pos < end && header->type != IPROTO_NOP) {
		const char *body = *pos;
		if (mp_check(pos, end)) {
			pr_err("packet header\n");
			return -1;
		}

		header->bodycnt = 1;
		header->body[0].iov_base = (void *)body;
		header->body[0].iov_len = *pos - body;
	}

	if (end_is_exact && *pos < end) {
		pr_err("packet header\n");
		return -1;
	}

	return 0;
}

static int parse_fixheader(struct xlog_fixheader *xhdr,
			   const char **data, size_t *size)
{
	const char *pos = *data;
	const char *end = pos + XLOG_FIXHEADER_SIZE;

	memset(xhdr, 0, sizeof(*xhdr));

	if (*size < sizeof(xhdr->magic)) {
		pr_err("fixheader: size is too small %zd (need %zd)\n",
		       *size, sizeof(xhdr->magic));
		return -1;
	}

	xhdr->magic = load_u32(pos);
	if (xhdr->magic != row_marker &&
	    xhdr->magic != zrow_marker &&
	    xhdr->magic != eof_marker) {
		pr_err("fixheader: invalid magic: 0x%x\n", xhdr->magic);
		return -1;
	}

	if (xhdr->magic == eof_marker) {
		*size -= sizeof(xhdr->magic);
		*data += sizeof(xhdr->magic);
		return 0;
	}

	if (*size < XLOG_FIXHEADER_SIZE) {
		pr_err("fixheader: size is too small %zd (need %zd)\n",
		       *size, sizeof(*xhdr));
		return -1;
	}

	pos += sizeof(xhdr->magic);
	const char *val = pos;
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("fixheader: broken length\n");
		return -1;
	}

	xhdr->len = mp_decode_uint(&val);
	assert(val == pos);
	if (xhdr->len > IPROTO_BODY_LEN_MAX) {
		pr_err("fixheader: too large length (%zd while max %zd)\n",
		       xhdr->len, IPROTO_BODY_LEN_MAX);
		return -1;
	}

	/* Read previous crc32 */
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("fixheader: broken crc32p\n");
		return -1;
	}
	xhdr->crc32p = mp_decode_uint(&val);
	assert(val == pos);

	/* Read current crc32 */
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("fixheader: broken crc32c\n");
		return -1;
	}
	xhdr->crc32c = mp_decode_uint(&val);
	assert(val == pos);

	/* Check and skip padding if any */
	if (pos < end && (mp_check(&pos, end) != 0 || pos != end)) {
		pr_err("fixheader: broken padding\n");
		return -1;
	}

	assert(pos == end);
	*size -= (end - *data);
	*data = end;
	return 0;
}

static ssize_t decompress(ZSTD_DCtx *zdctx, char *dst, ssize_t dst_size,
			  const char *src, ssize_t src_size)
{
//	pr_info("decomp %zd\n", ZSTD_estimateDStreamSize_fromFrame(src, src_size));
	ZSTD_inBuffer input = {
		.src	= src,
		.size	= src_size,
		.pos	= 0,
	};

	ZSTD_outBuffer output = {
		.dst	= dst,
		.size	= dst_size,
		.pos	= 0,
	};

	while (input.pos < input.size && output.pos < output.size) {
		size_t rc = ZSTD_decompressStream(zdctx, &output, &input);
		if (ZSTD_isError(rc)) {
			pr_err("zstd: decompression failed %s\n", ZSTD_getErrorName(rc));
			return -1;
		}
	}
	return output.pos;
}

static int parse_data(xlog_ctx_t *ctx)
{
	struct xlog_fixheader xhdr;
	struct xrow_header hdr;

	const char *pos = ctx->meta_end;
	const char *rows, *rows_end;
	static char buf[IPROTO_BODY_LEN_MAX];

	int rc = -1;

	while (pos < ctx->end) {
		size_t size = ctx->end - pos;

		if (parse_fixheader(&xhdr, &pos, &size))
			return -1;

		emit_xlog_fixheader(&xhdr);

		if (xhdr.magic == zrow_marker) {
			ssize_t len = decompress(ctx->zdctx,
						 buf, sizeof(buf),
						 pos, xhdr.len);
			if (len < 0)
				return -1;
			rows = buf;
			rows_end = buf + len;
		} else if (xhdr.magic == row_marker) {
			rows = pos;
			rows_end = pos + xhdr.len;
		} else {
			assert(xhdr.magic == eof_marker);
			break;
		}

		do {
			if (xrow_header_decode(&hdr, (const char **)&rows, rows_end, false))
				return -1;

			emit_xlog_header(&hdr);
			for (size_t i = 0; i < hdr.bodycnt; i++) {
				emit_xlog_data(hdr.body[0].iov_base,
					       hdr.body[0].iov_base + hdr.body[0].iov_len);
			}
		} while (rows < rows_end);
		emit_hr();

		pos = rows_end;
	}

	return 0;
}

static const char *get_meta_end(const char *addr, size_t size)
{
	const char *end = memmem(addr, size, "\n\n", 2);
	if (end == NULL) {
		pr_err("No meta end found\n");
		return NULL;
	}
	return end + 2;
}

static int parse_meta(const char *data, const char *end)
{
	ssize_t size = end - data - 2;
	char *copy = malloc(size+1);

	assert(size > 0);

	if (!copy) {
		pr_perror("Can't allocate meta");
		return -1;
	}

	memcpy(copy, data, size);
	copy[size] = '\0';

	for (char *tok = strtok(copy, "\n");
	     tok; tok = strtok(NULL, "\n")) {
		pr_info("meta: '%s'\n", tok);
	}

	free(copy);
	return 0;
}

int parse_file(xlog_ctx_t *ctx)
{
	if (ctx->size < sizeof(log_magic_t)) {
		pr_err("The size is too small %zd\n", ctx->size);
		return -1;
	}

	for (int i = 0; i < (int)ARRAY_SIZE(wal_signatures); i++) {
		int slen = strlen(wal_signatures[i]);
		if (!strncmp(ctx->data, wal_signatures[i], slen)) {
			ctx->file_type = i;
			break;
		}
	}

	if (ctx->file_type == WAL_TYPE_MAX) {
		pr_err("Signature mismatch\n");
		return -1;
	}

	ctx->meta_end = get_meta_end(ctx->meta, ctx->size);
	if (!ctx->meta_end)
		return -1;
	if (ctx->meta_end >= ctx->end) {
		pr_err("No data without marker\n");
		return -1;
	}

	if (parse_meta(ctx->data, ctx->end))
		return -1;

	return parse_data(ctx);
}
