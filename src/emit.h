#ifndef EMIT_H__
#define EMIT_H__

#include "xlog.h"

extern void emit_xlog_fixheader(const struct xlog_fixheader *xhdr);
extern void emit_xlog_header(const struct xrow_header *hdr);
extern void emit_value(const char **pos, const char *end);
extern void emit_xlog_data(const char *pos, const char *end);
extern void emit_hr(void);

#endif /* EMIT_H__ */
