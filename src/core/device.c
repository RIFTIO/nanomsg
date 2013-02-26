/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#if defined NN_HAVE_POLL

#include "../nn.h"

#include "../utils/err.h"
#include "../utils/fast.h"

#include <poll.h>
#include <string.h>

/*  Private functions. */
static void nn_device_getfds (int s, int *rcvfd, int *sndfd);
static int nn_device_mvmsg (int from, int to);

int nn_device (int s1, int s2)
{
    int rc;
    int i;
    struct pollfd pfd [4];

    /*  Initialise the pollset. */
    nn_device_getfds (s1, &pfd [0].fd, &pfd [1].fd);
    nn_device_getfds (s2, &pfd [2].fd, &pfd [3].fd);
    for (i = 0; i != 4; ++i)
        pfd [i].events = POLLIN;

    while (1) {

        /*  Wait for network events. */
        rc = poll (pfd, 4, -1);
        errno_assert (rc >= 0);
        if (nn_slow (rc < 0 && errno == EINTR))
            return -1;
        nn_assert (rc != 0);

        /*  Process the events. When the event is received, we cease polling
            for it. */
        if (pfd [0].revents & POLLIN)
            pfd [0].events = 0;
        if (pfd [1].revents & POLLIN)
            pfd [1].events = 0;
        if (pfd [2].revents & POLLIN)
            pfd [2].events = 0;
        if (pfd [3].revents & POLLIN)
            pfd [3].events = 0;

        /*  If possible, pass the message from s1 to s2. */
        if (pfd [0].events == 0 && pfd [3].events == 0) {
            rc = nn_device_mvmsg (s1, s2);
            if (nn_slow (rc < 0))
                return -1;
            pfd [0].events = POLLIN;
            pfd [3].events = POLLIN;
        }

        /*  If possible, pass the message from s2 to s1. */
        if (pfd [2].events == 0 && pfd [1].events == 0) {
            rc = nn_device_mvmsg (s2, s1);
            if (nn_slow (rc < 0))
                return -1;
            pfd [2].events = POLLIN;
            pfd [1].events = POLLIN;
        }
    }
}

static void nn_device_getfds (int s, int *rcvfd, int *sndfd)
{
    int rc;
    size_t optsz;

    optsz = sizeof (*rcvfd);
    rc = nn_getsockopt (s, NN_SOL_SOCKET, NN_RCVFD, rcvfd, &optsz);
    nn_assert (rc == 0);
    nn_assert (optsz == sizeof (*rcvfd));
    optsz = sizeof (*sndfd);
    rc = nn_getsockopt (s, NN_SOL_SOCKET, NN_SNDFD, sndfd, &optsz);
    nn_assert (rc == 0);
    nn_assert (optsz == sizeof (*sndfd));
}

static int nn_device_mvmsg (int from, int to)
{
    int rc;
    void *body;
    void *control;
    struct nn_iovec iov;
    struct nn_msghdr hdr;

    iov.iov_base = &body;
    iov.iov_len = NN_MSG;
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = &control;
    hdr.msg_controllen = NN_MSG;
    rc = nn_recvmsg (from, &hdr, NN_DONTWAIT);
    if (nn_slow (rc < 0 && nn_errno () == ETERM))
        return -1;
    errno_assert (rc >= 0);
    rc = nn_sendmsg (to, &hdr, NN_DONTWAIT);
    if (nn_slow (rc < 0 && nn_errno () == ETERM))
        return -1;
    errno_assert (rc >= 0);
    return 0;
}

#endif
