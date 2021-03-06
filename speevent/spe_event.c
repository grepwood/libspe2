/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS 
 *
 *  Copyright (C) 2006 Sony Computer Entertainment Inc.
 *  Copyright 2006 Sony Corp.
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

#include <stdlib.h>
#include "speevent.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <fcntl.h>

#define __SPE_EVENT_ALL \
  ( SPE_EVENT_OUT_INTR_MBOX | SPE_EVENT_IN_MBOX | \
    SPE_EVENT_TAG_GROUP | SPE_EVENT_SPE_STOPPED )

#define __SPE_EPOLL_SIZE 10

#define __SPE_EPOLL_FD_GET(handler) (*(int*)(handler))
#define __SPE_EPOLL_FD_SET(handler, fd) (*(int*)(handler) = (fd))

#define __SPE_EVENT_CONTEXT_PRIV_GET(spe) \
  ( (spe_context_event_priv_ptr_t)(spe)->event_private)
#define __SPE_EVENT_CONTEXT_PRIV_SET(spe, evctx) \
  ( (spe)->event_private = (evctx) )

#define __SPE_EVENTS_ENABLED(spe) \
  ((spe)->base_private->flags & SPE_EVENTS_ENABLE)


void _event_spe_context_lock(spe_context_ptr_t spe)
{
  pthread_mutex_lock(&__SPE_EVENT_CONTEXT_PRIV_GET(spe)->lock);
}

void _event_spe_context_unlock(spe_context_ptr_t spe)
{
  pthread_mutex_unlock(&__SPE_EVENT_CONTEXT_PRIV_GET(spe)->lock);
}

static void stop_event_lock(spe_context_event_priv_ptr_t evctx)
{
  pthread_mutex_lock(&evctx->stop_event_lock);
}

static void stop_event_unlock(spe_context_event_priv_ptr_t evctx)
{
  pthread_mutex_unlock(&evctx->stop_event_lock);
}

static int stop_event_pipe_close(spe_context_event_priv_ptr_t evctx)
{
  if (evctx->stop_event_pipe[0] != -1) {
    close(evctx->stop_event_pipe[0]);
    evctx->stop_event_pipe[0] = -1;
  }
  if (evctx->stop_event_pipe[1] != -1) {
    close(evctx->stop_event_pipe[1]);
    evctx->stop_event_pipe[1] = -1;
  }

  return 0;
}

static int stop_event_pipe_open(spe_context_event_priv_ptr_t evctx)
{
  int rc;

  rc = pipe(evctx->stop_event_pipe);
  if (rc == -1) {
    return -1;
  }
  rc = fcntl(evctx->stop_event_pipe[0], F_GETFL);
  if (rc != -1) {
    rc = fcntl(evctx->stop_event_pipe[0], F_SETFL, rc | O_NONBLOCK);
  }
  if (rc == -1) {
    stop_event_pipe_close(evctx);
    return -1;
  }

  return 0;
}

static int stop_event_pipe_acquire(spe_context_event_priv_ptr_t evctx)
{
  if (evctx->stop_event_handler_count == 0) {
    if (stop_event_pipe_open(evctx) == -1) {
      return -1;
    }
    evctx->stop_event_buffer_count = 0; /* invalidate the buffer */
  }

  evctx->stop_event_handler_count++;

  return 0;
}

static int stop_event_pipe_release(spe_context_event_priv_ptr_t evctx)
{
  evctx->stop_event_handler_count--;

  if (evctx->stop_event_handler_count == 0) {
    stop_event_pipe_close(evctx);
  }

  return 0;
}

int _event_spe_stop_info_read (spe_context_ptr_t spe, spe_stop_info_t *stopinfo)
{
  spe_context_event_priv_ptr_t evctx;
  int rc;
  int fd;
  size_t total;
  
  evctx = __SPE_EVENT_CONTEXT_PRIV_GET(spe);

  stop_event_lock(evctx); /* for atomic read */

  fd = evctx->stop_event_pipe[0];

  if (fd == -1) { /* no stop event handler */
    if (evctx->stop_event_buffer_count) {
      /* return the last stop info for backward compatibility */
      memcpy(stopinfo, &evctx->stop_event_buffer, sizeof(*stopinfo));
      evctx->stop_event_buffer_count--;
      rc = 0;
    }
    else {
      /* there is no valid stop info in the buffer */
      errno = EAGAIN;
      rc = -1;
    }
    stop_event_unlock(evctx);
    return rc;
  }

  /* any stop event handler. */

  rc = read(fd, stopinfo, sizeof(*stopinfo));
  if (rc == -1) {
    stop_event_unlock(evctx);
    return -1;
  }

  total = rc;
  while (total < sizeof(*stopinfo)) { /* this loop will be executed in few cases */
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;
    rc = poll(&fds, 1, -1);
    if (rc == -1) {
      if (errno != EINTR) {
	break;
      }
    }
    else if (rc == 1) {
      rc = read(fd, (char *)stopinfo + total, sizeof(*stopinfo) - total);
      if (rc == -1) {
	if (errno != EAGAIN) {
	  break;
	}
      }
      else {
	total += rc;
      }
    }
  }

  stop_event_unlock(evctx);

  return rc == -1 ? -1 : 0;
}

/* 
 * spe_event_handler_create
 */
 
spe_event_handler_ptr_t _event_spe_event_handler_create(void)
{
  int epfd;
  spe_event_handler_t *evhandler;

  evhandler = calloc(1, sizeof(*evhandler));
  if (!evhandler) {
    return NULL;
  }

  epfd = epoll_create(__SPE_EPOLL_SIZE);
  if (epfd == -1) {
    free(evhandler);
    return NULL;
  }

  __SPE_EPOLL_FD_SET(evhandler, epfd);

  return evhandler;
}

/*
 * spe_event_handler_destroy
 */
 
int _event_spe_event_handler_destroy (spe_event_handler_ptr_t evhandler)
{
  int epfd;
  
  if (!evhandler) {
    errno = ESRCH;
    return -1;
  }

  epfd = __SPE_EPOLL_FD_GET(evhandler);
  close(epfd);

  free(evhandler);
  return 0;
}

/*
 * spe_event_handler_register
 */
 
int _event_spe_event_handler_register(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event)
{
  int epfd;
  const int ep_op = EPOLL_CTL_ADD;
  spe_context_event_priv_ptr_t evctx;
  spe_event_unit_t *ev_buf;
  struct epoll_event ep_event;
  int fd;

  if (!evhandler) {
    errno = ESRCH;
    return -1;
  }
  if (!event || !event->spe) {
    errno = EINVAL;
    return -1;
  }
  if (!__SPE_EVENTS_ENABLED(event->spe)) {
    errno = ENOTSUP;
    return -1;
  }

  epfd = __SPE_EPOLL_FD_GET(evhandler);
  evctx = __SPE_EVENT_CONTEXT_PRIV_GET(event->spe);

  if (event->events & ~__SPE_EVENT_ALL) {
    errno = ENOTSUP;
    return -1;
  }

  _event_spe_context_lock(event->spe); /* for spe->event_private->events */

  if (event->events & SPE_EVENT_OUT_INTR_MBOX) {
    fd = __base_spe_event_source_acquire(event->spe, FD_IBOX);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    
    ev_buf = &evctx->events[__SPE_EVENT_OUT_INTR_MBOX];
    ev_buf->events = SPE_EVENT_OUT_INTR_MBOX;
    ev_buf->data = event->data;
    
    ep_event.events = EPOLLIN;
    ep_event.data.ptr = ev_buf;
    if (epoll_ctl(epfd, ep_op, fd, &ep_event) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
  }
  
  if (event->events & SPE_EVENT_IN_MBOX) {
    fd = __base_spe_event_source_acquire(event->spe, FD_WBOX);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    
    ev_buf = &evctx->events[__SPE_EVENT_IN_MBOX];
    ev_buf->events = SPE_EVENT_IN_MBOX;
    ev_buf->data = event->data;
    
    ep_event.events = EPOLLOUT;
    ep_event.data.ptr = ev_buf;
    if (epoll_ctl(epfd, ep_op, fd, &ep_event) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
  }

  if (event->events & SPE_EVENT_TAG_GROUP) {
    fd = __base_spe_event_source_acquire(event->spe, FD_MFC);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }

    if (event->spe->base_private->flags & SPE_MAP_PS) {
	    _event_spe_context_unlock(event->spe);
	    errno = ENOTSUP;
	    return -1;
    }
    
    ev_buf = &evctx->events[__SPE_EVENT_TAG_GROUP];
    ev_buf->events = SPE_EVENT_TAG_GROUP;
    ev_buf->data = event->data;
    
    ep_event.events = EPOLLIN;
    ep_event.data.ptr = ev_buf;
    if (epoll_ctl(epfd, ep_op, fd, &ep_event) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
  }

  if (event->events & SPE_EVENT_SPE_STOPPED) {
    /* prevent reading stop info while registering */
    stop_event_lock(evctx);

    if (stop_event_pipe_acquire(evctx) == -1) {
      stop_event_unlock(evctx);
      _event_spe_context_unlock(event->spe);
      return -1;
    }

    fd = evctx->stop_event_pipe[0];
    
    ev_buf = &evctx->events[__SPE_EVENT_SPE_STOPPED];
    ev_buf->events = SPE_EVENT_SPE_STOPPED;
    ev_buf->data = event->data;
    
    ep_event.events = EPOLLIN;
    ep_event.data.ptr = ev_buf;
    if (epoll_ctl(epfd, ep_op, fd, &ep_event) == -1) {
      stop_event_pipe_release(evctx);
      stop_event_unlock(evctx);
      _event_spe_context_unlock(event->spe);
      return -1;
    }

    stop_event_unlock(evctx);
  }

  _event_spe_context_unlock(event->spe);

  return 0;
}
/*
 * spe_event_handler_deregister
 */

int _event_spe_event_handler_deregister(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event)
{
  int epfd;
  const int ep_op = EPOLL_CTL_DEL;
  spe_context_event_priv_ptr_t evctx;
  int fd;

  if (!evhandler) {
    errno = ESRCH;
    return -1;
  }
  if (!event || !event->spe) {
    errno = EINVAL;
    return -1;
  }
  if (!__SPE_EVENTS_ENABLED(event->spe)) {
    errno = ENOTSUP;
    return -1;
  }

  epfd = __SPE_EPOLL_FD_GET(evhandler);
  evctx = __SPE_EVENT_CONTEXT_PRIV_GET(event->spe);

  if (event->events & ~__SPE_EVENT_ALL) {
    errno = ENOTSUP;
    return -1;
  }

  _event_spe_context_lock(event->spe); /* for spe->event_private->events */
  
  if (event->events & SPE_EVENT_OUT_INTR_MBOX) {
    fd = __base_spe_event_source_acquire(event->spe, FD_IBOX);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    if (epoll_ctl(epfd, ep_op, fd, NULL) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    evctx->events[__SPE_EVENT_OUT_INTR_MBOX].events = 0;
  }
  
  if (event->events & SPE_EVENT_IN_MBOX) {
    fd = __base_spe_event_source_acquire(event->spe, FD_WBOX);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    if (epoll_ctl(epfd, ep_op, fd, NULL) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    evctx->events[__SPE_EVENT_IN_MBOX].events = 0;
  }
  
  if (event->events & SPE_EVENT_TAG_GROUP) {
    fd = __base_spe_event_source_acquire(event->spe, FD_MFC);
    if (fd == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    if (epoll_ctl(epfd, ep_op, fd, NULL) == -1) {
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    evctx->events[__SPE_EVENT_TAG_GROUP].events = 0;
  }
  
  if (event->events & SPE_EVENT_SPE_STOPPED) {
    /* prevent reading stop info while unregistering */
    stop_event_lock(evctx);

    fd = evctx->stop_event_pipe[0];
    if (epoll_ctl(epfd, ep_op, fd, NULL) == -1) {
      stop_event_unlock(evctx);
      _event_spe_context_unlock(event->spe);
      return -1;
    }
    evctx->events[__SPE_EVENT_SPE_STOPPED].events = 0;

    stop_event_pipe_release(evctx);

    stop_event_unlock(evctx);
  }

  _event_spe_context_unlock(event->spe);

  return 0;
}

/*
 * spe_event_wait
 */
 
int _event_spe_event_wait(spe_event_handler_ptr_t evhandler, spe_event_unit_t *events, int max_events, int timeout)
{
  int epfd;
  struct epoll_event *ep_events;
  int rc;
  
  if (!evhandler) {
    errno = ESRCH;
    return -1;
  }
  if (!events || max_events <= 0) {
    errno = EINVAL;
    return -1;
  }
  
  epfd = __SPE_EPOLL_FD_GET(evhandler);

  ep_events = malloc(sizeof(*ep_events) * max_events);
  if (!ep_events) {
    return -1;
  }

  for ( ; ; ) {
    rc = epoll_wait(epfd, ep_events, max_events, timeout);
    if (rc == -1) { /* error */
      if (errno == EINTR) {
	if (timeout >= 0) { /* behave as timeout */
	  rc = 0;
	  break;
	}
	/* else retry */
      }
      else {
	break;
      }
    }
    else if (rc > 0) {
      int i;
      for (i = 0; i < rc; i++) {
	spe_event_unit_t *ev = (spe_event_unit_t *)(ep_events[i].data.ptr);
	_event_spe_context_lock(ev->spe); /* lock ev itself */
	events[i] = *ev;
	_event_spe_context_unlock(ev->spe);
      }
      break;
    }
    else { /* timeout */
      break;
    }
  }

  free(ep_events);

  return rc;
}

int _event_spe_context_finalize(spe_context_ptr_t spe)
{
  spe_context_event_priv_ptr_t evctx;

  if (!spe) {
    errno = ESRCH;
    return -1;
  }

  evctx = __SPE_EVENT_CONTEXT_PRIV_GET(spe);
  __SPE_EVENT_CONTEXT_PRIV_SET(spe, NULL);

  stop_event_pipe_close(evctx);

  pthread_mutex_destroy(&evctx->lock);
  pthread_mutex_destroy(&evctx->stop_event_lock);

  free(evctx);

  return 0;
}

struct spe_context_event_priv * _event_spe_context_initialize(spe_context_ptr_t spe)
{
  spe_context_event_priv_ptr_t evctx;
  int i;

  evctx = calloc(1, sizeof(*evctx));
  if (!evctx) {
    return NULL;
  }

  /* the pipe will be created when any stop event handler is registered */
  evctx->stop_event_pipe[0] = -1;
  evctx->stop_event_pipe[1] = -1;

  for (i = 0; i < sizeof(evctx->events) / sizeof(evctx->events[0]); i++) {
    evctx->events[i].spe = spe;
  }

  pthread_mutex_init(&evctx->lock, NULL);
  pthread_mutex_init(&evctx->stop_event_lock, NULL);

  return evctx;
}

int _event_spe_context_run	(spe_context_ptr_t spe, unsigned int *entry, unsigned int runflags, void *argp, void *envp, spe_stop_info_t *stopinfo)
{
  spe_context_event_priv_ptr_t evctx;
  spe_stop_info_t stopinfo_buf;
  int rc;
  int errno_saved;
  int fd;

  if (!stopinfo) {
    stopinfo = &stopinfo_buf;
  }
  rc = _base_spe_context_run(spe, entry, runflags, argp, envp, stopinfo);
  errno_saved = errno;

  evctx = __SPE_EVENT_CONTEXT_PRIV_GET(spe);

  stop_event_lock(evctx);

  fd = evctx->stop_event_pipe[1];
  /* don't write stop info to the pipe if no stop event handler is registered */
  if (fd == -1) {
    /* store the last stop info in the internal buffer for backward
     * compatibility */
    memcpy(&evctx->stop_event_buffer, stopinfo, sizeof(*stopinfo));
    evctx->stop_event_buffer_count = 1; /* overwrite the buffer */
    stop_event_unlock(evctx);
    return rc;
  }

  stop_event_pipe_acquire(evctx); /* to avoid closing the pipe after unlocked */
  stop_event_unlock(evctx); /* unlock here to avoid deadlocks */

  if (write(fd, stopinfo, sizeof(*stopinfo)) != sizeof(*stopinfo)) {
    /* error check. */
  }

  /* release the pipe */
  stop_event_lock(evctx);
  stop_event_pipe_release(evctx);
  stop_event_unlock(evctx);

  errno = errno_saved;

  return rc;
}

