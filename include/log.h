#ifndef __LOG_H__
#define __LOG_H__

#pragma once

#include <time.h>

#define LOG_TIME(fp)								\
{													\
	time_t ct = time(0);							\
	struct tm ctm;									\
													\
	if(!localtime_s(&ctm, &ct))						\
	{												\
		fprintf(fp, "%02d-%02d %02d:%02d:%02d: ",	\
			ctm.tm_mon + 1,							\
			ctm.tm_mday,							\
			ctm.tm_hour,							\
			ctm.tm_min,								\
			ctm.tm_sec);							\
	}												\
}													\

#define TRACEN(...)						\
LOG_TIME(stderr);						\
{										\
	fprintf(stderr, __VA_ARGS__);		\
	fprintf(stderr, "\n");				\
}										\

#define TRACE(...)						\
fprintf(stderr, __VA_ARGS__);			\


#endif