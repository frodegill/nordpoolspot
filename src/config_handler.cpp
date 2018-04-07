#include "config_handler.h"

#include "main.h"


using namespace nordpoolspot;

void nordpoolspot::config_handler(const std::shared_ptr<restbed::Session> session)
{
  const std::string response_body =
    "graph_title Nordpool Spot øre/KWh\n"\
    "graph_args --base 1000 -l 0\n"\
    "graph_vlabel Øre\n"\
    "graph_category homeautomation\n"\
    "graph_info This graph shows the nordpool spot price for the five Norwegian areas.\n"\
    "NO1.label Oslo\n"\
    "NO1.draw LINE2\n"\
    "NO1.info Øre/KWh\n"\
    "NO2.label Kristiansand\n"\
    "NO2.draw LINE2\n"\
    "NO2.info Øre/KWh\n"\
    "NO3.label Molde, Trondheim\n"\
    "NO3.draw LINE2\n"\
    "NO3.info Øre/KWh\n"\
    "NO4.label Tromsø\n"\
    "NO4.draw LINE2\n"\
    "NO4.info Øre/KWh\n"\
    "NO5.label Bergen\n"\
    "NO5.draw LINE2\n"\
    "NO5.info Øre/KWh\n";

    closeConnection(session, restbed::OK, response_body);
}
