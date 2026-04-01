#ifndef CLIENT_EXPONENTIAL_BACKOFF
#define CLIENT_EXPONENTIAL_BACKOFF

class ExponentialBackoff {
private:
	const int start;
	const int stop;
	const int multipler;
	int current_delay;
public:
	ExponentialBackoff();
	ExponentialBackoff(int n_start, int n_stop, int n_multiplier);
	// clear current delay
	void success();
	// wait(current delay) and update it
	void fail();
};

#endif