#include "event.h"
#include "event_queue.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <unistd.h>

struct EventQueue {
	// Message queue handle
	mqd_t mq; 
};

static void mq_gen_name_(char *name, size_t name_max) {
	snprintf(name, name_max, "/mpl/ui/event_queue/%d", getpid());
}

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));

	// Generate mq name
	char mq_name[NAME_MAX];
	mq_gen_name_(mq_name, NAME_MAX);
	eq->mq = mq_open(mq_name, O_RDWR | O_CREAT);
	if (eq->mq == -1) {
		perror("EventQueue message queue");
		free(eq);
		return NULL;
	}

	return eq;
}
void EventQueue_free(EventQueue *eq) {
	mq_close(eq->mq);
	free(eq);
}
