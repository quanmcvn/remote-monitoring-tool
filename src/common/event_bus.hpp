#ifndef COMMON_EVENT_BUS_HPP
#define COMMON_EVENT_BUS_HPP

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/event.hpp"

class EventBus {
public:
	using Callback = std::function<void(const Event&)>;

private:
	std::vector<Callback> subscribers;
	std::mutex subscribers_mutex;

public:
	void subscribe(Callback cb);
	void publish(const Event& event);
};

#endif