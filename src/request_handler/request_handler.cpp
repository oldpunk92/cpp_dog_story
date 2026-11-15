#include "request_handler.h"

namespace http_handler {

http::response<http::string_body> RequestHandler::InternalServerError(unsigned version) {
    http::response<http::string_body> response(http::status::internal_server_error, version);
    response.set(http::field::content_type, "text/plain");
    response.body() = "Error 505. Internal server error!";
    response.prepare_payload();
    return response;
}

http::response<http::string_body> RequestHandler::ResponseBadRequest(unsigned version) {
    http::response<http::string_body> response(http::status::bad_request, version);
    response.set(http::field::content_type, "text/plain");
    response.body() = "Bad Request: Invalid or empty target URI.";
    response.prepare_payload();
    return response;
}
}  // namespace http_handler
