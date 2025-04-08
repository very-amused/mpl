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
	snprintf(name, name_max, "/mpl_event_queue_%d", getpid());
}

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));

	// Generate mq name
	char mq_name[NAME_MAX];
	mq_gen_name_(mq_name, NAME_MAX);
	struct mq_attr queue_attr = {
		.mq_maxmsg = 5,
		.mq_msgsize = sizeof(Event)
	};
	eq->mq = mq_open(mq_name, O_RDWR | O_CREAT, 0600, &queue_attr);
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

int EventQueue_send(EventQueue *eq, Event *evt) {
	int status = mq_send(eq->mq, (const char *)evt, sizeof(Event), 0);
	if (status == -1) {
		perror("EventQueue message send");
		free(evt);
		return status;
	}
	return 0;
}
int EventQueue_recv(EventQueue *eq, Event *evt) {
	int status = mq_receive(eq->mq, (char *)evt, sizeof(Event), 0);
	if (status == -1) {
		perror("EventQueue message receive");
		free(evt);
		return status;
	}
	return 0;
}
