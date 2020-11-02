#include "spot_handler.h"

#include <Poco/JSON/Parser.h>
#ifdef DEBUG_LOCAL_FILE
# include <Poco/File.h>
# include <Poco/FileStream.h>
# include <Poco/Path.h>
#else
# include <Poco/Net/HTTPClientSession.h>
# include <Poco/Net/HTTPSClientSession.h>
# include <Poco/Net/HTTPRequest.h>
# include <Poco/Net/HTTPResponse.h>
# include <Poco/URI.h>
#endif
#include <Poco/StreamCopier.h>

#include "main.h"


using namespace nordpoolspot;

SpotPriceCache g_spotprice_cache;


double SpotPriceCache::get_price(uint8_t zone)
{
  auto now = std::chrono::system_clock::now();
  uint32_t now_key = to_key(now);
  auto now_t = std::chrono::system_clock::to_time_t(now);
  auto now_local_tm = *localtime(&now_t);

  //If we have value cached in the tomorrow spot, move it to today
  if (m_spotprice_cache[TOMORROW] && m_spotprice_cache[TOMORROW]->m_key==now_key)
  {
#ifdef VERBOSE_LOG
    log(stdout, "Moving TOMORROW to TODAY");
#endif
    m_spotprice_cache[TODAY] = std::move(m_spotprice_cache[TOMORROW]);
  }

  if (!m_spotprice_cache[TODAY] || m_spotprice_cache[TODAY]->m_key!=now_key)
  {
#ifdef VERBOSE_LOG
    log(stdout, "Fetching TODAY");
#endif
    m_spotprice_cache[TODAY] = fetch_prices(now);
    if (!m_spotprice_cache[TODAY] || m_spotprice_cache[TODAY]->m_key!=now_key) //Fetch failed
    {
      log(stdout, "Couldn't find TODAY price list");
      return 0.0;
    }
  }

  if (now_local_tm.tm_hour >= (PRICE_PUBLISH_HOUR+PRICE_FETCH_DELAY_HOURS))
  {
    auto tomorrow = now + std::chrono::hours(24);
    uint32_t tomorrow_key = to_key(tomorrow);
    if (!m_spotprice_cache[TOMORROW] || m_spotprice_cache[TOMORROW]->m_key!=tomorrow_key) {
#ifdef VERBOSE_LOG
    log(stdout, "Fetching TOMORROW");
#endif
      m_spotprice_cache[TOMORROW] = fetch_prices(tomorrow);
    }
  }
  
  return m_spotprice_cache[TODAY]->m_price[now_local_tm.tm_hour][zone];
}

uint32_t SpotPriceCache::to_key(const std::chrono::system_clock::time_point& time)
{
  auto time_t = std::chrono::system_clock::to_time_t(time);
  auto time_local_tm = *localtime(&time_t); //We want local time
  return (time_local_tm.tm_year+1900)*100*100 + (time_local_tm.tm_mon+1)*100 + time_local_tm.tm_mday;
}

std::unique_ptr<SpotPrice> SpotPriceCache::fetch_prices(const std::chrono::system_clock::time_point& time)
{
  std::unique_ptr<SpotPrice> spot_price = std::make_unique<SpotPrice>();
  auto time_t = std::chrono::system_clock::to_time_t(time);
  auto time_local_tm = *localtime(&time_t); //We want local time
  spot_price->m_key = to_key(time);

  for (uint8_t hour=0; hour<24; hour++)
  {
    for (uint8_t zone=0; zone<5; zone++)
    {
      spot_price->m_price[hour][zone] = 0.0;
    }
  }

  
  try
  {
    // prepare session
    char date_str[11];
    snprintf(date_str, sizeof(date_str)/sizeof(date_str[0]), "%02d-%02d-%04d",
             time_local_tm.tm_mday, time_local_tm.tm_mon+1, time_local_tm.tm_year+1900); 

#ifdef VERBOSE_LOG
    log(stdout, "Preparing session for %s", date_str);
#endif

#ifdef DEBUG_LOCAL_FILE
	Poco::Path filePath(".", FILENAME);
	std::ostringstream ostr;

	if (filePath.isFile())
	{
		Poco::File inputFile(filePath);
		if (inputFile.exists())
		{
			Poco::FileInputStream fis(filePath.toString());
			Poco::StreamCopier::copyStream(fis, ostr);
		}
		else
		{
			std::cout << filePath.toString() << " doesn't exist!" << std::endl;
			return nullptr; 
		}
	}

	Poco::JSON::Parser parser;
	auto json_root = parser.parse(ostr.str());
#else
    Poco::URI uri(SPOT_URL+"?currency=NOK&endDate="+std::string(date_str));
    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    if(EQUAL == uri.getScheme().compare("https"))
    {
      session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort());
    }
    else
    {
      session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
    }

    // prepare path
    std::string path(uri.getPathAndQuery());
    if (path.empty()) path = "/";

    // send request
    Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
    session->sendRequest(req);

    // get response
    Poco::Net::HTTPResponse res;
    if (Poco::Net::HTTPResponse::HTTP_OK != res.getStatus())
    {
      log(stdout, "HTTP %d: %s", res.getStatus(), res.getReason().c_str());
      return nullptr;
    }

    Poco::JSON::Parser parser;
    auto json_root = parser.parse(session->receiveResponse(res));
#endif

    if (!json_root)
    {
      log(stdout, "Couldn't parse JSON root object");
      return nullptr;
    }

    auto object_root = json_root.extract<Poco::JSON::Object::Ptr>();
    if (!object_root)
    {
      log(stdout, "Couldn't extract JSON root object");
      return nullptr;
    }

    int page_id = object_root->getValue<int>("pageId");
    if (10 != page_id)
    {
      log(stdout, "Expected pageId=10, got pageId=%d", page_id);
      return nullptr;
    }

    std::string currency = object_root->getValue<std::string>("currency");
    if (0 != currency.compare("NOK"))
    {
      log(stdout, "Expected currency=NOK, got currency=%s", currency.c_str());
      return nullptr;
    }
    
#ifndef SKIP_DATE_CHECK
    std::string end_date = object_root->getValue<std::string>("endDate");
    if (0 != end_date.compare(date_str))
    {
      log(stdout, "Expected endDate=%s, got endDate=%s", date_str, end_date.c_str());
      return nullptr;
    }
#endif

    Poco::JSON::Object::Ptr data_object = object_root->getObject("data");
    if (!data_object)
    {
      log(stdout, "Couldn't get data object");
      return nullptr;
    }

    Poco::JSON::Array::Ptr rows_array = data_object->getArray("Rows");
    if (!rows_array)
    {
      log(stdout, "Couldn't get rows array");
      return nullptr;
    }

    for(size_t row_index=0; rows_array && row_index<rows_array->size(); row_index++)
    {
      Poco::JSON::Object::Ptr row_object = rows_array->getObject(row_index);
      
      if (true == row_object->getValue<bool>("IsExtraRow"))
      {
#ifdef VERBOSE_LOG
        log(stdout, "Skipping %s", row_object->getValue<std::string>("Name").c_str());
#endif
        continue;
      }
      
      int start_year, start_month, start_day, start_hour, start_minute, start_second;
      if (EOF == ::sscanf(row_object->getValue<std::string>("StartTime").c_str(), "%04d-%02d-%02dT%02d:%02d:%02d",
        &start_year, &start_month, &start_day, &start_hour, &start_minute, &start_second))
      {
        continue;
      }

#ifndef SKIP_DATE_CHECK
      if ((time_local_tm.tm_year+1900)!=start_year ||
          (time_local_tm.tm_mon+1)!=start_month ||
          time_local_tm.tm_mday!=start_day ||
          start_minute!=0 ||
          start_second!=0)
      {
        continue;
      }
#endif
      if (start_hour<0 || start_hour>=HOURS)
      {
        continue;
      }

      Poco::JSON::Array::Ptr columns_array = row_object->getArray("Columns");
      if (!columns_array)
      {
        log(stdout, "Couldn't get columns array");
        continue;
      }

      for(size_t column_index=0; columns_array && column_index<columns_array->size(); column_index++)
      {
        Poco::JSON::Object::Ptr column_object = columns_array->getObject(column_index);

        uint8_t zone = UNKNOWN;
        std::string name = column_object->getValue<std::string>("Name");
        if (EQUAL==name.compare("Oslo") || EQUAL==name.compare("NO1"))
        {
          zone = NO1;
        }
        else if (EQUAL==name.compare("Kr.sand") || EQUAL==name.compare("NO2"))
        {
          zone = NO2;
        }
        else if (EQUAL==name.compare("Molde") || EQUAL==name.compare("Tr.heim") || EQUAL==name.compare("NO3"))
        {
          zone = NO3;
        }
        else if (EQUAL==name.compare("Tromsø") || EQUAL==name.compare("NO4"))
        {
          zone = NO4;
        }
        else if (EQUAL==name.compare("Bergen") || EQUAL==name.compare("NO5"))
        {
          zone = NO5;
        }

        if (zone<=ZONES)
        {
          const char* value_string = column_object->getValue<std::string>("Value").c_str();
          char* locale_invariant_string = new char[strlen(value_string)+1];
          const char* source = value_string;
          char* destination = locale_invariant_string;
          while (*source)
          {
            if ((*source>='0' && *source<='9') || *source=='-')
            {
              *destination++ = *source;
            }
            else if (*source==',' || *source=='.')
            {
              *destination++ = '.';
            }
            
            source++;
          }
          
          if (destination == locale_invariant_string)
          {
            *destination++ = '0';
          }
          
          *destination = 0;

          double value = atof(locale_invariant_string);

          delete[] locale_invariant_string;
          
          spot_price->m_price[start_hour][zone] = value;
        }
      }
    }
  }
  catch (Poco::Exception &ex)
  {
    log(stdout, ex.displayText().c_str());
  }

  return spot_price;
}

#ifdef RUN_TEST
void SpotPriceCache::logCache()
{
  int hour;
  if (m_spotprice_cache[TODAY])
  {
    log(stdout, "\nCached TODAY:");
    for (hour=0; hour<HOURS; hour++)
    {
      log(stdout, "%d: NO1=%lf NO2=%lf NO3=%lf NO4=%lf NO5=%lf",
          hour,
          m_spotprice_cache[TODAY]->m_price[hour][NO1],
          m_spotprice_cache[TODAY]->m_price[hour][NO2],
          m_spotprice_cache[TODAY]->m_price[hour][NO3],
          m_spotprice_cache[TODAY]->m_price[hour][NO4],
          m_spotprice_cache[TODAY]->m_price[hour][NO5]);
    }
  }
  
  if (m_spotprice_cache[TOMORROW])
  {
    log(stdout, "\nCached TOMORROW:");
    for (hour=0; hour<HOURS; hour++)
    {
      log(stdout, "%d: NO1=%lf NO2=%lf NO3=%lf NO4=%lf NO5=%lf",
          hour,
          m_spotprice_cache[TOMORROW]->m_price[hour][NO1],
          m_spotprice_cache[TOMORROW]->m_price[hour][NO2],
          m_spotprice_cache[TOMORROW]->m_price[hour][NO3],
          m_spotprice_cache[TOMORROW]->m_price[hour][NO4],
          m_spotprice_cache[TOMORROW]->m_price[hour][NO5]);
    }
  }
}
#endif


void nordpoolspot::spot_handler(const std::shared_ptr<restbed::Session> session)
{
  std::string response_body;
  for (uint8_t i=0; i<5; i++)
  {
    response_body += "NO"+std::to_string(i+1)+".value "+std::to_string(g_spotprice_cache.get_price(i)/10.0)+"\n"; //Price is NOK/MWh, we want Øre/KWh. Thus: divide by 10
  }

  closeConnection(session, restbed::OK, response_body, "iso-8859-1");
}

#ifdef RUN_TEST
void nordpoolspot::testSpotHandler()
{
  log(stdout, "Current price:");
  for (uint8_t i=0; i<5; i++)
  {
    double spotprice = g_spotprice_cache.get_price(i);
    log(stdout, "Zone %d: %lf", i, spotprice);
  }

  g_spotprice_cache.logCache();
}
#endif
