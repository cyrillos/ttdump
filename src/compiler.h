#ifndef COMPILER_H__
#define COMPILER_H__

#define __packed  __attribute__((packed))

#define ARRAY_SIZE(a)		(sizeof((a)) / sizeof((a)[0]))

#define stringify_1(__x)	#__x
#define stringify(__x)		stringify_1(__x)

#define stringify_item(__item)	\
	[__item]	= stringify_1(__item)

#endif /* COMPILER_H__ */
