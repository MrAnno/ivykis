/*
 * ivykis, an event handling library
 * Copyright (C) 2020 Lennert Buytenhek
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
#include <iv.h>

struct iv_fd fd_stdin;

static void callback(void *dummy)
{
    char buf[1024];
    int len;

    len = read(fd_stdin.fd, buf, sizeof(buf));
    if (len <= 0) {
        if (len < 0) {
            if (errno == EAGAIN)
                return;
            perror("read");
        }
        exit(1);
    }

    printf("read %d bytes of data from stdin\n", len);
}

int main()
{
        iv_init();

        IV_FD_INIT(&fd_stdin);
        fd_stdin.fd = 0;
        fd_stdin.handler_in = callback;
        iv_fd_register(&fd_stdin);
        iv_fd_unregister(&fd_stdin);

        iv_main();

        iv_deinit();

        return 0;
}
