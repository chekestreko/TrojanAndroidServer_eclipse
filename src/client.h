#ifndef CLIENT_H_
#define CLIENT_H_

#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <functional>

class Client {
public:
	Client(std::string _strID, int _iSock);
	void ResetSocket(int fd);
	int GetSocketFD() const;
	std::string GetStrID() const;
	void AddSentText(const std::string& str);
	void AddReceivedText(const std::string& str, std::optional<std::reference_wrapper<Client>> activeClient);
	friend std::ostream& operator<<(std::ostream& strm, const Client& c) {
		strm << c.strID;
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
	void ReplaceSocketFD(int iNewSockFD);

private:
	std::vector<Client::TxtRecord> vecTxtRecords;
	std::string strID;
	int iSockFD;
};

#endif /* CLIENT_H_ */
