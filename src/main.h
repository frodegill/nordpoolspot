#ifndef _MAIN_H_
#define _MAIN_H_

#include <memory>
#include <cstdlib>
#include <restbed>

#include "setup.h"


void log_arg(FILE* stream, const char* format, va_list& arguments);

namespace nordpoolspot {

void log(FILE* stream, const char* format, ...);

void nowAsString(char* buf, const size_t& buf_len);
void closeConnection(const std::shared_ptr<restbed::Session> session, int response_status, const std::string& response_body);

} // namespace nordpoolspot

#endif // _MAIN_H_
