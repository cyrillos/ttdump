#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>

#define pr_info(fmt, ...)	fprintf(stdout, fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)	fprintf(stderr, "Error(%s:%d): " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_perror(fmt, ...)	fprintf(stderr, "Error(%s:%d): " fmt ": %m\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif /* LOG_H__ */
