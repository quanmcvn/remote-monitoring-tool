#include "common/event_bus.hpp"

void EventBus::subscribe(Callback cb) {
	std::lock_guard<std::mutex> lock(subscribers_mutex);
	subscribers.push_back(cb);
}

void EventBus::publish(const Event& event) {
	// copy to a new vector to allow calling publish() in publish()
	std::vector<Callback> copy;
	{
		std::lock_guard<std::mutex> lock(subscribers_mutex);
		copy = subscribers;
	}
	// runs the callbacks in place
	// should be fine for the mean time
	for (auto& cb : copy) {
		cb(event);
	}
}