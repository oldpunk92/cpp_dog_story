#pragma once

#include <boost/log/trivial.hpp>                         // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>                            // для logging::core
#include <boost/log/expressions.hpp>                     // для выражения, задающего фильтр
#include <boost/log/utility/setup/file.hpp>              // для сохранения лога в файл
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp> // для того, чтобы таймстемп отображался, нужно предварительно вызвать функцию logging::add_common_attributes() из этого header'а
#include <boost/date_time.hpp>                           // для работы со временем
#include <boost/log/utility/manipulators/add_value.hpp>  // для задания атрибутов для логирования
#include <boost/beast/http.hpp>
#include <string_view>
#include <iostream>


#include "http_server/http_server.h"
#include "api_handler.h"
#include "file_handler.h"
#include "game_model/model.h"

namespace http_handler {
namespace beast = boost::beast;
namespace net = boost::asio;
namespace http = beast::http;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace fs = std::filesystem;
namespace sys = boost::system;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

//-------------------------------------------------------------------------------------------------
//                       CLASSes
//-------------------------------------------------------------------------------------------------

//----------------LoggerInitializer----------------------------------------------------------------

class LoggerInitializer {
private:
    static void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
        boost::json::object final_log;

        if (auto ts_attr = rec[timestamp]) {
            final_log["timestamp"] = boost::posix_time::to_iso_extended_string(*ts_attr);
        } else {
            final_log["timestamp"] = "unknown";
        }

        std::string raw_msg = *rec[logging::expressions::smessage];

        auto parsed_json = boost::json::parse(raw_msg).as_object();

        final_log["message"] = parsed_json["message"];
        final_log["data"] = parsed_json["data"];

        strm << boost::json::serialize(final_log);
    }

public:
    static void Init() {
        logging::add_common_attributes();

        auto sink = logging::add_console_log(
            std::cout,
            logging::keywords::filter = logging::trivial::severity >= logging::trivial::info
            );

        sink->locked_backend()->auto_flush(true);
        sink->set_formatter(&JsonFormatter);
    }
};

//-------------------------------------------------------------------------------------------------

//---------------------ServerLogger----------------------------------------------------------------

class ServerLogger {
private:
    static void EmitLog(logging::trivial::severity_level level, const std::string& message, boost::json::object data) {
        boost::json::object payload;
        payload["message"] = message;
        payload["data"] = std::move(data);

        BOOST_LOG_TRIVIAL(info) << boost::json::serialize(payload);
    }

public:
    static void LogStart(const std::string& address, unsigned short port) {
        EmitLog(logging::trivial::info, "server started", {{"address", address}, {"port", port}});
    }

    static void LogExit(int code, const std::exception* ex = nullptr) {
        boost::json::object data{{"code", code}};
        if (ex) {
            data["exception"] = ex->what();
        }
        EmitLog(logging::trivial::info, "server exited", std::move(data));
    }

    template <typename Body, typename Fields>
    static void LogRequest(const http::request<Body, Fields>& req, const std::string& ip) {
        EmitLog(logging::trivial::info, "request received", {
                                                                {"ip", ip},
                                                                {"URI", std::string(req.target())},
                                                                {"method", std::string(req.method_string())}
                                                            });
    }

    template <typename Body, typename Fields>
    static void LogResponse(const http::response<Body, Fields>& resp, long long response_time_ms) {
        auto content_type_iter = resp.find(http::field::content_type);
        boost::json::value content_type = nullptr;
        if (content_type_iter != resp.end()) {
            content_type = std::string(content_type_iter->value());
        }

        EmitLog(logging::trivial::info, "response sent", {
                                                             {"response_time", response_time_ms},
                                                             {"code", resp.result_int()},
                                                             {"content_type", content_type}
                                                         });
    }

    static void LogError(const beast::error_code& ec, const std::string& where) {
        EmitLog(logging::trivial::error, "error", {
                                                      {"code", ec.value()},
                                                      {"text", ec.message()},
                                                      {"where", where}
                                                  });
    }
};

//-------------------------------------------------------------------------------------------------

//---------------------SyncWriteOStreamAdapter-----------------------------------------------------

class SyncWriteOStreamAdapter {
public:
    explicit SyncWriteOStreamAdapter(std::ostream& os)
        : os_{os} {
    }

    template <typename ConstBufferSequence>
    size_t write_some(const ConstBufferSequence& cbs, sys::error_code& ec) {
        const size_t total_size = net::buffer_size(cbs);
        if (total_size == 0) {
            ec = {};
            return 0;
        }
        size_t bytes_written = 0;
        for (const auto& cb : cbs) {
            const size_t size = cb.size();
            const char* const data = reinterpret_cast<const char*>(cb.data());
            if (size > 0) {
                if (!os_.write(reinterpret_cast<const char*>(data), size)) {
                    ec = make_error_code(boost::system::errc::io_error);
                    return bytes_written;
                }
                bytes_written += size;
            }
        }
        ec = {};
        return bytes_written;
    }

    template <typename ConstBufferSequence>
    size_t write_some(const ConstBufferSequence& cbs) {
        sys::error_code ec;
        const size_t bytes_written = write_some(cbs, ec);
        if (ec) {
            throw std::runtime_error("Failed to write");
        }
        return bytes_written;
    }

private:
    std::ostream& os_;
};

//-------------------------------------------------------------------------------------------------

//---------------------RequestHandler--------------------------------------------------------------

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(model::Game& game,
                            model::Players& players,
                            const fs::path& static_path,
                            net::strand<net::io_context::executor_type> api_strand,
                            infrastructure::FrontendLootRepository& repo,
                            std::shared_ptr<infrastructure::StateManager> state_manager,
                            model::IRecordRepository& db)
        : api_handler_(game, players, repo, state_manager, db)
        , file_handler_(static_path)
        , api_strand_{api_strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

private:
    http::response<http::string_body> ResponseBadRequest(unsigned version);
    http::response<http::string_body> InternalServerError(unsigned version);

public:
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto target = req.target();
        auto version = req.version();

        if (target.empty()) {
            send(ResponseBadRequest(version));
            return;
        }

        if (target.starts_with("/api/")) {
            auto self = this->shared_from_this();
            net::dispatch(api_strand_, [self, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                try {
                    auto response = self->api_handler_.HandleApiRequest(req);
                    send(std::move(response));
                } catch (...) {
                    send(self->InternalServerError(req.version()));
                }
            });
            return;
        }
        try {
            auto file_result = file_handler_.HandleFileRequest(req);

            std::visit([&send](auto&& result) {
                send(std::forward<decltype(result)>(result));
            }, std::move(file_result));
        } catch (...) {
            send(InternalServerError(version));
        }
    }

private:
    ApiRequestHandler api_handler_;
    FileRequestHandler file_handler_;
    net::strand<net::io_context::executor_type> api_strand_;
};

//-------------------------------------------------------------------------------------------------

//---------------------LoggingRequestHandler-------------------------------------------------------

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& decorated)
        : decorated_(decorated) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        boost::asio::ip::tcp::endpoint endpoint,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send
        ) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::string client_ip = endpoint.address().to_string();

        ServerLogger::LogRequest(req, client_ip);

        auto logging_send = [send = std::forward<Send>(send), start_time](auto&& response) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            ServerLogger::LogResponse(response, duration);
            send(std::forward<decltype(response)>(response));
        };

        decorated_(std::move(req), std::move(logging_send));
    }

private:
    SomeRequestHandler& decorated_;
};

//-------------------------------------------------------------------------------------------------
}  // namespace http_handler
