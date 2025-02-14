#pragma once 
#include <boost/program_options.hpp>
#include <boost/asio/strand.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <vector>
#include <memory>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

struct Args {    
    std::string config_file_path;
    std::string static_data_path;   
    std::string state_path; 
    int tick_period;
    int save_state_period;
    bool randomize_spawn_points = false;
    bool tick_period_specified = false;
    bool state_path_specified = false;
    bool save_state_period_specified = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;    

    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"s), "set tick period")  
        ("save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"s), "set state saving period")      
        ("config-file,c", po::value(&args.config_file_path)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.static_data_path)->value_name("dir"s), "set static files root")
        ("state-file", po::value(&args.state_path)->value_name("state"s), "set state file path")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (vm.contains("randomize-spawn-points"s)) {
        args.randomize_spawn_points = true;
    }   
    if (vm.contains("tick-period"s)) {
        args.tick_period_specified = true;    
    }
    if (vm.contains("state-file"s)) {
        args.state_path_specified = true; 
        if (vm.contains("save-state-period"s)) {
            args.save_state_period_specified = true;    
        }   
    }
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("path to config file has not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("path to static directory has not been specified"s);
    }

    return args;
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};