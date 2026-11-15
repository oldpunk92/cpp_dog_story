#pragma once

#include "game_model/model.h"

#include <boost/beast/http.hpp>
#include <string_view>
#include <filesystem>
#include <variant>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
namespace sys = boost::system;

using namespace std::literals;

class FileRequestHandler {
private:
    http::response<http::string_body> ResponseFileNotFound(std::string_view target, unsigned version);
    http::response<http::string_body> ResponseBadRequestToFile(std::string_view target, unsigned version);
    http::response<http::string_body> ResponseNoContent(std::string_view target, unsigned version);
    http::response<http::string_body> InternalServerError(unsigned version);

    std::string_view ContentType(std::string file_extension);
    std::string DecodeUri(std::string_view uri_string);
    bool IsSubPath(fs::path path, fs::path base);

public:
    explicit FileRequestHandler(const fs::path& root_catalogue)
        : root_catalogue_(root_catalogue) {}

    using FileRequestResult = std::variant<http::response<http::file_body>, http::response<http::string_body>>;

    template <typename Body, typename Allocator>
    FileRequestResult HandleFileRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto method = req.method();
        auto target = req.target();
        auto version = req.version();

        if (method != http::verb::get && method != http::verb::head) {
            return ResponseBadRequestToFile(target, version);
        }

        std::string decoded_path_str = DecodeUri(target);
        std::string_view decode_path(decoded_path_str);

        if (decode_path.front() == '/') {
            decode_path.remove_prefix(1);
        }

        if (decode_path.empty()) {
            decode_path = "index.html";
        }

        fs::path file_path = fs::path(root_catalogue_) / fs::path(decode_path);

        if (!fs::exists(file_path)) {
            return ResponseFileNotFound(target, version);
        }

        if (!IsSubPath(file_path, root_catalogue_)) {
            return ResponseBadRequestToFile(target, version);
        }

        if (fs::file_size(file_path) == 0) {
            return ResponseNoContent(target, version);
        }

        auto file_extension = file_path.extension().string();

        http::response<http::file_body> res;
        http::file_body::value_type file;

        if (sys::error_code ec; file.open(file_path.string().c_str(), beast::file_mode::read, ec), ec) {
            return InternalServerError(version);
        }

        auto file_size = file.size();

        res.version(11);
        res.result(http::status::ok);
        res.set(http::field::content_type, std::string(ContentType(file_extension)));
        res.content_length(file_size);

        if (method == http::verb::head) {
            return res;
        }

        res.body() = std::move(file);
        res.prepare_payload();

        return res;
    }

private:
    fs::path root_catalogue_;
};
}  // namespace http_handler
