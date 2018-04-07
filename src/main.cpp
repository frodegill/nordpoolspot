#include <iostream>
#include <string>

#include <Poco/Net/NetSSL.h>
#include <Poco/Net/HTTPStreamFactory.h>
#include <Poco/Net/HTTPSStreamFactory.h>
#include "restbed/custom_logger.hpp"
#include "config_handler.h"
#include "spot_handler.h"


class SSLInitializer
{
public:
	SSLInitializer() {Poco::Net::initializeNetwork();
                    Poco::Net::initializeSSL();
                    Poco::Net::HTTPStreamFactory::registerFactory();
	                  Poco::Net::HTTPSStreamFactory::registerFactory();}
	~SSLInitializer() {Poco::Net::uninitializeSSL();
                     Poco::Net::uninitializeNetwork();
    	               Poco::Net::HTTPStreamFactory::unregisterFactory();
	                   Poco::Net::HTTPSStreamFactory::unregisterFactory();}
};


void log_arg(FILE* stream, const char* format, va_list& arguments)
{
  char date_str[100];
  nordpoolspot::nowAsString(date_str, sizeof(date_str)/sizeof(date_str[0]));
  fprintf(stream, "%s: ", date_str);

  vfprintf(stream, format, arguments);
  fprintf(stream, "\n");
  fflush(stream);
}

void nordpoolspot::log(FILE* stream, const char* format, ...)
{
  va_list arguments;
  va_start(arguments, format);

  log_arg(stream, format, arguments);

  va_end(arguments);
}

void nordpoolspot::nowAsString(char* buf, const size_t& buf_len)
{
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  ::strftime(buf, buf_len-1, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

void nordpoolspot::closeConnection(const std::shared_ptr<restbed::Session> session, int response_status, const std::string& response_body)
{
	char date_str[100];
	nordpoolspot::nowAsString(date_str, sizeof(date_str)/sizeof(date_str[0]));
	session->close(response_status, response_body,
								 {{"Server", "nordpool spot munin node"},
								  {"Date", date_str},
								  {"Content-Type", "text/plain; charset=utf-8"},
								  {"Content-Length", std::to_string(response_body.size())}});
}

int main ()
{
  SSLInitializer ssl_initializer;

  auto config_resource = std::make_shared<restbed::Resource>();
  config_resource->set_path("config");
  config_resource->set_method_handler("GET", nordpoolspot::config_handler);

  auto spot_resource = std::make_shared<restbed::Resource>();
  spot_resource->set_path("values");
  spot_resource->set_method_handler("GET", nordpoolspot::spot_handler);

#ifdef SECURE
  auto ssl_settings = std::make_shared<restbed::SSLSettings>();
  ssl_settings->set_port(nordpoolspot::REST_PORT);
  ssl_settings->set_http_disabled(true);
  ssl_settings->set_sslv2_enabled(false);
  ssl_settings->set_sslv3_enabled(false);
  ssl_settings->set_tlsv1_enabled(true);
  ssl_settings->set_tlsv11_enabled(true);
  ssl_settings->set_tlsv12_enabled(true);
  ssl_settings->set_private_key(restbed::Uri("file://gill-roxrud.dyndns.org.key"));
  ssl_settings->set_certificate_chain(restbed::Uri("file://fullchain.cer"));
#endif

  auto settings = std::make_shared<restbed::Settings>();
  settings->set_root("nordpoolspot");
  settings->set_connection_timeout(std::chrono::seconds(10));
  settings->set_worker_limit(nordpoolspot::WORKER_THREADS);
#ifdef SECURE
  settings->set_ssl_settings(ssl_settings);
#else
  settings->set_port(nordpoolspot::REST_PORT);
#endif
  settings->set_default_header("Connection", "close");

  restbed::Service service;
  service.publish(config_resource);
  service.publish(spot_resource);
  service.set_logger(make_shared<CustomLogger>());

  nordpoolspot::log(stdout, "nordpoolspot munin node operational.\n");

#ifdef RUN_TEST
  nordpoolspot::testSpotHandler();
#else
  service.start(settings);
#endif
  
  return EXIT_SUCCESS;
}
