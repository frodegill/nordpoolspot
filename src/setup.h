#ifndef _SETUP_H_
#define _SETUP_H_

#include <stdint.h>


namespace nordpoolspot {

// Enable to run test instead of starting REST API endpoint (useful for debug/testing)
//#define RUN_TEST

// Enable to disable date checks in parsed spot price files (useful when debugging/testing)
//#define SKIP_DATE_CHECK
  
// Enable to parse local file instead of downloading from internet (useful for debug/testing)
//#define DEBUG_LOCAL_FILE

// Enable verbose debugging
//#define VERBOSE_LOG


// REST API endpoint
static const uint8_t WORKER_THREADS = 4;
static const uint16_t REST_PORT= 1417;

// Enable to serve REST API endpoint over SSL/TLS
//#define SECURE


#ifdef DBG
# define VERBOSE_LOG
# undef SECURE
#endif

// Useful constants 
static const uint8_t EQUAL = 0;

} // namespace nordpoolspot

#endif // _SETUP_H_
