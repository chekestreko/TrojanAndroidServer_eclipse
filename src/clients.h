#ifndef CLIENTS_H_
#define CLIENTS_H_

#include <vector>
#include "client.h"
#include <optional>

class Clients {
public:
	size_t GetClientCount();
	void PrintClients();
	std::optional<int> AddClient(const Client& c_new);
	void RemoveClientBySockFD(const int iSockFD);
	std::optional<std::reference_wrapper<Client>> FindClientBySockFD(const int iSockFD);
	std::vector<Client>::iterator FindClientByID(const std::string strClientID);
	std::optional<std::reference_wrapper<Client>> GetActiveClient();
	void SetActiveClient(std::string strID);
	void PrintPrompt() {
		auto activeClient = GetActiveClient();
		if (activeClient)
			std::cout << activeClient->get().GetStrID() << std::string(">") << std::flush;
		else
			std::cout << std::string("no active client") << std::string(">") << std::flush;
	}
	void PrintInfo(const std::string& str) {
		std::cout << std::endl << str << std::endl;
		PrintPrompt();
	}
private:
	std::string activeClientID;
	std::vector<Client> vecClients;
};

#endif /* CLIENTS_H_ */
