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
	std::optional<std::reference_wrapper<Client>> GetClientByID(const std::string& strClientID);
	std::optional<std::reference_wrapper<Client>> GetActiveClient();
	void SetActiveClient(std::string strID);
	void PrintPrompt();
	void PrintInfo(const std::string& str);
	bool IsClientActive(const Client& c);
private:
	std::vector<Client>::iterator FindClientByID(const std::string strClientID);
	std::string activeClientID;
	std::vector<Client> vecClients;
};

#endif /* CLIENTS_H_ */
