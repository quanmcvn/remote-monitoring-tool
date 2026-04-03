#ifndef COMMON_EXPONENTIAL_BACKOFF
#define COMMON_EXPONENTIAL_BACKOFF

class ExponentialBackoff {
private:
	const int start;
	const int stop;
	const int multipler;
	int current_delay;

public:
	ExponentialBackoff();
	ExponentialBackoff(int start, int stop, int multiplier);
	// clear current delay
	void success();
	// wait(current delay) and update it
	void fail();
};

#endif