#include "clients.h"
#include <iostream>
#include <unistd.h>
#include <optional>
#include <functional>
#include "TrojanTime.h"
#include "Journal.h"

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

std::optional<int> Clients::AddClient(Client&& c_new) {
	for (auto&& c_exist : vecClients) {
		if (c_exist.GetStrID() == c_new.GetStrID()) {
			//if we already have a client with a such ID, then just replace an existing one
			int iOldFD = c_exist.GetSocketFD();
			c_exist.ReplaceSocketFD(c_new.GetSocketFD(), c_new.GetConnectionsInfo()[0].sa_in);
			std::cout << "Client replaced: " << c_exist << std::endl;
			return iOldFD;
		}
	}
	vecClients.emplace_back(c_new);

	if (vecClients.size() == 1)
		activeClientID = c_new.GetStrID();
	PrintInfo(CurrentTime() + " added new client: " + c_new.GetStrID());
	Journal::get().WriteLn("added new client: ", c_new.GetStrID(), "\n");
	return std::nullopt;
}

void Clients::RemoveClientBySockFD(const int iSockFD) {
	for (std::vector<Client>::iterator it = vecClients.begin(); it != vecClients.end(); ++it) {
		if (it->GetSocketFD() == iSockFD) {
			bool bDeletedActiveClient = GetActiveClient_const()->get().GetSocketFD() == iSockFD;
			std::string strRemovedClient = it->GetStrID();
			vecClients.erase(it);
			if (bDeletedActiveClient)
				activeClientID = (vecClients.size() > 0) ? (vecClients[0].GetStrID()) : ("");
			PrintInfo("removed client:" + strRemovedClient);
			Journal::get().WriteLn("removed client: ", strRemovedClient, "\n");
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

const std::vector<Client>::iterator Clients::FindClientByID(const std::string& strClientID) {
	return std::find_if(vecClients.begin(), vecClients.end(),
			[&strClientID](const Client& c){return c.GetStrID() == strClientID;});
}

std::optional<std::reference_wrapper<Client>> Clients::GetClientByID(const std::string& strClientID) {
	std::vector<Client>::iterator itC = FindClientByID(strClientID);
	if ( vecClients.end() == itC )
		return std::nullopt;
	return std::optional<std::reference_wrapper<Client>> { *itC };
}

std::optional<std::reference_wrapper<Client>> Clients::GetActiveClient() {
	if (vecClients.empty())
		return std::nullopt;
	return std::optional<std::reference_wrapper<Client>> { *FindClientByID(activeClientID) };
}

std::optional<std::reference_wrapper<const Client>> Clients::GetActiveClient_const() {
	if (vecClients.empty())
		return std::nullopt;
	return std::optional<std::reference_wrapper<const Client>> { *FindClientByID(activeClientID) };
}

void Clients::SetActiveClient(std::string strID) {
	auto c = FindClientByID(strID);
	if (c != vecClients.end()) {
		c->DumpCommunication();
		activeClientID = strID;
	} else {
		std::cout << "no such client" << std::endl;
	}
	PrintPrompt();
}

void Clients::PrintPrompt() {
	auto activeClient = GetActiveClient_const();
	if (activeClient)
		std::cout << activeClient->get().GetStrID() << ">" << std::flush;
	else
		std::cout << "no active client" << ">" << std::flush;
}

void Clients::PrintInfo(const std::string& str) {
	std::cout << std::endl << str << std::endl;
	PrintPrompt();
}

bool Clients::IsClientActive(const Client& c) {
	auto activeClient = GetActiveClient_const();
	if(activeClient && c.GetStrID() == activeClient->get().GetStrID()) {
		return true;
	}
	return false;
}
