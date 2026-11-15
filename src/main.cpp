#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <thread>
#include <vector>
#include <optional>

#include "ticker.h"
#include "json_lib/json_loader.h"
#include "request_handler/request_handler.h"
#include "postgres/postgres.h"

using namespace std::literals;
using namespace std::chrono_literals;

namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

namespace {

struct Args {
    std::optional<int> tick_period;
    std::string config_file;
    std::string www_root;
    std::optional<std::string> state_file;
    std::optional<uint32_t> save_state_period;
};

std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message")
        ("tick-period,t", po::value<int>(), "Tick period in milliseconds")
        ("config-file,c", po::value<std::string>()->required(), "Path to a game config file")
        ("www-root,w", po::value<std::string>()->required(), "Path to static files root")
        ("state-file", po::value<std::string>(), "Path to state file")
        ("save-state-period", po::value<uint32_t>(), "Save state period in ms");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return std::nullopt;
        }
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Command line error: " << e.what() << "\n";
        return std::nullopt;
    }

    Args args;
    args.config_file = vm["config-file"].as<std::string>();
    args.www_root = vm["www-root"].as<std::string>();
    if (vm.count("tick-period")) args.tick_period = vm["tick-period"].as<int>();
    if (vm.count("state-file")) args.state_file = vm["state-file"].as<std::string>();
    if (vm.count("save-state-period")) args.save_state_period = vm["save-state-period"].as<uint32_t>();
    return args;
}

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

} //namespace

int main(int argc, const char* argv[]) {
    http_handler::LoggerInitializer::Init();

    auto args = ParseCommandLine(argc, argv);
    if (!args) {
        return EXIT_FAILURE;
    }

    try {
        infrastructure::FrontendLootRepository frontend_loot_repo;

        const char* db_url = std::getenv("GAME_DB_URL");

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        size_t pool_capacity = std::max(1u, std::thread::hardware_concurrency());
        ConnectionPool conn_pool(pool_capacity, [db_url] {
            return std::make_shared<pqxx::connection>(db_url);
        });

        postgres::Database db(conn_pool);

        model::PlayerTokens player_tokens;
        model::Players players{player_tokens};

        model::Game game = json_loader::LoadGame(args->config_file, frontend_loot_repo, players, db);

        auto state_manager = std::make_shared<infrastructure::StateManager>(args->state_file, args->save_state_period);

        if (!state_manager->RestoreState(game, players)) {
            BOOST_LOG_TRIVIAL(error) << "{\"code\": \"EXIT_FAILURE\", \"message\": \"Failed to restore game state from file\"}";
            return EXIT_FAILURE;
        }

        std::shared_ptr<Ticker> ticker;
        if (args->tick_period) {
            std::chrono::milliseconds period(*args->tick_period);

            ticker = std::make_shared<Ticker>(
                api_strand,
                period,
                [&game, &players, state_manager](std::chrono::milliseconds delta) {
                    double delta_seconds = delta.count() / 1000.0;
                    game.Tick(delta_seconds);
                    state_manager->OnTick(delta, game, players);
                }
                );
            ticker->Start();
        }

        auto handler = std::make_shared<http_handler::RequestHandler>(game, players, args->www_root, api_strand, frontend_loot_repo, state_manager, db);
        auto logging_handler = std::make_shared<http_handler::LoggingRequestHandler<http_handler::RequestHandler>>(*handler);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, state_manager, &game, &players, api_strand](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                net::dispatch(api_strand, [state_manager, &game, &players, &ioc]() {
                    state_manager->SaveState(game, players);
                    ioc.stop();
                });
            }
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [logging_handler](net::ip::tcp::endpoint ep, auto&& req, auto&& sender) {
            (*logging_handler)(ep, std::forward<decltype(req)>(req), std::forward<decltype(sender)>(sender));
        });

        http_handler::ServerLogger::LogStart(address.to_string(), port);

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        http_handler::ServerLogger::LogExit(EXIT_FAILURE, &ex);
        return EXIT_FAILURE;
    }

    http_handler::ServerLogger::LogExit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
