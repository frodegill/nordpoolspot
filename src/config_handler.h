#ifndef _CONFIG_HANDLER_H_
#define _CONFIG_HANDLER_H_

#include <memory>
#include <restbed>

namespace nordpoolspot {

void config_handler(const std::shared_ptr<restbed::Session> session);

} // namespace nordpoolspot

#endif // _CONFIG_HANDLER_H_
