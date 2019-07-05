#include "client.h"
#include "TrojanTime.h"
#include <unistd.h>
#include <iostream>

Client::Client(std::string _strID, int _iSock) :
		strID(_strID), iSockFD(_iSock) {
}
void Client::ResetSocket(int fd) {
	close(fd);
	iSockFD = fd;
}
int Client::GetSocketFD() const {
	return iSockFD;
}
void Client::ReplaceSocketFD(int iNewSockFD) {
	iSockFD = iNewSockFD;
}
std::string Client::GetStrID() const {
	return strID;
}
void Client::AddSentText(const std::string& str) {
	vecTxtRecords.push_back(TxtRecord(str, enTxtType::sent));
}
void Client::AddReceivedText(const std::string& str, std::optional<std::reference_wrapper<Client>> activeClient) {
	if (activeClient) {
		if (strID == std::optional<std::reference_wrapper<Client>> { activeClient }->get().GetStrID()) {
			std::cout << std::endl << strID << std::string("(") << CurrentTime() << std::string(")") <<
					std::string("<") << str << std::string("\n") << std::flush;
		}
	}

	vecTxtRecords.push_back(TxtRecord(str, enTxtType::received));
}
void Client::DumpCommunication() {
	for (auto&& t : vecTxtRecords) {
		switch (t.GetTextType()) {
		case sent:
			std::cout << std::string(">");
			break;
		case received:
			std::cout << std::string("<");
			break;
		}
		std::cout << t << std::endl;
	}
}
