/**
 * Scenario 1: Portable Usage
 */
#include <libspe2.h>

#define MAX_EVENTS 8
#define SIZE 8
#define COUNT 1

int main()
{
  int i, rc, event_count;
  spe_event_handler_ptr_t evhandler;
  spe_event_unit_t event1, event2;
  //spe_context_ptr_t spe1, spe2;
  spe_event_unit_t events[MAX_EVENTS];
  unsigned int mbox_data[COUNT];
  spe_stop_info_t stop_info;

  /* Create a handle. */
  evhandler = spe_event_handler_create();

  /* Register events. */
  event1.events = SPE_EVENT_OUT_INTR_MBOX | SPE_EVENT_SPE_STOPPED;
//  rc = spe_epoll_ctl(epfd, SPE_EPOLL_CTL_ADD, spe1, &event1);
  rc = spe_event_handler_register(evhandler, &event1);

  event2.events = SPE_EVENT_IN_MBOX | SPE_EVENT_SPE_STOPPED;
  //rc = spe_epoll_ctl(epfd, SPE_EPOLL_CTL_ADD, spe2, &event2);
  rc = spe_event_handler_register(evhandler, &event2);

  /* Get events. */
  event_count = spe_event_wait(evhandler, events, MAX_EVENTS, 0);

  /* Handle events. */
	for (i = 0; i < event_count; i++) {
		if (events[i].events & SPE_EVENT_OUT_INTR_MBOX) {
			rc = spe_out_intr_mbox_read(events[i].spe, mbox_data, COUNT, SPE_MBOX_ANY_NONBLOCKING);
		}
		if (events[i].events & SPE_EVENT_SPE_STOPPED) {
			rc = spe_stop_info_read(events[i].spe, &stop_info);
		}
	}

  /* Destroy the handle. */
  spe_event_handler_destroy(evhandler);
  
  return 0;
}


