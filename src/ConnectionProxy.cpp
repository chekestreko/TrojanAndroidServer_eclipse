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
#include <iomanip>
#include <sstream>
#include <inttypes.h>
#include "NetUtils.h"
#include "TrojanTime.h"
#include "Journal.h"
#include <map>

#define DBG_PRINT(...) \
	{std::stringstream ss; ss << "ConnectionProxy(" << std::setfill(' ') << std::setw(3) << __LINE__ << "): ";\
	Journal::get().WriteLn(ss.str(), " ", __VA_ARGS__, "\n");}

ConnectionProxy::ConnectionProxy():m_efd(-1) {
}

std::vector<int> ConnectionProxy::AcceptIncomingConnections(const int iListenFD) {
	std::vector<int> v;
	bool bReset = false;
	do {
		do {
			sockaddr_in sa_in;
			socklen_t len = sizeof(sa_in);
			int new_sd = accept(iListenFD, (sockaddr*)&sa_in, &len);
			if (new_sd < 0) {
				if (errno != EWOULDBLOCK) {
					perror("accept() failed\n");
					exit(1);
				}
				break;//no more incoming connection in case of EWOULDBLOCK
			}
			DBG_PRINT("accepted client ", sa_in, " fd=", new_sd);
			NetUtils::SetFDnoneBlocking(new_sd);
			v.push_back(new_sd);
		} while (true);

		if(v.size() > 2) {//close all accepted sockets if there were more than 2
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
	DBG_PRINT("Entered StartDataExchange fds=", vFDs[0], ",", vFDs[1]);
	pollfd p[] = {pollfd {vFDs[0], POLLIN}, pollfd {vFDs[1], POLLIN}, pollfd {m_efd, POLLIN}};

	std::map<int, ssize_t> mapSentBytes;
	bool bRun = true, bEventFD = false;
	while(bRun) {
		int iPollRes = poll(p, sizeof(p)/sizeof(pollfd), 5000);
		if (iPollRes < 0) {
			DBG_PRINT("Error %d on poll: %s. Stop \n", iPollRes, strerror(errno));
			break;
		} else if (iPollRes == 0){//timeout
			DBG_PRINT("poll timeout");
			break;
		}

		for (unsigned int i = 0; i < sizeof(p)/sizeof(pollfd); i++) {
			if (p[i].revents == 0)
				continue;
			if (p[i].revents != POLLIN) {
				if (p[i].revents & POLLERR) {
					DBG_PRINT("POLLERR fd=", p[i].fd, " was closed");
					bRun = false;
					break;
				}
				DBG_PRINT("Error! p[%d].revents = %d, FD=%d\n", i, p[i].revents, p[i].fd);
			}
			if (p[i].fd != m_efd) {
				char buf[100000];
				ssize_t r;
				r = recv(p[i].fd, buf, sizeof(buf), 0);
				if(r < 1) {//0=other side closed the connection, -1=error
					DBG_PRINT("recv error. Other side of fd=", p[i].fd,
							(r == 0) ? " gracefully closed" : " lost", " connection, errno=", errno);
					bRun = false;
					break;
				}
				ssize_t s;
				//auto t1 = std::chrono::system_clock::now().time_since_epoch();
				int sFD = p[i == 0 ? 1 : 0].fd;
				s = send(sFD, buf, r, 0);
				/*auto t2 = std::chrono::system_clock::now().time_since_epoch();
				auto delta = t2-t1;
				std::cout << s << " bytes " << std::chrono::duration_cast<std::chrono::microseconds>(delta).count() <<
						"us" << std::endl;*/
				if(s < 1) {//0=other side closed the connection, -1=error
					DBG_PRINT("send error. Other side of fd=", sFD,
							(s == 0) ? " gracefully closed" : " lost", " connection, errno=", errno);
					bRun = false;
					break;
				}
				mapSentBytes[sFD] += s;
				if(s != r) {
					DBG_PRINT("s!=r ", s, "!=", r);
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
	for(const auto& pair : mapSentBytes)
		DBG_PRINT(pair.second, " Bytes sent to fd=", pair.first);
	DBG_PRINT("Leave StartDataExchange, closed fds=", p[0].fd, ",", p[1].fd);
	return bEventFD;
}

void ConnectionProxy::Start(const int port) {
	Stop();
	m_efd = eventfd(0, 0);
	m_t = std::thread(&ConnectionProxy::_start, this, port, 5000);
}

void ConnectionProxy::Stop() {
	DBG_PRINT("Enter Stop");
	if(m_efd != -1) {
		uint64_t u;
		DBG_PRINT("Stopping ConnectionProxy...");
		if(sizeof(u) != write(m_efd, &u, sizeof(u)))
			DBG_PRINT("Error stopping ConnectionProxy sizeof(u) != write...");
		m_t.join();
		DBG_PRINT("ConnectionProxy...stopped");
		m_efd = -1;
	} else {
		DBG_PRINT("not running");
	}
	DBG_PRINT("Leave Stop");
}

void ConnectionProxy::_start(const int port, const int iConnectionTimeout) {
	DBG_PRINT("started thread, listen port=", port);
	int sockListen = NetUtils::SetupListenSoket(port, 2);

	m_vecPollListenFD = {pollfd {sockListen, POLLIN}, pollfd {m_efd, POLLIN}};
	std::vector<int> vConnectedSocketFDs;
	bool bRun = true;
	while (bRun) {
		int iPollRes = poll(m_vecPollListenFD.data(), m_vecPollListenFD.size(), iConnectionTimeout);
		if (iPollRes < 0) {
			DBG_PRINT("Error ", iPollRes, " on poll: %s\n", strerror(errno));
			bRun = false;
			break;
		} else if (iPollRes == 0) {
			DBG_PRINT("No client appeared within of ", iConnectionTimeout, "ms. Stopping thread...");
			bRun = false;
			break;
		}
		for (unsigned int i = 0; i < m_vecPollListenFD.size(); i++) {
			if (m_vecPollListenFD[i].revents == 0)
				continue;
			if (m_vecPollListenFD[i].revents & POLLERR) {
				printf("%s Error! vecPollFDs[%d].revents = %d\n", __PRETTY_FUNCTION__, i, m_vecPollListenFD[i].revents);
				bRun = false;
				break;
			}

			if (m_vecPollListenFD[i].fd == sockListen) {
				std::vector<int> v = AcceptIncomingConnections(sockListen);//can return 1 or 2
				vConnectedSocketFDs.insert(vConnectedSocketFDs.end(), v.begin(), v.end());
				//printf("ConnectionProxy accepted %zu incoming connections\n", vConnectedSocketFDs.size());
				if(vConnectedSocketFDs.size() == 2) {//as soon as 2 clients are connected, we can start exchange
					if(StartDataExchange(vConnectedSocketFDs)) {//returns true if eventfd was set, so we should stop
						bRun = false;
						break;
					}
					vConnectedSocketFDs.clear();//FDs of a connected 2 clients are already closed in StartDataExchange
					//we do not have connected clients anymore
					//poll only on sockListen and m_efd
					if (m_vecPollListenFD.size() > 2)
						m_vecPollListenFD.erase(m_vecPollListenFD.begin() + 2, m_vecPollListenFD.end());
				} else if(vConnectedSocketFDs.size() == 1) {
					//if first client was connected
					//poll, not for data is ready to read, but for a connection was closed (POLLRDHUP)
					m_vecPollListenFD.push_back(pollfd {vConnectedSocketFDs[0], POLLRDHUP});
				}
			} else if ((vConnectedSocketFDs.size() > 0) && (m_vecPollListenFD[i].fd == vConnectedSocketFDs[0])) {
				if (m_vecPollListenFD[i].revents & POLLRDHUP) {
					DBG_PRINT("received POLLRDHUP for fd=", m_vecPollListenFD[i].fd);
					close(vConnectedSocketFDs[0]);
					DBG_PRINT("closed fd=", vConnectedSocketFDs[0]);
					vConnectedSocketFDs.clear();
					m_vecPollListenFD.erase(m_vecPollListenFD.begin() + i);
				} else {
					fprintf(stderr, "Error vecPollListenFD[i].revents & POLLRDHUP=%d", m_vecPollListenFD[i].revents);
				}
			} else if (m_vecPollListenFD[i].fd == m_efd) {
				bRun = false;
				break;
			}
		}
	}
	for (int fd : vConnectedSocketFDs) {
		close(fd);
		DBG_PRINT("closed fd=", fd);
	}
	close(sockListen);
	close(m_efd);
	DBG_PRINT("leave thread, closed FDs: sockListen=", sockListen, ", m_efd=", m_efd);
}

ConnectionProxy::~ConnectionProxy() {
	Stop();
}

