/* net-virtio-lwip acceptance guest: POSIX sockets via libposix_socket
 * over lib-lwip (nolibc headers). Slirp gives the lease, then a TCP
 * connect to the user-net host 10.0.2.2:5555 where accept.sh runs a
 * listener. Every stage prints to serial — the log IS the evidence. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void)
{
	int s, r;
	struct sockaddr_in a;
	char buf[64];
	const char *msg = "guest-says-hi\n";

	printf("netlwip: init done\n");

	s = socket(AF_INET, SOCK_STREAM, 0);
	printf("netlwip: socket rc=%d errno=%d\n", s, errno);
	if (s < 0)
		return 1;

	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(5555);
	a.sin_addr.s_addr = htonl(0x0A000202u); /* 10.0.2.2 */

	for (int i = 0; i < 10; i++) {
		r = connect(s, (struct sockaddr *)&a, sizeof a);
		printf("netlwip: connect try=%d rc=%d errno=%d\n", i, r, errno);
		if (r == 0)
			break;
		sleep(1);
		s = socket(AF_INET, SOCK_STREAM, 0);
	}
	if (r < 0)
		return 2;

	{
		struct sockaddr_in la;
		socklen_t ll = sizeof la;
		getsockname(s, (struct sockaddr *)&la, &ll);
		printf("netlwip: local %d.%d.%d.%d:%d\n",
		       (la.sin_addr.s_addr >> 0) & 0xff,
		       (la.sin_addr.s_addr >> 8) & 0xff,
		       (la.sin_addr.s_addr >> 16) & 0xff,
		       (la.sin_addr.s_addr >> 24) & 0xff,
		       ntohs(la.sin_port));
	}

	send(s, msg, strlen(msg), 0);
	r = recv(s, buf, sizeof buf, 0);
	printf("netlwip: recv rc=%d\n", r);
	close(s);
	return 0;
}
