#include "client.h"
#include "TrojanTime.h"
#include <unistd.h>
#include <iostream>

Client::Client(const std::string& _strID, int _iSock, const sockaddr_in _sa_in) :
		strID(_strID), iSockFD(_iSock) {
	vecConnEndPointsAndTimes.push_back({std::chrono::system_clock::now(), _sa_in});
}
void Client::ResetSocket(int fd) {
	close(fd);
	iSockFD = fd;
}
int Client::GetSocketFD() const {
	return iSockFD;
}
void Client::ReplaceSocketFD(int iNewSockFD, const sockaddr_in _sa_in) {
	iSockFD = iNewSockFD;
	vecConnEndPointsAndTimes.push_back({std::chrono::system_clock::now(), _sa_in});
}
std::string Client::GetStrID() const {
	return strID;
}
void Client::AddSentText(const std::string& str) {
	vecTxtRecords.push_back(TxtRecord(str, enTxtType::sent));
}
void Client::AddReceivedText(const std::string& str, std::optional<std::reference_wrapper<Client>> activeClient) {
	vecTxtRecords.push_back(TxtRecord(str, enTxtType::received));
}
void Client::DumpCommunication() {
	for (auto&& t : vecTxtRecords) {
		switch (t.GetTextType()) {
		case sent:
			std::cout << ">";
			break;
		case received:
			std::cout << "<";
			break;
		}
		std::cout << t << std::endl;
	}
}
