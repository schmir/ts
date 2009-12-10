/* 
  Copyright (C) 2009 Ralf Schmitt <ralf at systemexit.de>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSIZE (4096)

// --- globals
static char * output_path=0;
static char * dstname=0;
static char * tmpname=0;

static struct timeval now;
static struct timeval tv_start;

static char *cmalloc(size_t size)
{
	char *r = (char *)malloc(size);

	if (!r) {
		perror("malloc failed");
		exit(10);
	}
	return r;
}

static void update_now()
{
	gettimeofday(&now, NULL);
}

static void tvdiff(struct timeval *tv1, struct timeval *tv2, struct timeval *res)
{
	res->tv_sec = tv1->tv_sec- tv2->tv_sec;
	res->tv_usec = tv1->tv_usec - tv2->tv_usec;
	if (res->tv_usec < 0) {
		res->tv_sec -= 1;
		res->tv_usec += 1000000;
	}
}


static void write_absolute_ts(struct timeval *ts)
{
	char buff[32];
	sprintf(buff, "%010ld.%06ld ", (long)ts->tv_sec, (long)ts->tv_usec);
	write(1, buff, 18);
}

static void write_relative_ts(struct timeval *ts)
{
	char buff[32];
	struct timeval diff;
	tvdiff(ts, &tv_start, &diff);
	sprintf(buff, "%010ld.%06ld ", (long)diff.tv_sec, (long)diff.tv_usec);
	write(1, buff, 18);
}

static char *mybasename(char *s)
{
	char *slash=strrchr(s, '/');
	if (!slash) {
		return s;
	}
	return slash+1;
}


static void rotate()
{
	if (!output_path) {
		return;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);

	sprintf(dstname, "%s-%010ld.%06ld", output_path, (long)tv.tv_sec, (long)tv.tv_usec);

	close(1);
	int fd = open(dstname, O_WRONLY | O_APPEND | O_CREAT, 0755);
	if (fd<0) {
		perror("open failed");
	}

	unlink(tmpname);
	symlink(mybasename(dstname), tmpname);

	/* link(dstname, tmpname); */
	rename(tmpname, output_path);

}

void (*write_ts)(struct timeval *ts)=write_absolute_ts;


int main(int argc, char * const argv[])
{
	int maxlines = 10000;
	int c;
	while (-1 != (c=getopt(argc, argv, "+o:n:r"))) {
		switch (c) {
		case 'o':
			output_path = optarg;
			break;
		case 'r':
			write_ts = write_relative_ts;
			break;
		case 'n':
			maxlines = atoi(optarg);
			break;

		default:
			exit(10);
		}
	}

	if (maxlines <= 0) {
		maxlines = 10000;
	}
	
	if (output_path) {
		size_t len = strlen(output_path)+32;
		dstname = cmalloc(len);
		tmpname = cmalloc(len);
		sprintf(tmpname, "%s.tmp", output_path);
	}

	char *buf = cmalloc(BUFSIZE);

	signal(SIGINT , SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGHUP , SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	update_now();
	tv_start = now;

	if (output_path) {
		rotate();
	}

	int timestamp_written=0;
	int linecount = 0;

	ssize_t numread;
	while (0 != (numread=read(0, buf, BUFSIZE))) {
		if (numread<0 && errno==EINTR) {
			continue;
		}
		if (numread<0) {
			perror("ts: read failed");
			exit(10);
		}
		
		update_now();

		char *tmp = buf;
		while (numread>0) {
			char * nl = (char*) memchr(tmp, '\n', numread);
			if (nl) {
				if (!timestamp_written) {
					write_ts(&now);
				}
				timestamp_written = 0;
				write(1, tmp, nl-tmp+1);
				numread -= nl-tmp+1;

				tmp = nl+1;
				linecount += 1;

				if (linecount % maxlines == 0) {
					rotate();
				}
			} else {
				if (!timestamp_written) {
					write_ts(&now);
					timestamp_written = 1;
				}
				write(1, tmp, numread);
				break;
			}
		}
	}

	return 0;
}
