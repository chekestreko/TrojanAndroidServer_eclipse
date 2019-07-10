#ifndef CLIENT_H_
#define CLIENT_H_

#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <functional>
#include <chrono>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "TrojanTime.h"

class Client {
public:
	Client(const std::string& _strID, int _iSock, const sockaddr_in sa_in);
	void ResetSocket(int fd);
	int GetSocketFD() const;
	std::string GetStrID() const;
	void AddSentText(const std::string& str);
	void AddReceivedText(const std::string& str, std::optional<std::reference_wrapper<Client>> activeClient);
	friend std::ostream& operator<<(std::ostream& strm, const Client& c) {
		strm << "'" + c.strID + "'" << std::endl;
		for(auto&& cc : c.vecConnEndPointsAndTimes) {
			const std::chrono::time_point<std::chrono::system_clock>& t = cc.connectionTime;
			strm << PrintTime(t) + " " << std::string(inet_ntoa(cc.sa_in.sin_addr))+":"
					<<ntohs(cc.sa_in.sin_port) << std::endl;
		}
		return strm;
	}
	void DumpCommunication();
	enum enTxtType {
		sent, received
	};
	class TxtRecord: public std::string {
	public:
		TxtRecord(std::string str, enTxtType _enTxtType) :
				std::string(str), mTxtType(_enTxtType) {
		}
		const enTxtType& GetTextType() const {
			return mTxtType;
		}
	private:
		enTxtType mTxtType;
	};

protected:
	friend class Clients;
	void ReplaceSocketFD(int iNewSockFD, const sockaddr_in _sa_in);

private:
	std::vector<Client::TxtRecord> vecTxtRecords;
	struct connect_info_t {
		std::chrono::time_point<std::chrono::system_clock> connectionTime;
		sockaddr_in sa_in;
	};
	std::vector<connect_info_t> vecConnEndPointsAndTimes;
	std::string strID;
	int iSockFD;
public:
	const std::vector<connect_info_t>& GetConnectionsInfo() const {
		return vecConnEndPointsAndTimes;
	}
};

#endif /* CLIENT_H_ */
