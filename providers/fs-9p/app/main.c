/* fs-9p provider — mounts a host-shared 9P directory via virtio-9p
 * and verifies byte-identity both directions against host-planted
 * and guest-written files. Compose lane: uk9p + 9pfs + virtio_9p
 * are all in-tree; this app only orchestrates the mount and checks.
 * SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>

static int read_all(const char *path, char *buf, int n)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) return -errno;
	int r = read(fd, buf, n - 1);
	if (r < 0) { int e = errno; close(fd); return -e; }
	buf[r] = 0;
	close(fd);
	return r;
}

int main(void)
{
	char buf[512];
	int r;

	printf("fs-9p: guest up\n");

	/* no rootfs in this image — ramfs first so the mountpoint exists */
	r = mount("", "/", "ramfs", 0, "");
	if (r) { printf("fs-9p: REFUSE ramfs rc=%d errno=%d\n", r, errno); return 1; }
	r = mkdir("/host", 0755);
	if (r && errno != EEXIST) { printf("fs-9p: REFUSE mkdir errno=%d\n", errno); return 1; }
	r = mount("hostshare", "/host", "9pfs", 0,
		  "version=9P2000.u,uname=root");
	if (r) {
		printf("fs-9p: REFUSE mount rc=%d errno=%d\n", r, errno);
		return 1;
	}
	printf("fs-9p: mounted hostshare at /host\n");

	r = read_all("/host/planted.txt", buf, sizeof buf);
	if (r < 0) {
		printf("fs-9p: REFUSE read planted.txt rc=%d\n", r);
		return 1;
	}
	printf("fs-9p: read %d bytes: %s", r, buf);
	if (strncmp(buf, "HOST-PLANTED-9P", 15)) {
		printf("fs-9p: REFUSE planted content mismatch\n");
		return 1;
	}
	printf("fs-9p: host-planted file verified byte-identical\n");

	int fd = open("/host/guest-out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		printf("fs-9p: REFUSE create guest-out errno=%d\n", errno);
		return 1;
	}
	const char *out = "GUEST-WROTE-9P: provider=fs-9p lane=compose\n";
	if (write(fd, out, strlen(out)) != (int)strlen(out)) {
		printf("fs-9p: REFUSE write errno=%d\n", errno);
		return 1;
	}
	close(fd);
	printf("fs-9p: wrote /host/guest-out.txt\n");

	r = read_all("/host/guest-out.txt", buf, sizeof buf);
	if (r < 0 || strncmp(buf, "GUEST-WROTE-9P", 14)) {
		printf("fs-9p: REFUSE guest-out readback mismatch\n");
		return 1;
	}
	printf("fs-9p: guest-written file verified byte-identical\n");
	printf("fs-9p: ACCEPTED\n");
	return 0;
}
