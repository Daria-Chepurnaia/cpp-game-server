#include "logger.h"

void MyFormatter(const logging::record_view &rec, logging::formatting_ostream &strm) {
    boost::json::object root;
    auto ts = *rec[timestamp];
    auto data = *rec[additional_data];
    root.insert({{LoggerJSONKeys::timestamp, to_iso_extended_string(ts)}});
    root.insert({{LoggerJSONKeys::message, *rec[logging::expressions::smessage]}});
    root.insert({{LoggerJSONKeys::data, data}});
    std::string body = json::serialize(root);
    strm << body;
}
