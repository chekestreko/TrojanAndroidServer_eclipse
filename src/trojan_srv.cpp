#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <vector>
#include <map>
#include <algorithm>
#include <optional>
#include <functional>
#include <unistd.h>
#include <fcntl.h>
#include "client.h"
#include "clients.h"
#include "TrojanTime.h"
#include <netinet/in.h>

/*
 * read from a FD (int) and save data in std::string
 * until EOL was received
 */
struct ReadLineAndSockAddr {
	std::string strLine;
	sockaddr_in sa_in;
};
std::map<int, ReadLineAndSockAddr> mapReadLineFromFD;
std::vector<pollfd> vecPollFDs;
Clients clients;

std::vector<int> vecFDs2Close;

void CloseFDs() {
	for(const auto& iFD : vecFDs2Close) {
		close(iFD);
		mapReadLineFromFD.erase(iFD);
		for (std::vector<pollfd>::iterator it = vecPollFDs.begin(); it != vecPollFDs.end(); ++it) {
			if (it->fd == iFD) {
				vecPollFDs.erase(it);
				break;
			}
		}
	}
	vecFDs2Close.clear();
}

void AddFD2BeClosed(int iFD) {
	vecFDs2Close.push_back(iFD);
}

void SendData2Client(const int iFD, const std::string& str_buf) {
	//we write to a non blocking socket, thats why such a complicated stuff with EWOULDBLOCK and poll
	//write timeout is set to 10 seconds
	ssize_t sent_all = 0, sent = 0;
	auto lambdaFD_close = [iFD]() {
		clients.RemoveClientBySockFD(iFD);
		AddFD2BeClosed(iFD);
	};
	const ssize_t dataSize = str_buf.length();
	while (sent_all < dataSize) {
		sent = send(iFD, &str_buf[sent_all], dataSize - sent_all, 0);
		if (sent < 0) {
			if (errno == EWOULDBLOCK) {
				pollfd p = pollfd { iFD, POLLOUT };
				int iPollRes = poll(&p, 1, 10000);
				if (iPollRes < 0) {
					fprintf(stderr, "Error on poll: %s\n", strerror(errno));
					exit(-1);
				} else if (iPollRes == 0) {
					std::cout << "timeout" << std::endl;
					lambdaFD_close();
					return;
				} else {
					if (p.revents != POLLOUT) {
						printf("Error! revents = %d\n", p.revents);
						exit(-2);
					}
					continue;
				}
			}
			std::cerr << std::string("send returned error:") << errno << std::endl;
			lambdaFD_close();
		} else {
			sent_all += sent;
		}
	}
}

void SetFDnoneBlocking(int iFD) {
	int flags = fcntl(iFD, F_GETFL, 0);
	if (fcntl(iFD, F_SETFL, flags | O_NONBLOCK) < 0) {
		fprintf(stderr, "Error on fcntl: %s\n", strerror(errno));
		exit(-1);
	}
}

int SetupListenSoket(int port) {
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
	while ((-1 == (iTmp = listen(sockListen, 0))) && (EINTR == errno))
		;
	return sockListen;
}

void AddNewIncomingConnection(std::vector<pollfd>& vecPollFDs, unsigned int index) {
	do {
		sockaddr_in sa_in;
		socklen_t len = sizeof(sa_in);
		int new_sd = accept(vecPollFDs[index].fd, (sockaddr*)&sa_in, &len);
		if (new_sd < 0) {
			if (errno != EWOULDBLOCK) {
				perror("  accept() failed");
				exit(1);
			}
			break;
		}
		mapReadLineFromFD[new_sd].sa_in = sa_in;
		clients.PrintInfo("Incoming connection, fd=" + std::to_string(new_sd));
		SetFDnoneBlocking(new_sd);
		vecPollFDs.push_back(pollfd { new_sd, POLLIN });
	} while (true);
}

void ClearLineAndGoToBegin() {
	char clear_line[] = "\033[2K";
	char line_up[] = "\033[1A";
	std::cout << clear_line << std::endl << line_up;
}

void ReadDataFromClient(std::vector<pollfd>& vecPollFDs, const unsigned int index) {
	auto client = clients.FindClientBySockFD(vecPollFDs[index].fd);
	do {
		unsigned char byteRead;
		int rc = recv(vecPollFDs[index].fd, &byteRead, sizeof(byteRead), 0);
		if (rc < 0) {
			if (errno != EWOULDBLOCK) {
				perror("  recv() failed");
				exit(1);
			} else {
				if (errno == EINTR)
					continue;
			}
			break;
		} else if (rc == 0) {	//remote host closed a connection
			std::cout << "  Connection closed fd=" << vecPollFDs[index].fd << std::endl;
			if (client) {
				std::cout << "removing client: " << client->get().GetStrID() << std::endl;
				clients.RemoveClientBySockFD(vecPollFDs[index].fd);
			} else {
				std::cout << "unknown client" << std::endl;
			}
			AddFD2BeClosed(vecPollFDs[index].fd);
			break;
		} else {	//read a data
			if (byteRead == '\n') {
				std::string& strLine = mapReadLineFromFD[vecPollFDs[index].fd].strLine;
				//printf("readline: %s\n", strLine.c_str());
				if (client) {
					auto activeClient = clients.GetActiveClient();
					client->get().AddReceivedText(strLine, activeClient);
					if (client->get().GetStrID() == activeClient->get().GetStrID()) {
						ClearLineAndGoToBegin();
						std::cout << client->get().GetStrID() << std::string("(") <<
								CurrentTime() << std::string(")") <<
								std::string("<") << strLine << std::string("\n") << std::flush;
						clients.PrintPrompt();
					}
				} else {
					if (auto iFD_replaced = clients.AddClient(Client(strLine, vecPollFDs[index].fd,
							mapReadLineFromFD[vecPollFDs[index].fd].sa_in)))
						AddFD2BeClosed(*iFD_replaced);
				}
				strLine = "";
			} else {
				mapReadLineFromFD[vecPollFDs[index].fd].strLine += byteRead;
			}
		}
	} while (true);
}

void HandleStdinRead() {
	std::string str;
	std::getline(std::cin, str);
	if (str == "#") {
		auto count = clients.GetClientCount();
		if (count == 0) {
			std::cout << "no clients" << std::endl;
		} else {
			clients.PrintClients();
			std::cout << "choose a client:";
			std::getline(std::cin, str);
			clients.SetActiveClient(str);
		}
	} else {
		auto client = clients.GetActiveClient();
		if (client) {
			client->get().AddSentText(str);
			str += "\n";
			SendData2Client(client->get().GetSocketFD(), str);
			clients.PrintPrompt();
		}
	}
}

int main() {
	const int port = 18650;
	int sockListen = SetupListenSoket(port);
	std::cout<< "server started at: " << CurrentTime() << ", listening on port " << port << std::endl;
	vecPollFDs = {pollfd {STDIN_FILENO, POLLIN}, pollfd {sockListen, POLLIN}};

	while (true) {
		if (poll(vecPollFDs.data(), vecPollFDs.size(), -1) < 1) {
			fprintf(stderr, "Error on poll: %s\n", strerror(errno));
			exit(-1);
		}
		for (unsigned int i = 0; i < vecPollFDs.size(); i++) {
			if (vecPollFDs[i].revents == 0)
				continue;
			if (vecPollFDs[i].revents != POLLIN) {
				printf("Error! vecPollFDs[%d].revents = %d\n", i, vecPollFDs[i].revents);
				if (vecPollFDs[i].revents & POLLERR) {
					clients.RemoveClientBySockFD(vecPollFDs[i].fd);
					AddFD2BeClosed(vecPollFDs[i].fd);
				}
				continue;
			}

			if (vecPollFDs[i].fd == STDIN_FILENO) {
				HandleStdinRead();
			} else if (vecPollFDs[i].fd == sockListen) {
				AddNewIncomingConnection(vecPollFDs, i);
			} else {
				ReadDataFromClient(vecPollFDs, i);
			}
		}
		CloseFDs();
	}
	return 0;
}
