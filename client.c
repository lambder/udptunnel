#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <utun/socket.h>
#include <utun/tun.h>
#include <utun/util.h>
#include <utun/tunnel_packet.h>

#define PASSPHRASE v[3]

int main(int c, char **v)
{
	struct sockaddr_in server_addr, from;
	socklen_t fromlen;
	fd_set rfds;
	struct tunnel_packet *tp;
	char buf[MTU];
	unsigned int buflen;
	int ret;
	int tun_fd;
	int server_fd;

	tp = (struct tunnel_packet *)buf;

	if(c < 4) {
		printf("TCP/UDP/ICMP Tunnel over UDP\n"
			"%s <ip address> <port> <passphrase>\n", v[0]);
		return 0;
	}

	server_addr.sin_addr.s_addr = inet_addr(v[1]);
	if(strtoport(v[2], &server_addr.sin_port) == 0) {
		printf("%s: Invalid port\n", v[0]);
		return 1;
	}
	server_addr.sin_family = AF_INET;
	tun_fd = tun_create();
	server_fd = socket_create(0);

	/* Sending authentication */
	tp->type = CONTROL_PACKET;
	tp->cmd = AUTH_CMD;
	strcpy(tp->data, PASSPHRASE);
	socket_put_packet(server_fd, &server_addr, sizeof(server_addr), buf, sizeof(struct tunnel_packet) + strlen(PASSPHRASE));

	/* Waiting acknowledge */
	fromlen = sizeof(struct sockaddr_in);
	buflen = socket_get_packet(server_fd, &from, &fromlen, buf, sizeof(buf));
	if(tp->type == CONTROL_PACKET) {
		if(tp->cmd != OK_CMD) {
			puts("Password is incorret");
			return 1;
		}
	} else {
		puts("Unexpected packet was received.");
		return 1;
	}

#ifdef __linux__
	exec_script("linux_client.sh", v[1]);
#else
	exec_script("osx_client.sh", v[1]);
#endif
	puts("+ Auth is OK.\n+ UDP Tunnel is running.");
	FD_ZERO(&rfds);
	while(1) {
		FD_SET(server_fd, &rfds);
		FD_SET(tun_fd, &rfds);
		ret = select(server_fd+1, &rfds, NULL, NULL, NULL);

		if(ret == -1)
			break;

		if(FD_ISSET(tun_fd, &rfds)) {
			buflen = tun_get_packet(tun_fd, tp->data, sizeof(buf)-sizeof(struct tunnel_packet));
			tp->type = TRAFFIC_PACKET;
			tp->cmd = 0;
			socket_put_packet(server_fd, &server_addr, sizeof(server_addr), buf, buflen + sizeof(struct tunnel_packet));
		}

		if(FD_ISSET(server_fd, &rfds)) {
			buflen = socket_get_packet(server_fd, &from, &fromlen, buf, sizeof(buf));
			if(server_addr.sin_addr.s_addr == from.sin_addr.s_addr && server_addr.sin_port == from.sin_port)
				tun_put_packet(tun_fd, tp->data, buflen-sizeof(struct tunnel_packet));
		}
	}

	return 1;
}
