#include "event.h"
#include "event_queue.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <stdlib.h>
#include <unistd.h>

struct EventQueue {
	// Message queue name, needed to open other handles in the future
	char name[NAME_MAX];
	// Message queue handle
	mqd_t mq; 
};

// Universal EventQueue message queue attributes
static const  struct mq_attr EventQueue_mq_attr = {
	.mq_maxmsg = 5,
	.mq_msgsize = sizeof(Event)
};

static void mq_gen_name_(char *name, size_t name_max) {
	snprintf(name, name_max, "/mpl_event_queue_%d", getpid());
}

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));

	// Generate mq name
	mq_gen_name_(eq->name, sizeof(eq->name));
	eq->mq = mq_open(eq->name, O_RDWR | O_CREAT, 0600, &EventQueue_mq_attr);
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

int EventQueue_send(EventQueue *eq, const Event *evt) {
	int status = mq_send(eq->mq, (const char *)evt, sizeof(Event), 0);
	if (status == -1) {
		perror("EventQueue message send");
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

EventQueue *EventQueue_connect(const EventQueue *eq1, int oflags) {
	EventQueue *eq = malloc(sizeof(EventQueue));
	strncpy(eq->name, eq1->name, sizeof(eq->name));
	eq->mq = mq_open(eq->name, oflags, 0600, &EventQueue_mq_attr);
	if (eq->mq == -1) {
		perror("EventQueue_connect message queue");
		free(eq);
		return NULL;
	}

	return eq;
}
