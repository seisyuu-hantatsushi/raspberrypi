/*
  gpio control from pad.
  (C) 2018 TANIGUCHI, Kazushige
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <getopt.h>

#include <linux/joystick.h>

#include "motor_driver.h"

#define BULK_OF(x)     (sizeof(x)/sizeof(x[0]))
#define JOYSTICK_PATH  "/dev/input/js0"
#define PIDFILE "/var/run/motorctrl.pid"

struct AppContext {
	struct MotorDriverCreateParam motorDriverParam;
};

static const struct option long_options[] = {
	{"help",    no_argument,       NULL, 'h'},
	{"version", no_argument,       NULL, 'v'},
	{"maxduty", required_argument, NULL, 'm'},
	{NULL,                0,       NULL,  0 }
};

static int parseArgument(int argc, char **argv, struct AppContext *pAppCtx){
	int ret = 0;
	int c;
	int opt_index = 0;

	for(;;){
		c = getopt_long(argc, argv, "hvm:", &long_options[0], &opt_index);
		if(c == -1){
			break;
		}
		switch(c){
		case 'h':
			break;
		case 'v':
			break;
		case 'm':
			pAppCtx->motorDriverParam.maxDuty = (uint64_t)strtoll(optarg, NULL, 10);
			break;
		default:
			break;
		}
	}

	return ret;
}

static int createPIDFile(){
	int pidfile = -1;
	FILE *fp;

	pidfile = open(PIDFILE,O_CREAT|O_EXCL|O_WRONLY);

	if(pidfile == -1){
		return -1;
	}

	fp = fdopen(pidfile,"w");
	fprintf(fp, "%d", getpid());

	close(pidfile);

	return 0;
}

int main(int argc, char **argv){
	int ret;
	sigset_t sigmask;
	int epollfd = -1, sigfd = -1, jsfd = -1;
	struct epoll_event evs[3];
	bool bCnt = true;
	MotorDriverHandler motorHandler = NULL;
	uint8_t leftYAxis = 1,rightYAxis = 3;
	struct AppContext appCtx;

	ret = createPIDFile();
	if(ret < 0){
		fprintf(stderr, "failed to create pid file\n");
		return 3;
	}

	memset(&appCtx, 0x00, sizeof(appCtx));

	ret = parseArgument(argc, argv, &appCtx);
	if(ret < 0){
		return 1;
	}

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

	ret = motorDriverCreate(&appCtx.motorDriverParam, &motorHandler);
	if(ret < 0){
		ret = 1;
		goto error_exit;
	}

	{
		uint32_t trycount = 0;
		for(;;){
			jsfd = open(JOYSTICK_PATH, O_RDONLY);
			if(jsfd == -1){
				if(trycount<60){
					usleep(10000);
				}
				else {
					fprintf(stderr, "failed to open joystick fd %s(%d)\n", strerror(errno), errno);
					ret = errno;
					goto error_exit;
				}
			}
			else {
				break;
			}
			trycount++;
		}
	}
	printf("get joystick handler: %d\n", jsfd);

	{
		char jsid[128] = { '\0' };
		ret = ioctl(jsfd, JSIOCGNAME(sizeof(jsid)), &jsid);
		//printf("joystick id: %s\n", jsid);
		if(strcmp(jsid,"Sony PLAYSTATION(R)3 Controller") == 0){
			rightYAxis = 4;
		}
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

	evs[0].events  = EPOLLIN|EPOLLERR;
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
				if(evs[i].events & EPOLLERR){
					//printf("recv error of jsfd\n");
					ret = 1;
					goto error_exit;
				}
				if(evs[i].events & EPOLLIN){
					struct js_event jsevent;
					ret = read(jsfd, &jsevent, sizeof(jsevent));
					if(ret == -1){
						fprintf(stderr, "failed to read jpystick %s(%d)\n", strerror(errno), errno);
						ret = 1;
						goto error_exit;
					}
					switch(jsevent.type){
					case JS_EVENT_BUTTON:
						//printf("button %u: %d\n", jsevent.number, jsevent.value);
						break;
					case JS_EVENT_AXIS:
						//printf("axis %u: %d\n", jsevent.number, jsevent.value);
						if(jsevent.number == leftYAxis){
							motorDriverMotorAccel(motorHandler, 0, jsevent.value);
						}
						else if(jsevent.number == rightYAxis){
							motorDriverMotorAccel(motorHandler, 1, jsevent.value);
						}
						else {
							;
						}
						break;
					default:
						break;
					}
				}
			}
		}
	}
	ret = 0;

error_exit:

	if(jsfd != -1){
		close(jsfd);
	}

	if(motorHandler != NULL){
		motorDriverDestroy(motorHandler);
	}

	if(sigfd != -1){
		close(sigfd);
	}

	if(epollfd != -1){
		close(epollfd);
	}

	unlink(PIDFILE);
	
	return ret;
}
