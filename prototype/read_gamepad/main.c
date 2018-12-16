/*
  read gamepad
  (C) 2018 TANIGUCHI, Kazushige
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <getopt.h>

#include <linux/joystick.h>

#define BULK_OF(x)     (sizeof(x)/sizeof(x[0]))
#define JOYSTICK_PATH  "/dev/input/js0"

static const struct option long_options[] = {
	{"help",    no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL,                0, NULL,  0 }
};

int main(int argc, char **argv){
	int ret;
	sigset_t sigmask;
	int epollfd = -1, sigfd = -1, jsfd = -1;
	struct epoll_event evs[3];
	bool bCnt = true;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);

	ret = sigprocmask(SIG_BLOCK, &sigmask, NULL);
	if(ret != 0){
		fprintf(stderr, "failed to signal mask %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	sigfd = signalfd(-1, &sigmask, 0);
	if(sigfd == -1){
		fprintf(stderr, "failed to create signal fd %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	jsfd = open(JOYSTICK_PATH, O_RDONLY);
	if(jsfd == -1){
		fprintf(stderr, "failed to open joystick fd %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	epollfd = epoll_create1(0);
	if(epollfd == -1){
		fprintf(stderr, "failed to create epoll fd %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	evs[0].events  = EPOLLIN;
	evs[0].data.fd = sigfd;
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &evs[0]);
	if(ret == -1) {
		fprintf(stderr, "failed to add sigfd to epoll fd %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	evs[0].events  = EPOLLIN;
	evs[0].data.fd = jsfd;
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, jsfd, &evs[0]);
	if(ret == -1) {
		fprintf(stderr, "failed to add jsfd to epoll fd %s(%d)\n", strerror(errno), errno);
		ret = 1;
		goto error_exit;
	}

	while(bCnt){
		size_t i;

		ret = epoll_wait(epollfd, &evs[0], BULK_OF(evs), -1);
		if(ret == -1){
			fprintf(stderr, "failed to wait epoll fd %s(%d)\n", strerror(errno), errno);
			ret = 1;
			goto error_exit;
		}
		for(i=0;i<(size_t)ret;i++){
			if(evs[i].data.fd == sigfd){
				ssize_t readsize;
				struct signalfd_siginfo siginfo = { 0 };
				readsize = read(sigfd, &siginfo, sizeof(siginfo));
				if(readsize > 0){
					switch(siginfo.ssi_signo){
					case SIGINT:
					case SIGTERM:
						fprintf(stderr, "recv TERM or INT signal\n");
						bCnt = 0;
						break;
					default:
						break;
					}
				}
			}
			else if(evs[i].data.fd == jsfd){
				struct js_event jsevent;
				ret = read(jsfd, &jsevent, sizeof(jsevent));
				if(ret == -1){
					fprintf(stderr, "failed to read jpystick %s(%d)\n", strerror(errno), errno);
					ret = 1;
					goto error_exit;
				}
				switch(jsevent.type){
				case JS_EVENT_BUTTON:
					printf("button %u: %d\n", jsevent.number, jsevent.value);
					break;
				case JS_EVENT_AXIS:
					printf("axis %u: %d\n", jsevent.number, jsevent.value);
					break;
				default:
					break;
				}
			}
		}
	}

	ret = 0;

error_exit:

	if(jsfd != -1){
		close(jsfd);
	}

	if(sigfd != -1){
		close(sigfd);
	}

	if(epollfd != -1){
		close(epollfd);
	}

	return ret;
}
