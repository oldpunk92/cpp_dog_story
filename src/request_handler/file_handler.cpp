#include "file_handler.h"

namespace http_handler {
http::response<http::string_body> FileRequestHandler::ResponseFileNotFound(std::string_view target, unsigned version) {
    http::response<http::string_body> response(http::status::not_found, version);
    response.set(http::field::content_type, "text/plain");

    if (!target.empty() && target.front() == '/') {
        target.remove_prefix(1);
    }

    response.body() = "File " + std::string(target) + " not found!";
    response.prepare_payload();
    return response;
}

http::response<http::string_body> FileRequestHandler::ResponseBadRequestToFile(std::string_view target, unsigned version) {
    http::response<http::string_body> response(http::status::bad_request, version);
    response.set(http::field::content_type, "text/plain");

    if (!target.empty() && target.front() == '/') {
        target.remove_prefix(1);
    }

    response.body() = "File " + std::string(target) + " not exist in root catalogue!";
    response.prepare_payload();
    return response;
}

http::response<http::string_body> FileRequestHandler::ResponseNoContent(std::string_view target, unsigned version) {
    http::response<http::string_body> response(http::status::no_content, version);
    response.set(http::field::content_type, "text/plain");

    if (!target.empty() && target.front() == '/') {
        target.remove_prefix(1);
    }

    response.body() = "File " + std::string(target) + " is empty!";
    response.prepare_payload();
    return response;
}

http::response<http::string_body> FileRequestHandler::InternalServerError(unsigned version) {
    http::response<http::string_body> response(http::status::internal_server_error, version);
    response.set(http::field::content_type, "text/plain");
    response.body() = "Error 505. Internal server error!";
    response.prepare_payload();
    return response;
}

std::string_view FileRequestHandler::ContentType(std::string file_extension) {
    std::string_view content_type;

    std::transform(file_extension.begin()
                   , file_extension.end()
                   , file_extension.begin(), [](unsigned char c) {
                       return std::tolower(c);
                   });

    if(file_extension == ".htm" || file_extension == ".html") {
        content_type = "text/html"sv;
    }
    else if(file_extension == ".css") {
        content_type = "text/css"sv;
    }
    else if(file_extension == ".txt") {
        content_type = "text/plain"sv;
    }
    else if(file_extension == ".js") {
        content_type = "text/javascript"sv;
    }
    else if(file_extension == ".json") {
        content_type = "application/json"sv;
    }
    else if(file_extension == ".xml") {
        content_type = "application/xml"sv;
    }
    else if(file_extension == ".png") {
        content_type = "image/png"sv;
    }
    else if(file_extension == ".jpg" || file_extension == ".jpe" || file_extension == ".jpeg") {
        content_type = "image/jpeg"sv;
    }
    else if(file_extension == ".gif") {
        content_type = "image/gif"sv;
    }
    else if(file_extension == ".bmp") {
        content_type = "image/bmp"sv;
    }
    else if(file_extension == ".ico") {
        content_type = "image/vnd.microsoft.icon"sv;
    }
    else if(file_extension == ".tif" || file_extension == ".tiff") {
        content_type = "image/tiff"sv;
    }
    else if(file_extension == ".svg" || file_extension == ".svgz") {
        content_type = "image/svg+xml"sv;
    }
    else if(file_extension == ".mp3") {
        content_type = "image/vnd.microsoft.icon"sv;
    }
    else {
        content_type = "application/octet-stream"sv;
    }
    return content_type;
}

std::string FileRequestHandler::DecodeUri(std::string_view uri_string) {
    std::string decode_string;
    decode_string.reserve(uri_string.size());

    for (size_t i = 0; i < uri_string.size(); ++i) {
        if (uri_string[i] == '%') {
            if (i + 2 < uri_string.size()) {
                char high = uri_string[i + 1];
                char low = uri_string[i + 2];

                auto hex_to_val = [](char ch) -> int {
                    if (ch >= '0' && ch <= '9') return ch - '0';
                    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                    return -1;
                };

                int val_high = hex_to_val(high);
                int val_low = hex_to_val(low);

                if (val_high != -1 && val_low != -1) {
                    decode_string += static_cast<char>((val_high << 4) + val_low);
                    i += 2;
                    continue;
                }
            }
        } else if (uri_string[i] == '+') {
            decode_string += ' ';
            continue;
        }
        decode_string += uri_string[i];
    }
    return decode_string;
}

bool FileRequestHandler::IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}
}  // namespace http_handler
