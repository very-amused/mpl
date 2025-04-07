#include "event.h"
#include "event_queue.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

struct EventQueue {
	mqd_t mq;	// Message queue
};
