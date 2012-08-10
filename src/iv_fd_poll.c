/*
 * ivykis, an event handling library
 * Copyright (C) 2002, 2003, 2009 Lennert Buytenhek
 * Dedicated to Marija Kulikova.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License version 2.1 along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/poll.h>
#include "iv_private.h"
#include "iv_fd_private.h"

static int iv_fd_poll_init(struct iv_state *st)
{
	st->u.poll.pfds = malloc(maxfd * sizeof(struct pollfd));
	if (st->u.poll.pfds == NULL)
		return -1;

	st->u.poll.fds = malloc(maxfd * sizeof(struct iv_fd_ *));
	if (st->u.poll.fds == NULL) {
		free(st->u.poll.pfds);
		return -1;
	}

	st->u.poll.num_regd_fds = 0;

	return 0;
}

static void iv_fd_poll_poll(struct iv_state *st,
			    struct iv_list_head *active, struct timespec *to)
{
	int ret;
	int i;

#if _AIX
	/*
	 * AIX sometimes leaves errno uninitialized even if poll
	 * returns -1.
	 */
	errno = EINTR;
#endif

	ret = poll(st->u.poll.pfds, st->u.poll.num_regd_fds,
		   1000 * to->tv_sec + ((to->tv_nsec + 999999) / 1000000));
	if (ret < 0) {
		if (errno == EINTR)
			return;

		iv_fatal("iv_fd_poll_poll: got error %d[%s]", errno,
			 strerror(errno));
	}

	for (i = 0; i < st->u.poll.num_regd_fds; i++) {
		struct iv_fd_ *fd;
		int revents;

		fd = st->u.poll.fds[i];
		revents = st->u.poll.pfds[i].revents;

		if (revents & (POLLIN | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKIN);

		if (revents & (POLLOUT | POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKOUT);

		if (revents & (POLLERR | POLLHUP))
			iv_fd_make_ready(active, fd, MASKERR);
	}
}

static void iv_fd_poll_register_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	fd->u.index = -1;
}

static int bits_to_poll_mask(int bits)
{
	int mask;

	mask = 0;
	if (bits & MASKIN)
		mask |= POLLIN | POLLHUP;
	if (bits & MASKOUT)
		mask |= POLLOUT | POLLHUP;
	if (bits & MASKERR)
		mask |= POLLHUP;

	return mask;
}

static void iv_fd_poll_notify_fd(struct iv_state *st, struct iv_fd_ *fd)
{
	if (fd->registered_bands == fd->wanted_bands)
		return;

	if (fd->u.index == -1 && fd->wanted_bands) {
		fd->u.index = st->u.poll.num_regd_fds++;
		st->u.poll.pfds[fd->u.index].fd = fd->fd;
		st->u.poll.pfds[fd->u.index].events =
			bits_to_poll_mask(fd->wanted_bands);
		st->u.poll.fds[fd->u.index] = fd;
	} else if (fd->u.index != -1 && !fd->wanted_bands) {
		if (fd->u.index != st->u.poll.num_regd_fds - 1) {
			struct iv_fd_ *last;

			st->u.poll.pfds[fd->u.index] =
				st->u.poll.pfds[st->u.poll.num_regd_fds - 1];

			last = st->u.poll.fds[st->u.poll.num_regd_fds - 1];
			last->u.index = fd->u.index;
			st->u.poll.fds[fd->u.index] = last;
		}
		st->u.poll.num_regd_fds--;

		fd->u.index = -1;
	} else {
		st->u.poll.pfds[fd->u.index].events =
			bits_to_poll_mask(fd->wanted_bands);
	}

	fd->registered_bands = fd->wanted_bands;
}

static int iv_fd_poll_notify_fd_sync(struct iv_state *st, struct iv_fd_ *fd)
{
	struct pollfd pfd;
	int ret;

	pfd.fd = fd->fd;
	pfd.events = POLLIN | POLLOUT | POLLHUP;

	do {
		ret = poll(&pfd, 1, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0 || (pfd.revents & POLLNVAL))
		return -1;

	iv_fd_poll_notify_fd(st, fd);

	return 0;
}

static void iv_fd_poll_deinit(struct iv_state *st)
{
	free(st->u.poll.fds);
	free(st->u.poll.pfds);
}


struct iv_fd_poll_method iv_fd_poll_method_poll = {
	.name		= "poll",
	.init		= iv_fd_poll_init,
	.poll		= iv_fd_poll_poll,
	.register_fd	= iv_fd_poll_register_fd,
	.notify_fd	= iv_fd_poll_notify_fd,
	.notify_fd_sync	= iv_fd_poll_notify_fd_sync,
	.deinit		= iv_fd_poll_deinit,
};
