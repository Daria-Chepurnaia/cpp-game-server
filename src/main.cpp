#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/signals2.hpp>

#include <iostream>
#include <string_view>
#include <deque>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "player.h"
#include "ticker.h"
#include "application.h"
#include "model_serialization.h"
#include "db_manager.h"

using namespace std::literals;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {  
    logging::add_common_attributes();
    logging::add_console_log(
        std::clog,
        keywords::format = &MyFormatter,
        keywords::auto_flush = true
    );
try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            boost::json::object server_stop_data;
            server_stop_data.insert({{LoggerJSONKeys::code, "EXIT_FAILURE"s}});
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, server_stop_data)
                                    << LoggerMessages::server_exited;
            return EXIT_FAILURE;
        }
        // Initialize the database

        const unsigned num_threads = std::thread::hardware_concurrency();

        postgres::ConnectionFactory factory(GetConfigFromEnv());
        ConnectionPool pool(num_threads, factory); // Pass the factory and pool size to the pool
        std::shared_ptr<postgres::DBManager> db_manager = std::make_shared<postgres::DBManager>(pool);
        auto on_leave_db_handler = [db_manager](std::string name, int total_time, int score) {
            db_manager->SavePlayer(name, total_time, score);
        };

        // 1. Load the map from a file and build the game model
        model::Game game = json_loader::LoadGame(args.value().config_file_path);
        game.SetPlayersStartPointRandomizing(args.value().randomize_spawn_points);
        game.SetOnLeaveHandler(on_leave_db_handler);
        if (args->state_path_specified) {
            serialization::RestoreGameState(args->state_path, game);
        }

        // 2. Initialize io_context
        net::io_context ioc(num_threads);
        using Strand = net::strand<net::io_context::executor_type>;
        Strand api_strand = net::make_strand(ioc);    

        // 3. Add an asynchronous handler for SIGINT and SIGTERM signals
        net::signal_set signals(ioc, SIGINT, SIGTERM);
           signals.async_wait([&ioc, &game, &args](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {                   
                   boost::json::object server_stop_data;
                   server_stop_data.insert({{LoggerJSONKeys::code, ec.value()}});
                   BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, server_stop_data)
                                           << LoggerMessages::server_exited  << std::flush;
                   ioc.stop();
                }
           });

        // 4. Create an HTTP request handler and link it to the game model
        std::optional<std::string>  save_state_path;
        if (args->state_path_specified) save_state_path = args->state_path;

        auto application = std::make_shared<app::Application>(game, save_state_path, db_manager);
        auto api_handler = std::make_shared<http_handler::APIHandler>(application, !args.value().tick_period_specified);
        auto handler = std::make_shared<http_handler::RequestHandler>(api_handler, args->static_data_path, api_strand);

        // 5. Bind the game model state-saving handler to the game clock tick
        boost::signals2::connection connection;

        if (args->save_state_period_specified) {
            auto save_handler = [&application, total = 0.,
                                 save_period = args->save_state_period](double time_delta) mutable {
                total += time_delta;
                if(total > save_period) {
                    application->SaveState();
                    total = 0.;
                };
            };            
            connection = application->DoOnTimeUpdate(save_handler);
        } else {
            connection = application->DoOnTimeUpdate([&application](double) {
                application->SaveState();
            });
        }

        // 5. Start updating the state of players and items at the specified interval
        if (args.value().tick_period_specified) {
            std::shared_ptr<Ticker> ticker = std::make_shared<Ticker>(api_strand,std::chrono::milliseconds(args->tick_period), [&application](std::chrono::milliseconds period) {
                                                                                            application->UpdateTime(period.count());
            });
            ticker->Start(); 
        }            
        
        // 6. Start the HTTP request handler
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send, std::string ip) {
            handler->operator()(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), ip);
        });

        // Log the server startup
        boost::json::object server_start_data;
        server_start_data.insert({{LoggerJSONKeys::port, port}});
        server_start_data.insert({{LoggerJSONKeys::address, address.to_string()}});
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, server_start_data)
                                << LoggerMessages::server_started;

        // 7. Start processing asynchronous operations
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        // 8. Save the game state to a file before shutting down the program
        if (args->state_path_specified) {
            application->SaveState();
        }       
    } catch (const std::exception& ex) {
        boost::json::object server_stop_data;
        server_stop_data.insert({{LoggerJSONKeys::code, "EXIT_FAILURE"s}});
        server_stop_data.insert({{LoggerJSONKeys::exception, ex.what()}});
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, server_stop_data)
                                << LoggerMessages::server_exited;
        return EXIT_FAILURE;
    }
}
