#ifndef _SPOT_HANDLER_H_
#define _SPOT_HANDLER_H_

#include <memory>
#include <restbed>

#include "setup.h"


namespace nordpoolspot {

#ifdef DEBUG_LOCAL_FILE
  static const std::string FILENAME = "nordpoolspot.json";
# define SKIP_DATE_CHECK
#else
  static const std::string SPOT_URL = std::string("https://www.nordpoolgroup.com/api/marketdata/page/10");
  static const std::string EUR_URL = std::string("https://api.exchangeratesapi.io/latest?base=EUR&symbols=NOK");
#endif

static const uint8_t TODAY = 0;
static const uint8_t TOMORROW = 1;

static const uint8_t HOURS = 24;
static const uint8_t ZONES = 5;

static const uint8_t PRICE_PUBLISH_HOUR = 12; //Local Norwegian time
static const uint8_t PRICE_FETCH_DELAY_HOURS = 2; //Give them two hours to publish fetch_prices

static const uint8_t NO1 = 0;
static const uint8_t NO2 = 1;
static const uint8_t NO3 = 2;
static const uint8_t NO4 = 3;
static const uint8_t NO5 = 4;
static const uint8_t UNKNOWN = 255;

static const double INVALID_EUR_RATE = -1.0;

struct SpotPrice {
  uint32_t m_key;
  double m_price[HOURS][ZONES];
};

class SpotPriceCache {
public:
  double get_price(uint8_t zone);

#ifdef RUN_TEST
  void logCache();
#endif
  
private:
  uint32_t to_key(const std::chrono::system_clock::time_point& time);
  double fetch_eur();
  std::unique_ptr<SpotPrice> fetch_prices(const std::chrono::system_clock::time_point& time);

private:
  std::unique_ptr<SpotPrice> m_spotprice_cache[2]; //[0]=today, [1]=tomorrow
};

void spot_handler(const std::shared_ptr<restbed::Session> session);

#ifdef RUN_TEST
  void testSpotHandler();
#endif

} // namespace nordpoolspot

#endif // _SPOT_HANDLER_H_
