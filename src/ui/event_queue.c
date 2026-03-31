#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "error.h"
#include "event_queue.h"
#include "event.h"
#include "util/log.h"

struct EventQueue {
	EventSubQueue **subqueues;
	uint32_t subqueues_size, subqueues_cap; // The number of subqueues we're receiving Events from.
	bool has_prev_subqueue;
	uint32_t prev_subqueue; // Index of the last subqueue we read an Event from. Used to check subqueues in a round-robin fashion whenever ANY subqueue wakes us.

	// Main write semaphore. Posted whenever ANY subqueue sends an event.
	sem_t main_wr_sem;
};

// A ring buffer used to hold Events
struct EventSubQueue {
	size_t n_events_size; // Total size of the buffer in Events

	Event *data;
	atomic_int rd, wr; // Read/write indices.

	// Semaphore providing read notifications for this subqueue
	sem_t rd_sem;
	// Pointer to main EventQueue semaphore providing write notifications for all subqueues
	sem_t *main_wr_sem;
};

int EventSubQueue_init(EventSubQueue *sq, size_t n_events_size, sem_t *main_wr_sem);
void EventSubQueue_deinit(EventSubQueue *sq);
void EventSubQueue_send(EventSubQueue *sq, const Event *evt, bool allow_drop);
// Try to read an event from this subqueue.
// Returns whether an event was read.
// NOTE: never blocks.
bool EventSubQueue_recv(EventSubQueue *sq, Event *evt);

EventQueue *EventQueue_new() {
	EventQueue *eq = malloc(sizeof(EventQueue));
	if (!eq) {
		return NULL;
	}
	memset(eq, 0, sizeof(EventQueue));

	// initialize subqueues array
	// BufferThread, InputThread, async loop for AudioBackend, round to power of 2
	static const size_t N_AUX_THREADS = 4;
	eq->subqueues_cap =	N_AUX_THREADS;
	eq->subqueues = malloc(eq->subqueues_cap * sizeof(EventSubQueue *));


	// Initialize main write semaphore
	sem_init(&eq->main_wr_sem, 0, 0);

	return eq;
}

void EventQueue_free(EventQueue *eq) {
	for (size_t i = 0; i < eq->subqueues_size; i++) {
		EventSubQueue *sq = eq->subqueues[i];
		EventSubQueue_deinit(sq);
		free(sq);
		eq->subqueues[i] = NULL;
	}
	free(eq->subqueues);
	free(eq);
}


EventSubQueue *EventQueue_connect(EventQueue *eq, size_t sq_size) {
	// Create new subqueue
	EventSubQueue *sq = malloc(sizeof(EventSubQueue));
	if (!sq) {
		return NULL;
	}
	if (EventSubQueue_init(sq, sq_size, &eq->main_wr_sem) != 0) {
		free(sq);
		return NULL;
	}

	// Add to subqueues array
	if (!eq->subqueues_cap) {
	} else if (eq->subqueues_size == eq->subqueues_cap) {
		// resize geometrically
		eq->subqueues_cap *= 2;
		eq->subqueues = realloc(eq->subqueues, eq->subqueues_cap * sizeof(EventSubQueue *));
	}
	eq->subqueues[eq->subqueues_size] = sq;
	eq->subqueues_size++;

	return sq;
}

int EventQueue_recv(EventQueue *eq, Event *evt) {
	// Wait for an event to be available
	if (sem_wait(&eq->main_wr_sem) != 0) {
		return 1;
	}
	// We now have a guarantee that at least one subqueue has an Event for us to receive

	// Go through subqueues round-robin until we can read an event from ANY subqueue
	// This prevents one subqueue from being able to overwhelm the others
	const size_t i_start = eq->has_prev_subqueue ? (eq->prev_subqueue + 1) % eq->subqueues_size : 0;
	const size_t i_end = eq->has_prev_subqueue ? eq->has_prev_subqueue : eq->subqueues_size;
	for (size_t i = i_start; i != i_end; i++, i %= eq->subqueues_size) {
		EventSubQueue *sq = eq->subqueues[i];

		// Try to recv an event from this subqueue
		if (EventSubQueue_recv(sq, evt)) {
			eq->prev_subqueue = i;
			return 0;
		}
	}

	LOG(Verbosity_VERBOSE, "EventQueue_recv failed to get an event. EventQueue->main_wr_sem should make this impossible!\n");
	return 1;
}



int EventSubQueue_init(EventSubQueue *sq, size_t n_events_size, sem_t *main_wr_sem) {
	memset(sq, 0, sizeof(EventSubQueue));

	sq->n_events_size = n_events_size;
	sq->data = malloc((n_events_size + 1) * sizeof(Event));
	if (!sq->data) {
		return 1;
	}
	sq->rd = 0;
	sq->wr = 0;

	// Initialize semaphores
	sem_init(&sq->rd_sem, 0, 0);
	sq->main_wr_sem = main_wr_sem;

	return 0;
}

void EventSubQueue_deinit(EventSubQueue *sq) {
	free(sq->data);
	sq->data = NULL;
}

void EventSubQueue_send(EventSubQueue *sq, const Event *evt, bool allow_drop) {
	int wr = atomic_load(&sq->wr);
	int rd = atomic_load(&sq->rd);

	if ((wr+1) % sq->n_events_size == rd) {
		// I'm not using this option, but it's good to provide it while I'm implementing EventSubQueue
		if (allow_drop) {
			return;
		}
		sem_wait(&sq->rd_sem);
		rd = atomic_load(&sq->rd);
	}

	memcpy(&sq->data[wr], evt, sizeof(Event));

	// Increment write idx
	wr++;
	wr %= sq->n_events_size;

	// Store new wr index
	atomic_store(&sq->wr, wr);
	// Notify the main thread that an Event has been sent
	sem_post(sq->main_wr_sem);
}

bool EventSubQueue_recv(EventSubQueue *sq, Event *evt) {
	const int wr = atomic_load(&sq->wr);
	int rd = atomic_load(&sq->rd);

	if (rd == wr) {
		// ring buffer is empty
		return false;
	}

	// Read event -> *evt
	memcpy(evt, &sq->data[rd], sizeof(Event));

	// Increment rd idx 
	rd++;
	rd %= sq->n_events_size;

	// Store new rd index
	atomic_store(&sq->rd, rd);
	// Notify subqueue's thread that an Event has been read
	// (thus, wake any write on subqueue's thread blocking due to a full subqueue)
	sem_post(&sq->rd_sem);

	return true;
}
