#pragma once
#include "http_server.h"
#include "logger.h"
#include "loot.h"
#include "application.h"

#include <string_view>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <memory>
#include <optional>
#include <ostream>
#include <thread>
#include <variant>
#include <vector>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace app;
using namespace std::literals;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using Response = std::variant<StringResponse, FileResponse>;
using Strand = net::strand<net::io_context::executor_type>;   

const std::map<std::string, std::string> contentTypeMap = {
    {"htm", "text/html"},
    {"html", "text/html"},
    {"css", "text/css"},
    {"txt", "text/plain"},
    {"js", "text/javascript"},
    {"json", "application/json"},
    {"xml", "application/xml"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpe", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"bmp", "image/bmp"},
    {"ico", "image/vnd.microsoft.icon"},
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"svg", "image/svg+xml"},
    {"svgz", "image/svg+xml"},
    {"mp3", "audio/mpeg"},
    {"unknown_extension", "application/octet-stream"}
};

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;    
    constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    constexpr static std::string_view ALLOW = "GET, HEAD"sv;
    constexpr static std::string_view TXT = "text/plain"sv;
    constexpr static std::string_view POST = "POST"sv;
};

StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type = ContentType::TEXT_HTML,
                                      std::optional<std::string_view> allow_header = std::nullopt);

class APIHandler  {
public:  
    APIHandler(std::shared_ptr<Application> application, bool ticker_is_manual)
        : application_(application) 
        , ticker_is_manual_(ticker_is_manual)   
    {
    }

    template <typename Request>
    Response MakeAPIResponse(Request&& req);

private:
    std::shared_ptr<Application> application_;
    bool ticker_is_manual_;

    template <typename Fn, typename Request>
    Response ExecuteAuthorized(Fn&& action, Request&& req);

    template<typename Request>
    auto MovePlayer(Request& req);

    template<typename Request>
    auto GetStat(Request& req);

    template<typename Request>
    auto GetPlayersInfo(Request& req);

    template<typename Request>
    Response UpdateTime(Request& req);  

    template<typename Request>   
    Response JoinGame(Request& req);   

    template<typename Request>
    Response MakeRecordsResponse(Request& req);

    Response MakeJSONErrorResponse(http::status status, std::string error_code_description, std::string error_message,
                                      unsigned http_version, bool keep_alive,
                                      std::string_view content_type = ContentType::APPLICATION_JSON,
                                      std::optional<std::string_view> allow_header = std::nullopt);                                      
    Response MakeJSONResponse(http::status status, std::string body, unsigned http_version, bool keep_alive,
                                      std::string_view content_type = ContentType::APPLICATION_JSON);                                                              
    Response MakeEmptyJSONResponse(http::status status, unsigned http_version, bool keep_alive,                                      
                                      std::string_view content_type = ContentType::APPLICATION_JSON);
    Response MakeJoinResponse(std::string user_name, std::string map_id, unsigned http_version, bool keep_alive);
    Response MakeMapsInfoResponse(std::vector<std::string> uri_tokens, std::string req_target,
                             unsigned http_version, bool keep_alive, std::string authorization = ""s);

    // function to split string with delimiter
    std::vector<std::string> split(const std::string& str, char delimiter) const; 
    bool IsAuthStringValid(std::string auth_str); 
    std::string MakeAuthJSON(std::string authToken, std::string playerId);
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(std::shared_ptr<APIHandler> api_handler, fs::path path_to_static, Strand strand)
         : api_handler_(api_handler)
         , path_to_static_(path_to_static)
         , strand_(std::move(strand))
    {        
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;     

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send,
                    std::string ip={});

    private:
    std::shared_ptr<APIHandler> api_handler_;  
    fs::path path_to_static_;
    Strand strand_;
    

    template<typename Send>      
    void SendResponse(std::chrono::milliseconds ms, Send&& send, Response& r);       

    Response MakeStaticFileResponse(std::string req_target, unsigned http_version,bool keep_alive);
    FileResponse MakeFileResponse(std::string content_type, fs::path path); 
    std::string urlDecode(const std::string& url); 
    bool IsSubPath(fs::path path, fs::path base);
    std::optional<std::string> getLowercaseExtension(const std::filesystem::path& filePath);
};

struct RecordQueryParams {
    std::optional<int> start;
    std::optional<int> maxItems;
};

RecordQueryParams ParseQueryParams(const std::string& query);

// ====== Implementation of Template Methods for APIHandler ======

template <typename Request>
Response APIHandler::MakeAPIResponse(Request&& req) {
    std::string req_target(req.target());
    std::vector<std::string> uri_tokens = split(req_target, '/');  
    Response r;
    auto req_content_type = req[http::field::content_type];       

    if (req_target == "/api/v1/game/join") {
        
        if (req.method_string() == "POST") {
            if (req_content_type == contentTypeMap.at("json")) {
                r = JoinGame(req);
            } else {
                //wrong content type
                r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "content type should be application/json",
                                        req.version(), req.keep_alive());
            }
        } else {
            //only POST expected
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "only POST expected",
                                    req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, "POST");
        }       
        
    } else if (req_target == "/api/v1/game/players") {

        if (req.method_string() == "GET" || req.method_string() == "HEAD") {         
            r = ExecuteAuthorized(GetPlayersInfo(std::move(req)), req);       
        } else {
            //wrong method
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::ALLOW);
        }  

    } else if (req_target.starts_with("/api/v1/maps")) {

        if (req.method_string() == "GET" || req.method_string() == "HEAD") {        
            r = MakeMapsInfoResponse(uri_tokens, req_target, req.version(), req.keep_alive());                   
        } else {
            //wrong method
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::ALLOW);
        } 

    } else if (req_target.starts_with("/api/v1/game/state")) {

        if (req.method_string() == "GET" || req.method_string() == "HEAD") {        
            //get map statistic by player's token
            r = ExecuteAuthorized(GetStat(std::move(req)), req);                 
        } else {
            //wrong method
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::ALLOW);
        } 

    } else if (req_target == "/api/v1/game/player/action"){

        if (req.method_string() == "POST") {
            if (req_content_type == contentTypeMap.at("json")) {
                //move player and get response
                r = ExecuteAuthorized(MovePlayer(std::move(req)), req);
            } else {
                //wrong content type
                r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "content type should be application/json",
                                        req.version(), req.keep_alive());
            }                
        } else {
            //wrong method
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::POST);
        }
    
    } else if (req_target == "/api/v1/game/tick") {
        if (ticker_is_manual_) {
            if (req.method_string() == "POST") {
                if (req_content_type == contentTypeMap.at("json")) {
                    //uppdate time and get response
                    r = UpdateTime(std::move(req));
                } else {
                    //wrong content type
                    r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "content type should be application/json",
                                            req.version(), req.keep_alive());
                }                
            } else {
                //wrong method
                r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                            req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::POST);
            }
        } else {
            r = MakeJSONErrorResponse(http::status::bad_request, "badRequest", "invalid endpoint",
                                            req.version(), req.keep_alive());
        }
    } else if (req_target.starts_with("/api/v1/game/records")) {
        if (req.method_string() == "GET" || req.method_string() == "HEAD") { 
             
            r = MakeRecordsResponse(std::move(req));                 
        } else {
            //wrong method
            r = MakeJSONErrorResponse(http::status::method_not_allowed, "invalidMethod", "invalid method",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::ALLOW);
        } 
    //bad request
    } else {
        r = MakeJSONErrorResponse(http::status::bad_request, "badRequest", "Bad request",
                                        req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, ContentType::POST);
    }
    return r;
}


template <typename Fn, typename Request>
Response APIHandler::ExecuteAuthorized(Fn&& action, Request&& req) {       
    Response r;
    std::string req_authorization = std::string(req[http::field::authorization]);

    //check if token has right format            
    if (IsAuthStringValid(req_authorization)) {                              
        std::string token = req_authorization.substr(7);
        //check if token is in game
        std::shared_ptr<app::Player> player_ptr = application_->GetPlayerByToken(token);
        if (player_ptr) {                
            r = action(player_ptr);                                             

        //logic of correct token but not in game
        } else {
            r = MakeJSONErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found",
                                req.version(), req.keep_alive(), ContentType::APPLICATION_JSON);  
        }
    } else {
        //wrong token format
        r = MakeJSONErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missed",
                                req.version(), req.keep_alive(), ContentType::APPLICATION_JSON);
    }
    return r;
}

template<typename Request>
auto APIHandler::MovePlayer(Request& req) {

    return [this, &req](std::shared_ptr<app::Player> player_ptr) {    
        Response r;
        std::string req_body = req.body();
        try {
            auto root = json::parse(req_body);
            std::string move = root.at("move").as_string().c_str();                        
            if (move == "U" ||
                move == "R" ||
                move == "L" ||
                move == "D" ||
                move == "") {
                this->application_->Move(player_ptr, move);
                r = MakeEmptyJSONResponse(http::status::ok, req.version(), req.keep_alive());
            } else {
                r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "allowed values: R, L, U, D and emtpy string",
                                req.version(), req.keep_alive()); 
            }                                
                                
        }
        catch(const std::exception& e) {
            //parsing json error or incorrect keys in json
            r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "parsing json error",
                                req.version(), req.keep_alive());
        } 
        return r;
    };    
} 

template<typename Request>
Response APIHandler::UpdateTime(Request& req) {       
    Response r;
    std::string req_body = req.body();
    try {
        auto root = json::parse(req_body);
        double time_delta = root.at("timeDelta").as_int64();                        
        if (time_delta >= 0.) {
            this->application_->UpdateTime(time_delta);
            r = MakeEmptyJSONResponse(http::status::ok, req.version(), req.keep_alive());
        } else {
            r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "time delta should be greater than 0",
                            req.version(), req.keep_alive()); 
        }                          
    }
    catch(const std::exception& e) {
        //parsing json error or incorrect keys in json
        boost::json::object server_start_data;
        server_start_data.insert({{"exception: ", e.what()}});
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, server_start_data)
                                << LoggerMessages::server_started;
        r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "parsing json error",
                            req.version(), req.keep_alive());
    } 
    return r;    
} 

template<typename Request>
Response APIHandler::MakeRecordsResponse(Request& req) {
    Response r;    
    std::string req_target(req.target());
    RecordQueryParams query_params = ParseQueryParams(req_target);

    if (query_params.maxItems && *query_params.maxItems > 100) { 
        //bad request
        r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "query parameter maxItem should be not more than 100",
            req.version(), req.keep_alive());        
    } else {
        std::string body = this->application_->GetRecordsJSONInfo(query_params.start, query_params.maxItems);
        r = MakeJSONResponse(http::status::ok, body, req.version(), req.keep_alive());    
    }   
    return r;  
}

template<typename Request>
auto APIHandler::GetStat(Request& req) {
return [this, &req](std::shared_ptr<app::Player> player_ptr) {           
        std::string body = this->application_->GetStateJSONInfo(player_ptr);
        return MakeJSONResponse(http::status::ok, body, req.version(), req.keep_alive());    
    };
}

template<typename Request>
auto APIHandler::GetPlayersInfo(Request& req) {
return [this, &req](std::shared_ptr<app::Player> player_ptr) {           
        std::string body = this->application_->GetPlayersJSONInfo(player_ptr);
        return MakeJSONResponse(http::status::ok, body, req.version(), req.keep_alive());    
    };
}  

template<typename Request>   
Response APIHandler::JoinGame(Request& req) {
    Response r;
    std::string req_body = req.body();
    try {
        auto root = json::parse(req_body);
        auto user_name = root.at("userName").as_string().c_str();                        
        auto map_id = root.at("mapId").as_string().c_str();
        r = MakeJoinResponse(user_name, map_id, req.version(), req.keep_alive());                            
    }
    catch(const std::exception& e) {
        //parsing json error or incorrect keys in json
        r = MakeJSONErrorResponse(http::status::bad_request, "invalidArgument", "parsing json error",
                            req.version(), req.keep_alive());
    } 
    return r;
}

// ====== Implementation of Template Methods for RequestHandler ======

template <typename Body, typename Allocator, typename Send>
void RequestHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                Send&& send,
                std::string ip) {         

    std::string req_target(req.target());       

    boost::json::object req_data_log;
    req_data_log.insert({{LoggerJSONKeys::ip, ip}});
    req_data_log.insert({{LoggerJSONKeys::URI, req_target}});
    req_data_log.insert({{LoggerJSONKeys::method, req.method_string()}});        
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, req_data_log)
                            << LoggerMessages::request_received;

    auto start = std::chrono::steady_clock::now();        

    Response r;        
    
    if (req_target.starts_with("/api")) {
        auto self = shared_from_this();
        auto api_req_handler = [this, self, send, req = std::forward<decltype(req)>(req), start](){
            Response r = self->api_handler_->MakeAPIResponse(std::move(req));
            
            auto finish = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
            
            self->SendResponse(ms, std::move(send), r);
        };
        net::dispatch(strand_, api_req_handler);
        
        return;
    } else {
        //process static files
        r = MakeStaticFileResponse(req_target, req.version(), req.keep_alive());
        auto finish = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
        SendResponse(ms, std::move(send), r);
    } 
} 

template<typename Send>      
void RequestHandler::SendResponse(std::chrono::milliseconds ms, Send&& send, Response& r) {
    int response_status_code;
    std::string content_type;

    if (std::holds_alternative<StringResponse>(r)) {
        auto& response = std::get<StringResponse>(r);
        response_status_code = response.result_int();
        content_type = response[http::field::content_type];
        send(response);
    } else if (std::holds_alternative<FileResponse>(r)) {
        auto& response = std::get<FileResponse>(r);
        response_status_code = response.result_int();
        content_type = response[http::field::content_type];
        send(response);
    }

    boost::json::object response_data_log;
    response_data_log.insert({{LoggerJSONKeys::response_time, ms.count()}});
    response_data_log.insert({{LoggerJSONKeys::code, response_status_code}});
    response_data_log.insert({{LoggerJSONKeys::content_type, content_type}});

    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, response_data_log)
                            << LoggerMessages::response_sent;
}

}  // namespace http_handler
