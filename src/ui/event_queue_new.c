#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "event_queue.h"
#include "event.h"

struct EventQueue {
	EventSubQueue **subqueues;
	uint32_t subqueues_size; // The number of subqueues we're receiving Events from.
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

// Send an Event via this subqueue to the main EventQueue.
// Makes a copy of *evt (you don't need to heap-allocate events)
// NOTE: may block if the subqueue is full. If this isn't desired, pass allow_drop=true,
// which causes the subqueue to drop new events when it doesn't have room for them.
void EventSubQueue_send(EventSubQueue *sq, Event *evt, bool allow_drop);
// Try to read an event from this subqueue.
// Sets *evt to either the value of the event received or NULL if the subqueue is empty.
// NOTE: never blocks.
void EventSubQueue_recv(EventSubQueue *sq, Event **evt);

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

void EventSubQueue_send(EventSubQueue *sq, Event *evt, bool allow_drop) {
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

void EventSubQueue_recv(EventSubQueue *sq, Event **evt) {
	const int wr = atomic_load(&sq->wr);
	int rd = atomic_load(&sq->rd);

	if (rd == wr) {
		// ring buffer is empty
		*evt = NULL;
		return;
	}

	// Increment rd idx 
	rd++;
	rd %= sq->n_events_size;

	// Store new rd index
	atomic_store(&sq->rd, rd);
	// Notify subqueue's thread that an Event has been read
	// (thus, wake any write on subqueue's thread blocking due to a full subqueue)
	sem_post(&sq->rd_sem);
}
