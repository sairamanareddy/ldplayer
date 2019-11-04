/*
 * Copyright (C) 2015 by the University of Southern California
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef DBG_H
#define DBG_H

#include <stdio.h>

//#define DEBUG 1
enum { LOG_NONE, LOG_ERR, LOG_WARN, LOG_INFO, LOG_DBG, LOG_ALL};
#ifdef DEBUG

#define ENTER       do { fprintf(stderr, "%s:%s:%-5d: ENTER\n",  __FILE__,__FUNCTION__, __LINE__); } while(0)
#define RET(args)   do { fprintf(stderr, "%s:%s:%-5d: RETURN\n", __FILE__, __FUNCTION__, __LINE__);\
    return args; } while(0)
#define DBG(fmt, args...) fprintf(stderr, "%s:%s:%-5d: " fmt,  __FILE__,__FUNCTION__, __LINE__, ## args)
#define LOG(level, fmt, args... ) fprintf(stderr, fmt, ## args)

#else

extern int log_level;
#define ENTER            do {  } while(0)
#define RET(args)        return args;
#define DBG(fmt, args...)   do {  } while(0)
#define LOG(level, fmt, args...)                                  \
  do {                                                            \
    if (level <= log_level) fprintf(stderr, fmt   , ## args );	  \
    if (level <= log_level) fflush(stderr);			  \
  } while(0)

#endif

#define CHECK(A, M, ...) if(!(A)) { LOG(LOG_ERR, M, ##__VA_ARGS__); errno=0; goto error;}

#endif  //DBG_H
