#include "ConnectionProxy.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

ConnectionProxy::ConnectionProxy():m_efd(-1) {
}

void ConnectionProxy::SetFDnoneBlocking(int iFD) {
	int flags = fcntl(iFD, F_GETFL, 0);
	if (fcntl(iFD, F_SETFL, flags | O_NONBLOCK) < 0) {
		fprintf(stderr, "Error on fcntl: %s\n", strerror(errno));
		exit(-1);
	}
}

int ConnectionProxy::SetupListenSoket(const int port) {
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

	SetFDnoneBlocking(sockListen);

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

std::vector<int> ConnectionProxy::AddNewIncomingConnections(int iListenFD) {
	std::vector<int> v;
	bool bReset = false;
	do {
		do {
			sockaddr_in sa_in;
			socklen_t len = sizeof(sa_in);
			int new_sd = accept(iListenFD, (sockaddr*)&sa_in, &len);
			if (new_sd < 0) {
				if (errno != EWOULDBLOCK) {
					perror("  accept() failed");
					exit(1);
				}
				break;//no more incoming connection in case of EWOULDBLOCK
			}
			printf("proxy: accepted client fd = %d\n", new_sd);
			//SetFDnoneBlocking(new_sd);
			v.push_back(new_sd);
		} while (true);

		if(v.size() > 2) {//closed accepted sockets if there were more than 2
			for(int fd : v)
				close(fd);
			v.clear();
			bReset = true;
		} else
			bReset = false;
	}while(bReset);
	return v;
}

bool ConnectionProxy::StartDataExchange(std::vector<int>& vFDs) {
	pollfd p[] = {pollfd {vFDs[0], POLLIN}, pollfd {vFDs[1], POLLIN}, pollfd {m_efd, POLLIN}};

	bool bRun = true, bEventFD = false;
	while(bRun) {
		int iPollRes = poll(p, sizeof(p)/sizeof(pollfd), 5000);
		if (iPollRes < 0) {
			fprintf(stderr, "Error %d on poll: %s\n", iPollRes, strerror(errno));
			exit(-1);
		} else if (iPollRes == 0){//timeout
			break;
		}

		for (unsigned int i = 0; i < sizeof(p)/sizeof(pollfd); i++) {
			if (p[i].revents == 0)
				continue;
			if (p[i].revents != POLLIN) {
				printf("Error! p[%d].revents = %d, FD=%d\n", i, p[i].revents, p[i].fd);
				if (p[i].revents & POLLERR) {
					bRun = false;
					break;
				}
			}
			if (p[i].fd != m_efd) {
				char buf[100000];
				ssize_t r;
				r = recv(p[i].fd, buf, sizeof(buf), 0);
				if(r < 1) {//0=other side closed the connection, -1=error
					bRun = false;
					break;
				}
				ssize_t s;
				//auto t1 = std::chrono::system_clock::now().time_since_epoch();
				s = send(p[i == 0 ? 1 : 0].fd, buf, r, 0);
				/*auto t2 = std::chrono::system_clock::now().time_since_epoch();
				auto delta = t2-t1;
				std::cout << s << " bytes " << std::chrono::duration_cast<std::chrono::microseconds>(delta).count() <<
						"us" << std::endl;*/
				if(s < 1) {//0=other side closed the connection, -1=error
					bRun = false;
					break;
				}
				if(s != r) {
					fprintf(stderr, "s(%d) != r(%d)\n", s, r);
					bRun = false;
					break;
				}
			} else {//eventfd set, we should stop
				bRun = false;
				bEventFD = true;
				break;
			}
		}
	}
	close(p[0].fd);
	close(p[1].fd);
	printf("closed FD=%d, FD=%d\n", p[0].fd, p[1].fd);
	return bEventFD;
}

void ConnectionProxy::Start(const int port) {
	m_efd = eventfd(0, 0);
	t = std::thread(&ConnectionProxy::_start, this, port);
}

void ConnectionProxy::Stop() {
	if(m_efd != -1) {
		uint64_t u;
		write(m_efd, &u, sizeof(u));
		t.join();
	}
}

void ConnectionProxy::_start(const int port) {
	int sockListen = SetupListenSoket(port);

	vecPollListenFD = {pollfd {sockListen, POLLIN}, pollfd {m_efd, POLLIN}};
	std::vector<int> vConnectedSocketFDs;
	bool bRun = true;
	while (bRun) {
		if (poll(vecPollListenFD.data(), vecPollListenFD.size(), -1) < 1) {
			fprintf(stderr, "Error on poll: %s\n", strerror(errno));
			exit(-1);
		}
		for (unsigned int i = 0; i < vecPollListenFD.size(); i++) {
			if (vecPollListenFD[i].revents == 0)
				continue;
			if (vecPollListenFD[i].revents != POLLIN) {
				printf("%s Error! vecPollFDs[%d].revents = %d\n", __PRETTY_FUNCTION__, i, vecPollListenFD[i].revents);
				if (vecPollListenFD[i].revents & POLLERR) {
					bRun = false;
					break;
				}
				continue;
			}
			if (vecPollListenFD[i].fd == sockListen) {
				std::vector<int> v = AddNewIncomingConnections(sockListen);
				vConnectedSocketFDs.insert(vConnectedSocketFDs.end(), v.begin(), v.end());
				printf("vConnectedSocketFDs.size()=%d\n", vConnectedSocketFDs.size());
				if(vConnectedSocketFDs.size() == 2) {//as soon as 2 clients are connected, we can start exchange
					if(StartDataExchange(vConnectedSocketFDs)) {//returns true if eventfd was set, so we should stop
						bRun = false;
						break;
					}
					vConnectedSocketFDs.clear();
				}
			} else if (vecPollListenFD[i].fd == m_efd) {
				bRun = false;
				break;
			}
		}
	}
	for (int fd : vConnectedSocketFDs)
		close(fd);
	close(sockListen);
	close(m_efd);
	m_efd = -1;
}

ConnectionProxy::~ConnectionProxy() {
}

