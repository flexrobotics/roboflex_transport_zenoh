#include "roboflex_transport_zenoh/zenoh_nodes.h"
#include "roboflex_core/message_backing_store.h"
#include "roboflex_core/util/utils.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <zenoh_constants.h>

namespace roboflex {
namespace transportzenoh {

namespace {

std::string json_array_from_strings(const std::vector<std::string>& entries) {
    std::string out = "[";
    for (size_t i = 0; i < entries.size(); ++i) {
        out += "\"";
        out += entries[i];
        out += "\"";
        if (i + 1 < entries.size()) {
            out += ",";
        }
    }
    out += "]";
    return out;
}

} // namespace


// -- ZenohSession --

ZenohSession::ZenohSession(
    const string& config_json,
    const string& mode,
    const std::vector<string>& connect_endpoints,
    const std::vector<string>& listen_endpoints,
    bool enable_multicast_scouting):
    session_open(false)
{
    z_owned_config_t config{};
    if (!config_json.empty()) {
        if (zc_config_from_str(&config, config_json.c_str()) < 0) {
            throw std::runtime_error("ZenohSession failed to load config json");
        }
    } else {
        if (z_config_default(&config) < 0) {
            throw std::runtime_error("ZenohSession failed to create default config");
        }
    }

    if (!enable_multicast_scouting) {
        if (zc_config_insert_json5(z_config_loan_mut(&config), Z_CONFIG_MULTICAST_SCOUTING_KEY, "false") < 0) {
            throw std::runtime_error("ZenohSession failed to disable multicast scouting");
        }
    }

    if (!mode.empty()) {
        auto mode_json = std::string("\"") + mode + "\"";
        if (zc_config_insert_json5(z_config_loan_mut(&config), Z_CONFIG_MODE_KEY, mode_json.c_str()) < 0) {
            throw std::runtime_error("ZenohSession failed to set mode");
        }
    }

    if (!connect_endpoints.empty()) {
        auto connects_json = json_array_from_strings(connect_endpoints);
        if (zc_config_insert_json5(z_config_loan_mut(&config), Z_CONFIG_CONNECT_KEY, connects_json.c_str()) < 0) {
            throw std::runtime_error("ZenohSession failed to set connect endpoints");
        }
    }

    if (!listen_endpoints.empty()) {
        auto listens_json = json_array_from_strings(listen_endpoints);
        if (zc_config_insert_json5(z_config_loan_mut(&config), Z_CONFIG_LISTEN_KEY, listens_json.c_str()) < 0) {
            throw std::runtime_error("ZenohSession failed to set listen endpoints");
        }
    }

    z_open_options_t opts{};
    z_open_options_default(&opts);

    z_owned_session_t sess{};
    if (z_open(&sess, z_move(config), &opts) < 0) {
        throw std::runtime_error("ZenohSession failed to open session");
    }

    session = sess;
    session_open = true;
}

ZenohSession::~ZenohSession()
{
    if (session_open) {
        z_close_options_t opts{};
        z_close_options_default(&opts);
        z_close(loaned_session_mut(), &opts);
        z_drop(z_move(session));
    }
}


// -- ZenohPublisher --

ZenohPublisher::ZenohPublisher(
    ZenohSessionPtr session,
    const string& key_expression,
    const string& name,
    bool express,
    z_priority_t priority,
    zc_locality_t allowed_destination):
        core::Node(name),
        session(session),
        key_expression(key_expression),
        express(express),
        priority(priority),
        allowed_destination(allowed_destination),
        publisher_constructed(false)
{
}

ZenohPublisher::~ZenohPublisher()
{
    destroy_publisher();
}

void ZenohPublisher::ensure_publisher()
{
    if (!publisher_constructed) {
        if (z_keyexpr_from_str(&keyexpr, key_expression.c_str()) < 0) {
            throw std::runtime_error("ZenohPublisher failed to parse key expression " + key_expression);
        }

        z_publisher_options_t opts{};
        z_publisher_options_default(&opts);
        opts.is_express = express;
        opts.priority = priority;
        opts.allowed_destination = allowed_destination;

        if (z_declare_publisher(
                session->loaned_session(),
                &publisher,
                z_loan(keyexpr),
                &opts) < 0) {
            throw std::runtime_error("ZenohPublisher failed to declare publisher for " + key_expression);
        }
        publisher_constructed = true;
    }
}

void ZenohPublisher::destroy_publisher()
{
    if (publisher_constructed) {
        z_drop(z_move(publisher));
        z_drop(z_move(keyexpr));
        publisher_constructed = false;
    }
}

void ZenohPublisher::receive(core::MessagePtr m)
{
    ensure_publisher();

    if (m == nullptr) {
        signal(nullptr);
        return;
    }

    auto payload_copy = new uint8_t[m->get_raw_size()];
    std::memcpy(payload_copy, m->get_raw_data(), m->get_raw_size());

    z_owned_bytes_t bytes{};
    if (z_bytes_from_buf(&bytes, payload_copy, m->get_raw_size(), [](void* data, void*) {
        delete[] static_cast<uint8_t*>(data);
    }, nullptr) < 0) {
        delete[] payload_copy;
        std::cout << "ZenohPublisher failed to wrap payload bytes" << std::endl;
        return;
    }

    z_publisher_put_options_t opts{};
    z_publisher_put_options_default(&opts);

    auto rc = z_publisher_put(
        z_loan(publisher),
        z_move(bytes),
        &opts);

    if (rc != 0) {
        std::cout << "ZenohPublisher failed to publish on key expression \"" << key_expression << "\" rc=" << rc << std::endl;
    }

    signal(m);
}


// -- ZenohSubscriber --

ZenohSubscriber::ZenohSubscriber(
    ZenohSessionPtr session,
    const string& key_expression,
    const string& name,
    size_t max_queued_msgs,
    int wait_timeout_milliseconds,
    zc_locality_t allowed_origin):
        core::RunnableNode(name),
        session(session),
        key_expression(key_expression),
        max_queued_msgs(max_queued_msgs),
        wait_timeout_milliseconds(wait_timeout_milliseconds),
        allowed_origin(allowed_origin),
        subscriber_constructed(false)
{
}

ZenohSubscriber::~ZenohSubscriber()
{
    destroy_subscriber();
}

void ZenohSubscriber::subscription_callback(z_loaned_sample_t* sample, void* arg)
{
    if (sample == nullptr) {
        return;
    }

    ZenohSubscriber* self = static_cast<ZenohSubscriber*>(arg);

    const z_loaned_bytes_t* payload_bytes = z_sample_payload(sample);
    size_t len = z_bytes_len(payload_bytes);
    std::vector<uint8_t> bytes(len);

    auto reader = z_bytes_get_reader(payload_bytes);
    z_bytes_reader_read(&reader, bytes.data(), len);

    auto payload = std::make_shared<core::MessageBackingStoreVector>(std::move(bytes));

    auto msg = std::make_shared<core::Message>(payload);

    {
        std::unique_lock lock(self->queue_mutex);
        if (self->queue.size() >= self->max_queued_msgs) {
            self->queue.pop_front();
        }
        self->queue.push_back(msg);
    }
    self->queue_cv.notify_one();
}

void ZenohSubscriber::ensure_subscriber()
{
    if (!subscriber_constructed) {
        if (z_keyexpr_from_str(&keyexpr, key_expression.c_str()) < 0) {
            throw std::runtime_error("ZenohSubscriber failed to parse key expression " + key_expression);
        }

        z_owned_closure_sample_t closure{};
        z_closure_sample(&closure, subscription_callback, nullptr, this);

        z_subscriber_options_t opts{};
        z_subscriber_options_default(&opts);
        opts.allowed_origin = allowed_origin;

        if (z_declare_subscriber(
                session->loaned_session(),
                &subscriber,
                z_loan(keyexpr),
                z_move(closure),
                &opts) < 0) {
            throw std::runtime_error("ZenohSubscriber failed to declare subscriber for " + key_expression);
        }
        subscriber_constructed = true;
    }
}

void ZenohSubscriber::destroy_subscriber()
{
    if (subscriber_constructed) {
        z_undeclare_subscriber(z_move(subscriber));
        z_drop(z_move(keyexpr));
        subscriber_constructed = false;
    }
}

core::MessagePtr ZenohSubscriber::pull(int timeout_milliseconds)
{
    ensure_subscriber();

    std::unique_lock lock(queue_mutex);
    if (queue.empty()) {
        queue_cv.wait_for(lock, std::chrono::milliseconds(timeout_milliseconds));
    }

    if (queue.empty()) {
        return nullptr;
    }

    auto m = queue.front();
    queue.pop_front();
    return m;
}

void ZenohSubscriber::produce(int timeout_milliseconds)
{
    auto m = pull(timeout_milliseconds);
    if (m != nullptr) {
        signal(m);
    }
}

void ZenohSubscriber::child_thread_fn()
{
    ensure_subscriber();

    while (!stop_requested()) {
        auto m = pull(wait_timeout_milliseconds);
        if (m != nullptr) {
            signal(m);
        }
    }

    destroy_subscriber();
}

} // namespace transportzenoh
} // namespace roboflex
