#include "common/event_bus.hpp"

void EventBus::subscribe(Callback cb) {
	std::lock_guard<std::mutex> lock(subscribers_mutex);
	subscribers.push_back(cb);
}

void EventBus::publish(const Event& event) {
	std::lock_guard<std::mutex> lock(subscribers_mutex);
	// runs the callbacks in place
	// should be fine for the mean time
	for (auto& cb : subscribers) {
		cb(event);
	}
}