#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <iostream>
#include <netinet/in.h>

class NetUtils {
public:
	static void SetFDnoneBlocking(int iFD);
	static void SetTcpKeepAliveOnSocket(int iFD, int iKeepIdle, int iKeepIntvl);
	static int SetupListenSoket(const int port, const int iListenQueue);
};

std::ostream& operator<<(std::ostream& strm, const sockaddr_in& c);

#endif /* NETUTILS_H_ */
