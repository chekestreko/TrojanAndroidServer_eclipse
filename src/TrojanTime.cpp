#include <iomanip>
#include "TrojanTime.h"

std::string CurrentTime()
{
	const auto now = std::chrono::system_clock::now();
	const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
	const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
	    now.time_since_epoch()) % 1000;
	std::stringstream nowSs;
	nowSs
		<< std::put_time(std::localtime(&nowAsTimeT), "%F %T")
		<< '.' << std::setfill('0') << std::setw(3) << nowMs.count();
	return nowSs.str();
}

std::string PrintTime(const std::chrono::time_point<std::chrono::system_clock>& time)
{
    auto in_time_t = std::chrono::system_clock::to_time_t(time);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%F %T");
    return ss.str();
}
