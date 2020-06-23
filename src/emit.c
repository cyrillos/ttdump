#include "compiler.h"
#include "constants.h"
#include "xlog.h"
#include "log.h"

#include "msgpuck/msgpuck.h"

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
		mp_type_str[type] : "UNKNOWN";
}

static const char *pr_iproto_type(unsigned int type)
{
	return type < IPROTO_TYPE_MAX ?
		iproto_type_strs[type] : "UNKNOWN";
}

void emit_hr(void)
{
	pr_info("-------\n");
}

void emit_xlog_fixheader(const struct xlog_fixheader *xhdr)
{
	pr_info("fixed header\n");
	emit_hr();
	pr_info("  magic %#8x crc32p %#x crc32c %#x len %d\n",
		xhdr->magic, xhdr->crc32p, xhdr->crc32c, xhdr->len);
	emit_hr();
}

void emit_xlog_header(const struct xrow_header *hdr)
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

void emit_value(const char **pos, const char *end)
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

void emit_xlog_data(const char *pos, const char *end)
{
	if (mp_typeof(pos[0]) != MP_MAP) {
		pr_err("map expected but got %d\n", mp_typeof(pos[0]));
		return;
	}

	uint32_t size = mp_decode_map(&pos);
	for (uint32_t i = 0; i < size; i++) {
		if (mp_typeof(*pos) != MP_UINT) {
			pr_err("MP_UINT expected but got %d\n", mp_typeof(*pos));
			return;
		}

		uint64_t key = mp_decode_uint(&pos);
		if (key >= IPROTO_KEY_MAX ||
		    iproto_key_type[key] != mp_typeof(*pos)) {
			pr_err("unknown key %#llx\n", key);
			return;
		}

		pr_info("key: %#llx '%s' ", key, iproto_key_strs[key]);
		pr_info("value: ");
		emit_value(&pos, end);
		pr_info("\n");
	}
}
