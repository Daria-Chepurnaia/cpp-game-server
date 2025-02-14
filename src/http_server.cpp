#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>
#include <string_view>
#include "logger.h"

namespace http_server {


void ReportError(beast::error_code ec, std::string_view what) {
    boost::json::object error_data_log;

    error_data_log.insert({{LoggerJSONKeys::code, ec.value()}});
    error_data_log.insert({{LoggerJSONKeys::text, ec.message()}});
    error_data_log.insert({{LoggerJSONKeys::where, std::string(what)}});

    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, error_data_log)
                            << LoggerMessages::error;
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}
}  // namespace http_server
