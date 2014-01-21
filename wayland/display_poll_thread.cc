// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/wayland/display_poll_thread.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <wayland-client.h>

#include "base/bind.h"
#include "ozone/wayland/display.h"

namespace ozonewayland {
const int MAX_EVENTS = 16;
// os-compatibility
extern "C" {
int osEpollCreateCloExec(void);

static int setCloExecOrClose(int fd) {
  long flags;

  if (fd == -1)
    return -1;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    goto err;

  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    goto err;

  return fd;

  err:
    close(fd);
    return -1;
}

int osEpollCreateCloExec(void) {
  int fd;

#ifdef EPOLL_CLOEXEC
  fd = epoll_create1(EPOLL_CLOEXEC);
  if (fd >= 0)
    return fd;
  if (errno != EINVAL)
    return -1;
#endif

  fd = epoll_create(1);
  return setCloExecOrClose(fd);
}
}  // os-compatibility

WaylandDisplayPollThread::WaylandDisplayPollThread(wl_display* display)
    : base::Thread("WaylandDisplayPollThread"),
      display_(display),
      active_(false),
      epoll_fd_(0) {
  DCHECK(display_);
}

WaylandDisplayPollThread::~WaylandDisplayPollThread() {
  StopProcessingEvents();
  Stop();
}

void WaylandDisplayPollThread::StartProcessingEvents() {
  DCHECK(!active_ && !epoll_fd_);
  epoll_fd_ = osEpollCreateCloExec();
  DCHECK(epoll_fd_ > 0) << "Epoll creation failed.";
  struct epoll_event ep;
  ep.events = EPOLLIN | EPOLLOUT;
  ep.data.ptr = 0;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wl_display_get_fd(display_), &ep) < 0)
    LOG(ERROR) << "epoll_ctl Add failed";

  active_ = true;
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  StartWithOptions(options);
  SetPriority(base::kThreadPriority_Background);
  message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &WaylandDisplayPollThread::DisplayRun, this));
}

void WaylandDisplayPollThread::StopProcessingEvents() {
  active_ = false;
  if (epoll_fd_) {
    close(epoll_fd_);
    epoll_fd_ = 0;
  }
}

void  WaylandDisplayPollThread::DisplayRun(WaylandDisplayPollThread* data) {
  struct epoll_event ep[MAX_EVENTS];
  int i, count, ret;
  unsigned display_fd = wl_display_get_fd(data->display_);
  // Adopted from:
  // http://cgit.freedesktop.org/wayland/weston/tree/clients/window.c#n5531.
  while (1) {
    wl_display_dispatch_pending(data->display_);

    if (!data->active_)
      break;

    ret = wl_display_flush(data->display_);
    if (ret < 0 && errno == EAGAIN) {
      ep[0].events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
      epoll_ctl(data->epoll_fd_, EPOLL_CTL_MOD, display_fd, &ep[0]);
    } else if (ret < 0) {
      break;
    }

    count = epoll_wait(data->epoll_fd_, ep, MAX_EVENTS, -1);
    if (!data->active_)
      break;

    for (i = 0; i < count; i++) {
      int ret;
      uint32_t event = ep[i].events;

      if (event & EPOLLERR || event & EPOLLHUP)
        return;

      if (event & EPOLLIN) {
        ret = wl_display_dispatch(data->display_);
        if (ret == -1)
          return;
      }

      if (event & EPOLLOUT) {
        ret = wl_display_flush(data->display_);
        if (ret == 0) {
          struct epoll_event eps;
          memset(&eps, 0, sizeof(eps));

          eps.events = EPOLLIN | EPOLLERR | EPOLLHUP;
          epoll_ctl(data->epoll_fd_, EPOLL_CTL_MOD, display_fd, &eps);
        } else if (ret == -1 && errno != EAGAIN) {
          return;
        }
      }
    }
  }
}

}  // namespace ozonewayland