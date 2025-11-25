#ifndef ROBOFLEX_TRANSPORT_ZENOH_NODES__H
#define ROBOFLEX_TRANSPORT_ZENOH_NODES__H

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#if __has_include(<zenoh.h>)
#include <zenoh.h>
#elif __has_include(<zenoh/zenoh.h>)
#include <zenoh/zenoh.h>
#else
#error "zenoh headers not found; please ensure zenoh-c is available"
#endif
#include <zenoh_constants.h>
#include "roboflex_core/core.h"

namespace roboflex {
namespace transportzenoh {

using std::string, std::shared_ptr;

/**
 * A thin wrapper around a zenoh session.
 *
 * @param config_json Optional JSON/JSON5 configuration string passed to zenoh. This matches the
 *        format described in the zenoh docs (e.g. {"mode":"peer","connect":{"endpoints":["tcp/localhost:7447"]}}).
 *        See https://zenoh.io/docs/getting-started/configuration/ for available keys.
 * @param mode Optional explicit mode ("peer", "client", "router") inserted into the config when provided.
 * @param connect_endpoints Optional list of connect endpoints inserted into the config.
 * @param listen_endpoints Optional list of listen endpoints inserted into the config.
 * @param enable_multicast_scouting Set false to disable zenoh multicast discovery; when true the
 *        default zenoh multicast scouting remains enabled.
 */
class ZenohSession {
public:
    ZenohSession(
        const string& config_json = "",
        const string& mode = "",
        const std::vector<string>& connect_endpoints = {},
        const std::vector<string>& listen_endpoints = {},
        bool enable_multicast_scouting = true);
    ~ZenohSession();

    z_owned_session_t& owned_session() { return session; }
    const z_loaned_session_t* loaned_session() const { return z_loan(session); }
    z_loaned_session_t* loaned_session_mut() { return z_loan_mut(session); }
    bool is_open() const { return session_open; }

private:
    z_owned_session_t session;
    bool session_open;
};

using ZenohSessionPtr = shared_ptr<ZenohSession>;

inline ZenohSessionPtr MakeZenohSession(
    const string& config_json = "",
    const string& mode = "",
    const std::vector<string>& connect_endpoints = {},
    const std::vector<string>& listen_endpoints = {},
    bool enable_multicast_scouting = true) {
    return std::make_shared<ZenohSession>(config_json, mode, connect_endpoints, listen_endpoints, enable_multicast_scouting);
}


// Publishes roboflex messages on a zenoh key expression.
//
// @param key_expression Zenoh key expression to publish to (e.g. "roboflex/demo/*" per zenoh rules).
//        Key expressions are the zenoh topic-like addressing scheme:
//        https://zenoh.io/docs/manual/abstractions/#key-expressions
// @param express When true, disable batching for lower latency.
// @param priority Zenoh message priority for this publisher.
// @param allowed_destination Limit destinations (see zc_locality_t).
//
class ZenohPublisher: public core::Node {
public:
    ZenohPublisher(
        ZenohSessionPtr session,
        const string& key_expression,
        const string& name = "ZenohPublisher",
        bool express = false,
        z_priority_t priority = Z_PRIORITY_DEFAULT,
        zc_locality_t allowed_destination = zc_locality_default());

    ~ZenohPublisher();

    void receive(core::MessagePtr m) override;
    void publish(core::MessagePtr m) { this->signal_self(m); }

    const string& get_key_expression() const { return key_expression; }

protected:
    void ensure_publisher();
    void destroy_publisher();

    ZenohSessionPtr session;
    string key_expression;
    bool express;
    z_priority_t priority;
    zc_locality_t allowed_destination;
    bool publisher_constructed;
    z_owned_publisher_t publisher;
    z_owned_keyexpr_t keyexpr;
};

using ZenohPublisherPtr = shared_ptr<ZenohPublisher>;


/**
 * Subscribes to a zenoh key expression and signals incoming messages.
 *
 * @param key_expression Zenoh key expression to subscribe to. Wildcards follow zenoh rules.
 * @param max_queued_msgs Local queue limit before oldest messages are dropped.
 * @param wait_timeout_milliseconds Poll interval for the worker thread when waiting for data.
 * @param allowed_origin Restrict accepted publications (see zc_locality_t).
 */
class ZenohSubscriber: public core::RunnableNode {
public:
    ZenohSubscriber(
        ZenohSessionPtr session,
        const string& key_expression,
        const string& name = "ZenohSubscriber",
        size_t max_queued_msgs = 1000,
        int wait_timeout_milliseconds = 50,
        zc_locality_t allowed_origin = zc_locality_default());

    ~ZenohSubscriber();

    core::MessagePtr pull(int timeout_milliseconds = 10);
    void produce(int timeout_milliseconds = 10);

    const string& get_key_expression() const { return key_expression; }
    size_t get_max_queued_msgs() const { return max_queued_msgs; }
    int get_wait_timeout_milliseconds() const { return wait_timeout_milliseconds; }

protected:
    void ensure_subscriber();
    void destroy_subscriber();

    void child_thread_fn() override;

    static void subscription_callback(z_loaned_sample_t* sample, void* arg);

    ZenohSessionPtr session;
    string key_expression;
    size_t max_queued_msgs;
    int wait_timeout_milliseconds;
    zc_locality_t allowed_origin;

    bool subscriber_constructed;
    z_owned_subscriber_t subscriber;
    z_owned_keyexpr_t keyexpr;

    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::deque<core::MessagePtr> queue;
};

using ZenohSubscriberPtr = shared_ptr<ZenohSubscriber>;

} // namespace transportzenoh
} // namespace roboflex

#endif // ROBOFLEX_TRANSPORT_ZENOH_NODES__H
