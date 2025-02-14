#include "request_handler.h"

namespace http_handler {

Response RequestHandler::MakeStaticFileResponse(std::string req_target, unsigned http_version,
                                                    bool keep_alive) {
    Response response;

    //URL-decoding of request target
    std::string decoded_target = urlDecode(req_target);
    if (!decoded_target.empty() && decoded_target[0] == '/') {
        decoded_target = decoded_target.substr(1);
    }
    if (decoded_target.empty()) {
        decoded_target = "index.html";
    }

    //Checking if path is correct and inside the static directory
    fs::path abs_path_target = path_to_static_ / fs::path(decoded_target);
    if (IsSubPath(abs_path_target, path_to_static_)) {
        if (fs::exists(abs_path_target)) {
            if (fs::is_regular_file(abs_path_target)) {
                //processing of extension
                auto lowercase_extension = getLowercaseExtension(abs_path_target);
                if (lowercase_extension.has_value() && contentTypeMap.contains(lowercase_extension.value())) {
                    response = MakeFileResponse(contentTypeMap.at(lowercase_extension.value()),
                                                        abs_path_target);
                    // if there is no extension or the extension is unknown
                } else {
                    response = MakeFileResponse(contentTypeMap.at("unknown_extension"),
                                                        abs_path_target);
                }
                return response;
                // if it's folder
            } else if (fs::is_directory(abs_path_target)){
                fs::path abs_path_target_indexhtml = abs_path_target / "index.html";

                if (std::filesystem::exists(abs_path_target_indexhtml)) {
                    response = MakeFileResponse(contentTypeMap.at("html"),
                                                        abs_path_target_indexhtml);
                    return response;
                } else {
                    response = MakeStringResponse(http::status::not_found, "file doesn't exist", http_version, keep_alive, ContentType::TXT);
                }
            }
            //if there is no folder or file with specified path
        } else {
            response = MakeStringResponse(http::status::not_found, "file or folder doesn't exist", http_version, keep_alive, ContentType::TXT);
        }

    } else {
        // if folder is on server but not in folder for common access
        if (fs::exists(abs_path_target)) {
            response = MakeStringResponse(http::status::bad_request, "access denied", http_version, keep_alive, ContentType::TXT);
        } else {
            response = MakeStringResponse(http::status::not_found, "file or folder doesn't exist", http_version, keep_alive, ContentType::TXT);
        }
    }
    return response;
}

bool APIHandler::IsAuthStringValid(std::string auth_str) {
    
    if (!auth_str.starts_with("Bearer ") || 
        auth_str.size() != 39) {
        return false;
    }

    for (uint i = 7; i < auth_str.size(); i++) {
        if (!std::isxdigit(auth_str[i])) {
            return false;
        }
    }
    return true;
}

Response APIHandler::MakeMapsInfoResponse(std::vector<std::string> uri_tokens,
                            std::string req_target,
                            unsigned http_version,
                            bool keep_alive,
                            std::string authorization) {
    std::string body;
    Response response;    
    if (req_target == "/api/v1/maps") {        
        body = json::serialize(application_->GetJSONforAllMaps());
        response = MakeJSONResponse(http::status::ok, body, http_version, keep_alive, ContentType::APPLICATION_JSON);
    } else if (uri_tokens.size() == 4 &&
                uri_tokens[0] == "api" &&
                uri_tokens[1] == "v1" &&
                uri_tokens[2] == "maps") {

        auto map_ptr = application_->FindMap(model::Map::Id(uri_tokens[3]));

        if (map_ptr) {
            body = json::serialize(application_->GetJSONforMap(model::Map::Id(map_ptr->GetId())));
            response = MakeJSONResponse(http::status::ok, body, http_version, keep_alive, ContentType::APPLICATION_JSON);
        } else {
            //wrong map           
            response = MakeJSONErrorResponse(http::status::not_found, "mapNotFound", "mapNotFound", http_version, keep_alive, ContentType::APPLICATION_JSON);
        }
    } else {
        //bad request       
        response = MakeJSONErrorResponse(http::status::bad_request, "badRequest", "Bad request", http_version, keep_alive, ContentType::APPLICATION_JSON);
    }
    return response;                           
}

std::vector<std::string> APIHandler::split(const std::string &str, char delimiter) const {
    std::vector<std::string> tokens;
    std::string::size_type start = 0;
    std::string::size_type end = 0;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        if (end != start) {
            tokens.emplace_back(str.substr(start, end - start));
        }
        start = end + 1;
    }
    if (start < str.size()) {
        tokens.emplace_back(str.substr(start));
    }

    return tokens;
}

std::string RequestHandler::urlDecode(const std::string &url) {
    std::string decoded;
    size_t length = url.length();

    for (size_t i = 0; i < length; ++i) {
        if (url[i] == '%') {
            if (i + 2 < length) {
                std::string hexStr = url.substr(i + 1, 2);
                char decodedChar = static_cast<char>(std::stoi(hexStr, nullptr, 16));
                decoded += decodedChar;
                i += 2; 
            }
        } else if (url[i] == '+') {
            decoded += ' '; 
        } else {
            decoded += url[i]; 
        }
    }

    return decoded;
}

bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
    
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> RequestHandler::getLowercaseExtension(const std::filesystem::path &filePath) {
    auto extension = filePath.extension().string();

    if (extension.empty()) {
        return std::nullopt; 
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (!extension.empty() && extension[0] == '.') {
        extension.erase(0, 1);
    }
    return extension;
}

RecordQueryParams ParseQueryParams(const std::string& query) {
    RecordQueryParams params;
    auto pos = query.find('?');
    if (pos == std::string::npos) {
        return params; 
    }

    std::string query_string = query.substr(pos + 1);

    std::istringstream stream(query_string);
    std::string key_value;
    while (std::getline(stream, key_value, '&')) {
        auto eq_pos = key_value.find('=');
        if (eq_pos == std::string::npos) {
            continue; 
        }

        std::string key = key_value.substr(0, eq_pos);
        std::string value = key_value.substr(eq_pos + 1);

        if (key == "start") {
            int parsed_value = 0;
            if (std::istringstream(value) >> parsed_value) {
                params.start = parsed_value;
            }
        } else if (key == "maxItems") {
            int parsed_value = 0;
            if (std::istringstream(value) >> parsed_value) {
                params.maxItems = parsed_value;
            }
        }
    }

    return params;
}

http_handler::StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version, bool keep_alive,
                                                std::string_view content_type, std::optional<std::string_view> allow_header) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    if (allow_header) {
        response.set(http::field::allow, allow_header.value());
    }
    return response;
}

FileResponse RequestHandler::MakeFileResponse(std::string content_type, fs::path path) {

    FileResponse response;
    response.version(11);  // HTTP/1.1
    http::file_body::value_type file;
    sys::error_code ec;
    file.open(path.string().c_str(), beast::file_mode::read, ec);

    if (ec) {
        throw std::runtime_error("Failed to open file: " + ec.message());
    }
    response.result(http::status::ok);
    response.set(http::field::content_type, content_type);
    response.body() = std::move(file);
    response.prepare_payload();
    return response;
}

Response APIHandler::MakeJSONErrorResponse(http::status status, std::string error_code_description, std::string error_message,
                                      unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type ,
                                      std::optional<std::string_view> allow_header) {
                
    json::object root;                                
    root.insert({{"code", error_code_description}});
    root.insert({{"message", error_message}});
    std::string body = json::serialize(root);
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    response.set(http::field::cache_control, "no-cache");
    if (allow_header) {
        response.set(http::field::allow, allow_header.value());
    }
    return response;
}

    Response APIHandler::MakeJSONResponse(http::status status, std::string body,
                                      unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type) {
                
        
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        response.set(http::field::cache_control, "no-cache");        
        return response;
    }

    std::string APIHandler::MakeAuthJSON (std::string authToken, std::string playerId) {
        json::object root;
        root.insert({{"authToken", authToken}});
        root.insert({{"playerId", std::stoi(playerId)}});
        std::string body = json::serialize(root);
        return body;
    } 

     Response APIHandler::MakeEmptyJSONResponse(http::status status, 
                                      unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type) {
                
        json::object root = {};        
        std::string body = json::serialize(root);
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        response.set(http::field::cache_control, "no-cache");
        
        return response;
    }  


    Response APIHandler::MakeJoinResponse(std::string user_name, std::string map_id, unsigned http_version, bool keep_alive) {
        
        Response response;
        if (user_name == "") {
        // empty user name 
            response = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name",
                                            http_version, keep_alive);
        // check if map exists
        } else if (application_->FindMap(model::Map::Id(map_id)) == nullptr) {
            //map not found
            response = MakeJSONErrorResponse(http::status::not_found, "mapNotFound", "Map not found",
                                            http_version, keep_alive);
        } else {
            std::shared_ptr<app::Player> player = application_->JoinGame(user_name, application_->FindMap(model::Map::Id(map_id)));            
            response = MakeJSONResponse(http::status::ok, MakeAuthJSON(player->GetToken(), std::to_string(player->GetId())), http_version, keep_alive);                                             
        }        
        return response;
    }


}  // namespace http_handler
