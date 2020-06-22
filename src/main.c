#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "xlog.h"

int main(int argc, char *argv[])
{
	xlog_ctx_t ctx;

	if (argc < 2) {
		pr_err("Provide path\n");
		return 1;
	}

	int fd = open(argv[1], O_RDWR);
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

	void *addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		pr_perror("Can't mmap %s", argv[1]);
		close(fd);
		return 1;
	}

	xlog_ctx_create(&ctx);

	ctx.path = argv[1];
	ctx.data = addr;
	ctx.end = addr + st.st_size;
	ctx.meta = addr;
	ctx.size = st.st_size;

	int ret = parse_file(&ctx);
	munmap(addr, st.st_size);
	close(fd);

	xlog_ctx_destroy(&ctx);
	return ret;
}
