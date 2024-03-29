/*
 * Copyright (c) 2005 Christophe Varoqui
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 * Copyright (c) 2005 Jun'ichi Nomura, NEC
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <pthread.h>

#include "log.h"
#include "util.h"

#define ALIGN(len, s) (((len)+(s)-1)/(s)*(s))

struct logarea* la;
static pthread_mutex_t logq_lock = PTHREAD_MUTEX_INITIALIZER;

#if LOGDBG
static void dump_logarea (void)
{
	struct logmsg * msg;

	logdbg(stderr, "\n==== area: start addr = %p, end addr = %p ====\n",
		la->start, la->end);
	logdbg(stderr, "|addr     |next     |prio|msg\n");

	for (msg = (struct logmsg *)la->head; (void *)msg != la->tail;
	     msg = msg->next)
		logdbg(stderr, "|%p |%p |%i   |%s\n", (void *)msg, msg->next,
				msg->prio, (char *)&msg->str);

	logdbg(stderr, "|%p |%p |%i   |%s\n", (void *)msg, msg->next,
			msg->prio, (char *)&msg->str);

	logdbg(stderr, "\n\n");
}
#endif

static int logarea_init (int size)
{
	logdbg(stderr,"enter logarea_init\n");
	la = (struct logarea *)calloc(1, sizeof(struct logarea));

	if (!la)
		return 1;

	if (size < MAX_MSG_SIZE)
		size = DEFAULT_AREA_SIZE;

	la->start = calloc(1, size);
	if (!la->start) {
		free(la);
		la = NULL;
		return 1;
	}

	la->empty = 1;
	la->end = la->start + size;
	la->head = la->start;
	la->tail = la->start;

	la->buff = calloc(1, MAX_MSG_SIZE + sizeof(struct logmsg));

	if (!la->buff) {
		free(la->start);
		free(la);
		la = NULL;
		return 1;
	}
	return 0;

}

int log_init(char *program_name, int size)
{
	int ret = 1;

	logdbg(stderr,"enter log_init\n");

	pthread_mutex_lock(&logq_lock);
	pthread_cleanup_push(cleanup_mutex, &logq_lock);

	openlog(program_name, 0, LOG_DAEMON);
	if (!la)
		ret = logarea_init(size);

	pthread_cleanup_pop(1);

	return ret;
}

static void free_logarea (void)
{
	free(la->start);
	free(la->buff);
	free(la);
	la = NULL;
	return;
}

void log_close (void)
{
	pthread_mutex_lock(&logq_lock);
	pthread_cleanup_push(cleanup_mutex, &logq_lock);

	if (la)
		free_logarea();
	closelog();

	pthread_cleanup_pop(1);
	return;
}

void log_reset (char *program_name)
{
	pthread_mutex_lock(&logq_lock);
	pthread_cleanup_push(cleanup_mutex, &logq_lock);

	closelog();
	openlog(program_name, 0, LOG_DAEMON);

	pthread_cleanup_pop(1);
}

__attribute__((format(printf, 2, 0)))
static int _log_enqueue(int prio, const char * fmt, va_list ap)
{
	int len, fwd;
	char buff[MAX_MSG_SIZE];
	struct logmsg * msg;
	struct logmsg * lastmsg;

	lastmsg = (struct logmsg *)la->tail;

	if (!la->empty) {
		fwd = sizeof(struct logmsg) +
		      strlen((char *)&lastmsg->str) * sizeof(char) + 1;
		la->tail += ALIGN(fwd, sizeof(void *));
	}
	vsnprintf(buff, MAX_MSG_SIZE, fmt, ap);
	len = ALIGN(sizeof(struct logmsg) + strlen(buff) * sizeof(char) + 1,
		    sizeof(void *));

	/* not enough space on tail : rewind */
	if (la->head <= la->tail && len > (la->end - la->tail)) {
		logdbg(stderr, "enqueue: rewind tail to %p\n", la->tail);
		if (la->head == la->start ) {
			logdbg(stderr, "enqueue: cannot rewind tail, drop msg\n");
			la->tail = lastmsg;
			return 1;  /* can't reuse */
		}
		la->tail = la->start;

		if (la->empty)
			la->head = la->start;
	}

	/* not enough space on head : drop msg */
	if (la->head > la->tail && len >= (la->head - la->tail)) {
		logdbg(stderr, "enqueue: log area overrun, drop msg\n");

		if (!la->empty)
			la->tail = lastmsg;

		return 1;
	}

	/* ok, we can stage the msg in the area */
	la->empty = 0;
	msg = (struct logmsg *)la->tail;
	msg->prio = prio;
	memcpy((void *)&msg->str, buff, strlen(buff) + 1);
	lastmsg->next = la->tail;
	msg->next = la->head;

	logdbg(stderr, "enqueue: %p, %p, %i, %s\n", (void *)msg, msg->next,
		msg->prio, (char *)&msg->str);

#if LOGDBG
	dump_logarea();
#endif
	return 0;
}

int log_enqueue(int prio, const char *fmt, va_list ap)
{
	int ret = 1;

	pthread_mutex_lock(&logq_lock);
	pthread_cleanup_push(cleanup_mutex, &logq_lock);
	if (la)
		ret = _log_enqueue(prio, fmt, ap);
	pthread_cleanup_pop(1);
	return ret;
}

static int _log_dequeue(void *buff)
{
	struct logmsg * src = (struct logmsg *)la->head;
	struct logmsg * dst = (struct logmsg *)buff;
	struct logmsg * lst = (struct logmsg *)la->tail;

	if (la->empty)
		return 1;

	int len = strlen((char *)&src->str) * sizeof(char) +
		  sizeof(struct logmsg) + 1;

	dst->prio = src->prio;
	memcpy(dst, src,  len);

	if (la->tail == la->head)
		la->empty = 1; /* we purge the last logmsg */
	else {
		la->head = src->next;
		lst->next = la->head;
	}
	logdbg(stderr, "dequeue: %p, %p, %i, %s\n",
		(void *)src, src->next, src->prio, (char *)&src->str);

	memset((void *)src, 0,  len);

	return 0;
}

int log_dequeue(void *buff)
{
	int ret = 1;

	pthread_mutex_lock(&logq_lock);
	pthread_cleanup_push(cleanup_mutex, &logq_lock);
	if (la)
		ret = _log_dequeue(buff);
	pthread_cleanup_pop(1);
	return ret;
}

/*
 * this one can block under memory pressure
 */
void log_syslog (void * buff)
{
	struct logmsg * msg = (struct logmsg *)buff;

	syslog(msg->prio, "%s", (char *)&msg->str);
}
