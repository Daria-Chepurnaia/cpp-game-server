#pragma once

#include <boost/log/trivial.hpp>

#include <string_view>

#include <boost/log/core.hpp>       
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <boost/json.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;

namespace json = boost::json;

using namespace std::literals;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

namespace LoggerMessages {
    const std::string server_started = "server started"s;
    const std::string server_exited = "server exited"s;
    const std::string request_received = "request received"s;
    const std::string response_sent = "response sent"s;
    const std::string error = "error"s;
}

namespace LoggerJSONKeys {
    const std::string message = "message"s;
    const std::string timestamp = "timestamp"s;
    const std::string data = "data"s;
    const std::string code = "code"s;
    const std::string port = "port"s;
    const std::string address = "address"s;
    const std::string ip = "ip"s;
    const std::string URI = "URI"s;
    const std::string method = "method"s;
    const std::string response_time = "response_time"s;
    const std::string content_type = "content_type"s;
    const std::string exception = "exception"s;
    const std::string text = "text"s;
    const std::string where = "where"s;
}

void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);

