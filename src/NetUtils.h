#ifndef NETUTILS_H_
#define NETUTILS_H_

class NetUtils {
public:
	static void SetFDnoneBlocking(int iFD);
	static int SetupListenSoket(const int port, const int iListenQueue);
};

#endif /* NETUTILS_H_ */
