#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>

#include <zstd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "load.h"
#include "constants.h"
#include "msgpuck/msgpuck.h"

#define ARRAY_SIZE(a)		(sizeof((a)) / sizeof((a)[0]))

#define pr_info(fmt, ...)	fprintf(stdout, fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)	fprintf(stderr, "Error(%s:%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_perror(fmt, ...)	fprintf(stderr, "Error(%s:%d): " fmt ": %m\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define stringify_item(__item)	\
	[__item]	= #__item

typedef uint32_t log_magic_t;

const char *mp_type_str[] = {
	stringify_item(MP_NIL),
	stringify_item(MP_UINT),
	stringify_item(MP_INT),
	stringify_item(MP_STR),
	stringify_item(MP_BIN),
	stringify_item(MP_ARRAY),
	stringify_item(MP_MAP),
	stringify_item(MP_BOOL),
	stringify_item(MP_FLOAT),
	stringify_item(MP_DOUBLE),
	stringify_item(MP_EXT),
};

static const char *pr_mp_type(unsigned int type)
{
	return type < ARRAY_SIZE(mp_type_str) ?
		mp_type_str[type] : "NO";
}

static const char *pr_iproto_type(unsigned int type)
{
	return type < IPROTO_TYPE_MAX ?
		iproto_type_strs[type] : "NO";
}

static const log_magic_t row_marker = mp_bswap_u32(0xd5ba0bab); /* host byte order */
static const log_magic_t zrow_marker = mp_bswap_u32(0xd5ba0bba); /* host byte order */
static const log_magic_t eof_marker = mp_bswap_u32(0xd510aded); /* host byte order */

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
			pr_info("unknown\n");
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

static void emit_hr(void)
{
	pr_info("-------\n");
}

static void emit_xlog_fixheader(const struct xlog_fixheader *xhdr)
{
	pr_info("fixed header\n");
	emit_hr();
	pr_info("  magic %#8x crc32p %#x crc32c %#x len %d\n",
		xhdr->magic, xhdr->crc32p, xhdr->crc32c, xhdr->len);
	emit_hr();
}

static void emit_xlog_header(const struct xrow_header *hdr)
{
	pr_info("xrow header\n");
	emit_hr();
	pr_info("  type %#x (%s) replica_id %#x group_id %#x "
		"sync %lld lsn %lld tm %3.4g tsn %lld is_commit %d "
		"bodycnt %d schema_version %#x\n",
		hdr->type, pr_iproto_type(hdr->type),
		hdr->replica_id,
		hdr->group_id,
		hdr->sync,
		hdr->lsn,
		hdr->tm,
		hdr->tsn,
		hdr->is_commit,
		hdr->bodycnt);
	for (size_t i = 0; i < hdr->bodycnt; i++)
		pr_info("    iov: len %zu\n", hdr->body[i].iov_len);
	emit_hr();
}

static void emit_value(const char **pos, const char *end)
{
	char buf[4096];
	int type = mp_typeof(**pos);
	switch (type) {
	case MP_NIL:
		pr_info("nil");
		mp_decode_nil(pos);
		break;
	case MP_UINT:
		pr_info("%#llu", mp_decode_uint(pos));
		break;
	case MP_INT:
		pr_info("%#lld", mp_decode_int(pos));
		break;
	case MP_STR: {
		uint32_t len;
		const char *str = mp_decode_str(pos, &len);
		if (len >= sizeof(buf))
			len = sizeof(buf)-1;
		memcpy(buf, str, len);
		buf[len] = '\0';
		pr_info("%s", buf);
		break;
	}
	case MP_BIN: {
		uint32_t len;
		const char *str = mp_decode_bin(pos, &len);
		pr_info("%*s", len, str);
		break;
	}
	case MP_ARRAY: {
		uint32_t size = mp_decode_array(pos);
		pr_info("{");
		for (size_t i = 0; i < size; i++) {
			emit_value(pos, end);
			pr_info("%s", i < size-1 ? ", " : "");
		}
		pr_info("}");
		break;
	}
	case MP_MAP: {
		uint32_t size = mp_decode_map(pos);
		pr_info("{");
		for (size_t i = 0; i < size; i++) {
			emit_value(pos, end);
			pr_info(": ");
			emit_value(pos, end);
			pr_info("%s", i < size-1 ? ", " : "");
		}
		pr_info("}");
		break;
	}
	case MP_BOOL:
		pr_info("%s", mp_decode_bool(pos) ? "true" : "false");
		break;
	case MP_FLOAT:
		pr_info("%g", mp_decode_float(pos));
		break;
	case MP_DOUBLE:
		pr_info("%g", mp_decode_double(pos));
		break;
	case MP_EXT:
		pr_info("ext");
		mp_next(pos);
		break;
	default:
		mp_next(pos);
	}
}

static void pr_xlog_data(const char *pos, const char *end)
{
	if (mp_typeof(pos[0]) != MP_MAP) {
		pr_err("map expected but got %d\n", mp_typeof(pos[0]));
		return;
	}

	uint32_t size = mp_decode_map(&pos);
	for (uint32_t i = 0; i < size; i++) {
		if (mp_typeof(*pos) != MP_UINT) {
			pr_err("MP_UINT expected\n");
			return;
		}

		uint64_t key = mp_decode_uint(&pos);
		if (key >= IPROTO_KEY_MAX ||
		    iproto_key_type[key] != mp_typeof(*pos)) {
			pr_err("unknown key\n");
			return;
		}

		pr_info("key: %#llx '%s' ", key, iproto_key_strs[key]);

		switch (key) {
		case IPROTO_SPACE_ID:
			if (mp_typeof(*pos) != MP_UINT) {
				pr_err("MP_UINT expected\n");
				return;
			}
			pr_info("value: ");
			emit_value(&pos, end);
			break;
		case IPROTO_REPLICA_ID:
			if (mp_typeof(*pos) != MP_ARRAY) {
				pr_err("MP_ARRAY expected\n");
				return;
			}
			pr_info("value: ");
			emit_value(&pos, end);
			break;
		case IPROTO_TUPLE:
			if (mp_typeof(*pos) != MP_ARRAY) {
				pr_err("MP_ARRAY expected\n");
				return;
			}
			pr_info("value: ");
			emit_value(&pos, end);
			break;
		default:
			pr_info("default");
			mp_next(&pos);
			break;
		}
		pr_info("\n");
	}
}

static int parse_fixheader(struct xlog_fixheader *xhdr,
			   const char **data, size_t *size)
{
	const char *pos = *data;
	const char *end = pos + XLOG_FIXHEADER_SIZE;

	memset(xhdr, 0, sizeof(*xhdr));

	if (*size < sizeof(xhdr->magic)) {
		pr_err("size is too small %zd (need %zd)\n",
		       *size, sizeof(xhdr->magic));
		return -1;
	}

	xhdr->magic = load_u32(pos);
	if (xhdr->magic != row_marker &&
	    xhdr->magic != zrow_marker &&
	    xhdr->magic != eof_marker) {
		pr_err("invalid magic: 0x%x\n", xhdr->magic);
		return -1;
	}

	if (xhdr->magic == eof_marker) {
		*size -= sizeof(xhdr->magic);
		*data += sizeof(xhdr->magic);
		return 0;
	}

	if (*size < XLOG_FIXHEADER_SIZE) {
		pr_err("size is too small %zd (need %zd)\n",
		       *size, sizeof(*xhdr));
		return -1;
	}

	pos += sizeof(xhdr->magic);
	const char *val = pos;
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("broken fixheader length\n");
		return -1;
	}

	xhdr->len = mp_decode_uint(&val);
	assert(val == pos);
	if (xhdr->len > IPROTO_BODY_LEN_MAX) {
		pr_err("too large fixheader length\n");
		return -1;
	}

	/* Read previous crc32 */
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("broken fixheader crc32p\n");
		return -1;
	}
	xhdr->crc32p = mp_decode_uint(&val);
	assert(val == pos);

	/* Read current crc32 */
	if (pos >= end || mp_check(&pos, end) != 0 ||
	    mp_typeof(*val) != MP_UINT) {
		pr_err("broken fixheader crc32c\n");
		return -1;
	}
	xhdr->crc32c = mp_decode_uint(&val);
	assert(val == pos);

	/* Check and skip padding if any */
	if (pos < end && (mp_check(&pos, end) != 0 || pos != end)) {
		pr_err("broken fixheader padding\n");
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
			pr_err("decompression failed %s\n", ZSTD_getErrorName(rc));
			return -1;
		}
	}
	return output.pos;
}

static int parse_data(const char *data, const char *end)
{
	ZSTD_DCtx *zdctx = ZSTD_createDCtx();
	struct xlog_fixheader xhdr;
	struct xrow_header hdr;

	const char *pos = data;
	const char *rows, *rows_end;
	char buf[2 << 20];

	int rc = -1;

	while (pos < end) {
		size_t size = end - pos;

		if (parse_fixheader(&xhdr, &pos, &size))
			goto err;

		emit_xlog_fixheader(&xhdr);

		if (xhdr.magic == zrow_marker) {
			ssize_t len = decompress(zdctx,
						 buf, sizeof(buf),
						 pos, xhdr.len);
			if (len < 0)
				goto err;
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
				goto err;

			emit_xlog_header(&hdr);
			for (size_t i = 0; i < hdr.bodycnt; i++) {
				pr_xlog_data(hdr.body[0].iov_base,
					     hdr.body[0].iov_base + hdr.body[0].iov_len);
			}
		} while (rows < rows_end);
		emit_hr();

		pos = rows_end;
	}
	rc = 0;

err:
	ZSTD_freeDCtx(zdctx);
	return rc;
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

static int parse_file(const char *data, size_t size)
{
	const char *data_end = data + size;
	if (size < 4) {
		printf("The file size is too small %zd\n", size);
		return -1;
	}

	if (strncmp(data, SIGNATURE_SNAP, strlen(SIGNATURE_SNAP)) &&
	    strncmp(data, SIGNATURE_XLOG, strlen(SIGNATURE_XLOG)) &&
	    strncmp(data, SIGNATURE_VY_XLOG, strlen(SIGNATURE_VY_XLOG)) &&
	    strncmp(data, SIGNATURE_VY_RUN, strlen(SIGNATURE_VY_RUN)) &&
	    strncmp(data, SIGNATURE_VY_INDEX, strlen(SIGNATURE_VY_INDEX))) {
		pr_err("Signature mismatch\n");
		return -1;
	}

	const char *meta_end = get_meta_end(data, size);
	if (!meta_end)
		return -1;
	if (meta_end >= data_end) {
		pr_err("No data without marker\n");
		return -1;
	}

	if (parse_meta(data, meta_end))
		return -1;

	return parse_data(meta_end, data_end);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		pr_err("Provide path\n");
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		pr_perror("Can't open %s", argv[1]);
		return 1;
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		pr_perror("Can't stat %s", argv[1]);
		close(fd);
		return 1;
	}

	void *addr = mmap(NULL, st.st_size, PROT_READ,
			  MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		pr_perror("Can't mmap %s", argv[1]);
		close(fd);
		return 1;
	}
	close(fd);

	return parse_file(addr, st.st_size);
}
