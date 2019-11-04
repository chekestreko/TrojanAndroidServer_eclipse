#ifndef CONNECTIONPROXY_H_
#define CONNECTIONPROXY_H_

#include <thread>
#include <vector>
#include <sys/poll.h>

/*
 * works for 2 peers
 */
class ConnectionProxy {
public:
	ConnectionProxy();
	std::vector<int> AcceptIncomingConnections(const int iFD);
	void Start(const int port);
	void Stop();
	virtual ~ConnectionProxy();
private:
	void _start(const int port, const int iConnectionTimeout);
	bool StartDataExchange(std::vector<int>& vFDs);
	int SetupListenSoket(const int port);
	std::thread m_myThread;
	std::vector<pollfd> m_vecPollListenFD;
	std::thread m_t;
	int m_efd;
};

#endif /* CONNECTIONPROXY_H_ */
