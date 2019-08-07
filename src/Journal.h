#ifndef JOURNAL_H_
#define JOURNAL_H_
#include <string>
#include <iostream>
#include <fstream>
#include "TrojanTime.h"

class Journal {
public:
	Journal(const Journal&) = delete;
	Journal(Journal&&) = delete;
	Journal& operator=(const Journal&) = delete;
	Journal& operator=(Journal&&) = delete;

	static Journal& get() {
		static Journal _instance;
		return _instance;
	}

	//C++17 feature "fold expressions"
	template<class... Args>
	void WriteLn(Args... args) {
		if(!_ofs.is_open())
			_ofs.open("/tmp/trojan_srv.log", std::ofstream::out | std::ofstream::app);
		if(!_ofs) {
			fprintf(stderr, "Error open log file");
			return;
		}
		_ofs << CurrentTime() << " ";
		(_ofs << ... << args);
		_ofs.flush();
	}

private:
	Journal(){}
	~Journal(){}

	std::ofstream _ofs;
};

#endif /* JOURNAL_H_ */
