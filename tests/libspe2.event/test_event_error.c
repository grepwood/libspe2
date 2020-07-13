/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2007 Sony Computer Entertainment Inc.
 *  Copyright 2007 Sony Corp.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This test checks if the library can handle event-related errors
 * correctly.
 */

#include "ppu_libspe2_test.h"

#include <string.h>

static int test(int argc, char **argv)
{
  int ret;
  spe_event_handler_ptr_t evhandler;
  spe_event_unit_t event;
  spe_context_ptr_t spe, spe_no_event;
  spe_stop_info_t stop_info;

  spe_no_event = spe_context_create(0, NULL);
  if (!spe_no_event) {
    eprintf("spe_context_create: %s\n", strerror(errno));
    fatal();
  }

  spe = spe_context_create(SPE_EVENTS_ENABLE, NULL);
  if (!spe) {
    eprintf("spe_context_create: %s\n", strerror(errno));
    fatal();
  }

  evhandler = spe_event_handler_create();
  if (!evhandler) {
    eprintf("spe_event_handler_create: %s\n", strerror(errno));
    fatal();
  }

  /* operations on an event-disabled context (should be failed) */
  ret = spe_stop_info_read(spe_no_event, &stop_info);
  if (ret == 0) {
    eprintf("spe_stop_info_read: Unexpected success.\n");
    fatal();
  }
  if (errno != ENOTSUP) {
    eprintf("spe_stop_info_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* error (invalid parameters) */
  event.spe = spe;
  event.events = SPE_EVENT_IN_MBOX;
  ret = spe_event_handler_register(NULL, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_event_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_event_handler_register(evhandler, NULL);
  if (ret != -1) {
    eprintf("spe_event_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_event_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = NULL;
  event.events = SPE_EVENT_IN_MBOX;
  ret = spe_event_handler_register(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_event_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = spe_no_event;
  event.events = SPE_EVENT_IN_MBOX;
  ret = spe_event_handler_register(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != ENOTSUP) {
    eprintf("spe_event_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = spe;
  event.events = -1;
  ret = spe_event_handler_register(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != ENOTSUP) {
    eprintf("spe_event_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }


  /* register event for spe_event_handler_deregister tests */
  event.spe = spe;
  event.events = SPE_EVENT_SPE_STOPPED;
  ret = spe_event_handler_register(evhandler, &event);
  if (ret) {
    eprintf("spe_event_handler_register(%p, %p): %s\n",
	    evhandler, &event, strerror(errno));
    failed();
  }

  ret = spe_event_handler_deregister(evhandler, NULL);
  if (ret != -1) {
    eprintf("spe_event_handler_deregister: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_event_handler_deregister: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = NULL;
  event.events = SPE_EVENT_IN_MBOX;
  ret = spe_event_handler_deregister(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_deregister: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_event_handler_deregister: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = spe_no_event;
  event.events = SPE_EVENT_IN_MBOX;
  ret = spe_event_handler_deregister(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_deregister: Unexpected success.\n");
    fatal();
  }
  if (errno != ENOTSUP) {
    eprintf("spe_event_handler_deregister: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  event.spe = spe;
  event.events = -1;
  ret = spe_event_handler_deregister(evhandler, &event);
  if (ret != -1) {
    eprintf("spe_event_handler_deregister: Unexpected success.\n");
    fatal();
  }
  if (errno != ENOTSUP) {
    eprintf("spe_event_handler_deregister: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* clean up */
  event.spe = spe;
  event.events = SPE_EVENT_SPE_STOPPED;
  ret = spe_event_handler_deregister(evhandler, &event);
  if (ret) {
    eprintf("spe_event_handler_deregister(%p, %p): %s\n",
	    evhandler, &event, strerror(errno));
    fatal();
  }

  ret = spe_event_handler_destroy(evhandler);
  if (ret) {
    eprintf("spe_event_handler_destroy(%p): %s\n", evhandler, strerror(errno));
    fatal();
  }

  ret = spe_context_destroy(spe_no_event);
  if (ret) {
    eprintf("spe_context_destroy: %s\n", strerror(errno));
    fatal();
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy: %s\n", strerror(errno));
    fatal();
  }

  /* error (invalid event handler) */
  ret = spe_event_handler_destroy(NULL);
  if (ret == 0) {
    eprintf("spe_event_handler_destroy: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_event_handler_destroy: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
