#ifndef LOAD_H__
#define LOAD_H__

#include <stdint.h>
#include <stdbool.h>

#define __packed  __attribute__((packed))

#define bit_likely(x)    __builtin_expect((x),1)
#define bit_unlikely(x)  __builtin_expect((x),0)

struct __packed unaligned_mem {
	union {
		uint8_t		u8;
		uint16_t	u16;
		uint32_t	u32;
		uint64_t	u64;
		float		f;
		double		lf;
		bool		b;
	};
};

/**
 * @brief Unaligned load from memory.
 * @param p pointer
 * @return number
 */
static inline uint8_t
load_u8(const void *p)
{
	return ((const struct unaligned_mem *)p)->u8;
}

/** @copydoc load_u8 */
static inline uint16_t
load_u16(const void *p)
{
	return ((const struct unaligned_mem *)p)->u16;
}

/** @copydoc load_u8 */
static inline uint32_t
load_u32(const void *p)
{
	return ((const struct unaligned_mem *)p)->u32;
}

/** @copydoc load_u8 */
static inline uint64_t
load_u64(const void *p)
{
	return ((const struct unaligned_mem *)p)->u64;
}

/** @copydoc load_u8 */
static inline float
load_float(const void *p)
{
	return ((const struct unaligned_mem *)p)->f;
}

/** @copydoc load_u8 */
static inline double
load_double(const void *p)
{
	return ((const struct unaligned_mem *)p)->lf;
}

/** @copydoc load_u8 */
static inline bool
load_bool(const void *p)
{
	return ((const struct unaligned_mem *)p)->b;
}

/**
 * @brief Unaligned store to memory.
 * @param p pointer
 * @param v number
 */
static inline void
store_u8(void *p, uint8_t v)
{
	((struct unaligned_mem *)p)->u8 = v;
}

/** @copydoc store_u8 */
static inline void
store_u16(void *p, uint16_t v)
{
	((struct unaligned_mem *)p)->u16 = v;
}

/** @copydoc store_u8 */
static inline void
store_u32(void *p, uint32_t v)
{
	((struct unaligned_mem *)p)->u32 = v;
}

/** @copydoc store_u8 */
static inline void
store_u64(void *p, uint64_t v)
{
	((struct unaligned_mem *)p)->u64 = v;
}

/** @copydoc store_u8 */
static inline void
store_float(void *p, float v)
{
	((struct unaligned_mem *)p)->f = v;
}

/** @copydoc store_u8 */
static inline void
store_double(void *p, double v)
{
	((struct unaligned_mem *)p)->lf = v;
}

/** @copydoc store_bool */
static inline void
store_bool(void *p, bool b)
{
	((struct unaligned_mem *)p)->b = b;
}

#endif /* LOAD_H__ */
