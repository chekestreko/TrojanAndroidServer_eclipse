#include "NetUtils.h"
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>

void NetUtils::SetFDnoneBlocking(int iFD) {
	int flags = fcntl(iFD, F_GETFL, 0);
	if (fcntl(iFD, F_SETFL, flags | O_NONBLOCK) < 0) {
		std::cerr<<"Error on fcntl:"<<strerror(errno)<<std::endl;
		exit(-1);
	}
}

int NetUtils::SetupListenSoket(const int port, const int iListenQueue) {
	int sockListen = socket(AF_INET, SOCK_STREAM, 0);
	if (sockListen < 0) {
		fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
		exit(-1);
	}

	int on = 1;
	if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on)) < 0) {
		fprintf(stderr, "Error on setsockopt socket: %s\n", strerror(errno));
		exit(-1);
	}

	NetUtils::SetFDnoneBlocking(sockListen);

	struct sockaddr_in saddr = { };
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);

	if (bind(sockListen, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		fprintf(stderr, "Error on binding socket: %s\n", strerror(errno));
		exit(-1);
	}
	int iTmp;
	while ((-1 == (iTmp = listen(sockListen, 2))) && (EINTR == errno))
		;
	return sockListen;
}
