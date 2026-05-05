#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "roboflex_core/core_messages/core_messages.h"
#include "roboflex_core/util/utils.h"
#include "roboflex_transport_zenoh/zenoh_nodes.h"

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAILED: " #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        return 1; \
    } \
} while (0)

using namespace roboflex;

namespace {

core::MessagePtr make_string_message(const std::string& value)
{
    return std::make_shared<core::StringMessage>("test_string", value);
}

int test_pub_sub_round_trip()
{
    auto session = transportzenoh::MakeZenohSession();
    const std::string key = "roboflex/tests/pub_sub_round_trip";

    transportzenoh::ZenohPublisher publisher(session, key);
    transportzenoh::ZenohSubscriber subscriber(session, key, "TestZenohSubscriber", 100, 10);

    // Declare the subscriber before publishing.
    (void) subscriber.pull(1);
    core::sleep_ms(100);

    core::MessagePtr received = nullptr;
    for (int i = 0; i < 50 && received == nullptr; ++i) {
        publisher.publish(make_string_message("zenoh-pub-sub"));
        received = subscriber.pull(50);
        core::sleep_ms(10);
    }

    REQUIRE(received != nullptr);
    core::StringMessage decoded(*received);
    REQUIRE(decoded.message() == "zenoh-pub-sub");
    return 0;
}

int test_request_server_round_trip()
{
    auto session = transportzenoh::MakeZenohSession();
    const std::string key = "roboflex/tests/request_server_round_trip";

    transportzenoh::ZenohRequestServer server(
        session,
        key,
        "TestZenohRequestServer",
        [](core::MessagePtr request) {
            core::StringMessage decoded(*request);
            return make_string_message("reply:" + decoded.message());
        });
    transportzenoh::ZenohRequestClient client(
        session,
        key,
        "TestZenohRequestClient",
        1000);

    server.start();
    core::sleep_ms(100);

    auto response = client.call(make_string_message("request"), 1000);
    server.stop();

    REQUIRE(response != nullptr);
    core::StringMessage decoded(*response);
    REQUIRE(decoded.message() == "reply:request");
    return 0;
}

int test_request_timeout()
{
    auto session = transportzenoh::MakeZenohSession();
    transportzenoh::ZenohRequestClient client(
        session,
        "roboflex/tests/request_timeout_without_server",
        "TestZenohRequestTimeoutClient",
        50);

    auto response = client.call(make_string_message("request"), 50);
    REQUIRE(response == nullptr);
    return 0;
}

} // namespace

int main()
{
    try {
        REQUIRE(test_pub_sub_round_trip() == 0);
        REQUIRE(test_request_server_round_trip() == 0);
        REQUIRE(test_request_timeout() == 0);
    } catch (const std::runtime_error& e) {
        const std::string reason = e.what();
        if (reason.find("ZenohSession failed to open session") != std::string::npos) {
            std::cerr << "SKIPPED: " << reason << std::endl;
            return 77;
        }
        std::cerr << "FAILED: unexpected runtime_error: " << reason << std::endl;
        return 1;
    }
    return 0;
}
