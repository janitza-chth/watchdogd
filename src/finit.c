/* Finit (PID 1) API
 *
 * Copyright (C) 2017-2020  Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <paths.h>
#include <poll.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <finit/finit.h>

#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "wdt.h"


int wdt_register(void)
{
	static int already = 0;
	struct init_request rq = {
		.magic    = INIT_MAGIC,
		.cmd      = INIT_CMD_WDOG_HELLO,
		.runlevel = getpid(),
	};
	struct sockaddr_un sun;
	struct pollfd pfd;
	size_t len;
	int sd, rc = -1, retry = 3;

	if (already) {
		DEBUG("No need to handover to Finit again.");
		return 0;
	}

	sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == sd)
		return -1;

	/*
	 * Try connecting to Finit, we should get a reply immediately,
	 * if nobody is at home we close the connection and continue.
	 */
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, INIT_SOCKET, sizeof(sun.sun_path));
	while (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1) {
		if (retry-- == 0)
			goto err;
		sleep(1);
	}

	len = sizeof(rq);
	pfd.fd = sd;
	pfd.events = POLLOUT;
	if (poll(&pfd, 1, 3000) <= 0)
		goto err;

	if (write(sd, &rq, len) != (ssize_t)len)
		goto err;

	pfd.events = POLLIN;
	if (poll(&pfd, 1, 3000) <= 0)
		goto err;

	if (read(sd, &rq, len) != (ssize_t)len)
		goto err;

	if (rq.cmd == INIT_CMD_ACK)
		rc = 0;

	if (!rc)
		already = 1;
err:
	close(sd);

	return rc;
}

/*
 * Communicate WDT ownership handover to Finit
 */
int wdt_handover(const char *devnode)
{
	int retries = 3;
	int rc = -1;

	DEBUG("Attempting WDT handover with Finit ...");
	if (wdt_register())
		return -1;

	/*
	 * Don't give up immediately, give the current
	 * daemon time to exit and the kernel time to
	 * close the WDT device.
	 */
	while (rc < 0 && retries--) {
		rc = open(devnode, O_WRONLY);
		if (rc >= 0)
			break;

		sleep(1);
	}

	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
