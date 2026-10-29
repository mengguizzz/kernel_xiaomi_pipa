/*
 * Minimal Android arm64 diagnostic helper for /dev/xiaomi-touch.
 * It forces Touch_Pen_ENABLE (mode 20) to -1, which asks the driver for
 * finger input and releases any stale pen-mode state.
 */

typedef unsigned long ulong;

static long syscall1(long nr, long arg0)
{
	register long x0 __asm__("x0") = arg0;
	register long x8 __asm__("x8") = nr;

	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
	return x0;
}

static long syscall3(long nr, long arg0, long arg1, long arg2)
{
	register long x0 __asm__("x0") = arg0;
	register long x1 __asm__("x1") = arg1;
	register long x2 __asm__("x2") = arg2;
	register long x8 __asm__("x8") = nr;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x1), "r"(x2), "r"(x8)
			 : "memory");
	return x0;
}

static int open_readonly(const char *path)
{
	/* openat(AT_FDCWD, path, O_RDONLY) */
	register long x0 __asm__("x0") = -100;
	register long x1 __asm__("x1") = (long)path;
	register long x2 __asm__("x2") = 0;
	register long x8 __asm__("x8") = 56;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x1), "r"(x2), "r"(x8)
			 : "memory");
	return x0;
}

void _start(void)
{
	static int buf[256];
	const char ok[] = "touchmode: finger requested\n";
	const char fail[] = "touchmode: ioctl failed\n";
	int fd;
	long ret;

	fd = open_readonly("/dev/xiaomi-touch");
	if (fd < 0)
		syscall1(93, 1);

	buf[0] = 20; /* Touch_Pen_ENABLE */
	buf[1] = -1;
	ret = syscall3(29, fd, 0, (long)buf); /* ioctl(fd, SET_CUR_VALUE, buf) */

	if (ret < 0) {
		syscall3(64, 1, (long)fail, sizeof(fail) - 1);
		syscall1(93, 1);
	}

	syscall3(64, 1, (long)ok, sizeof(ok) - 1);
	syscall1(93, 0);
}
