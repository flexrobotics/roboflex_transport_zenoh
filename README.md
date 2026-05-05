# roboflex.transport.zenoh

Roboflex transport nodes powered by [Zenoh](https://github.com/eclipse-zenoh/zenoh). Zenoh gives us pub/sub plus automatic discovery, so we can move `roboflex` messages across threads, processes, and machines without needing to manage a broker manually.

```
any node -> ZenohPublisher ==THE_NETWORK==> ZenohSubscriber -> any node
```

## Status

This transport mirrors the ZMQ/MQTT bindings but uses `zenoh-c` under the hood, with Zenoh discovery/scouting enabled by default. Nodes:

- `ZenohPublisher`: publishes raw `roboflex` messages to a Zenoh key expression.
- `ZenohSubscriber`: subscribes to a key expression and signals incoming messages. It runs its own thread (call `start()`).
- `ZenohRequestClient`: sends one roboflex message as a Zenoh query payload and blocks for a reply.
- `ZenohRequestServer`: declares a Zenoh queryable and replies with handler-returned roboflex messages.

## Build (C++)

```
mkdir build && cd build
cmake ..
make
```

Python bindings are off by default for plain C++ CMake builds. To build them with CMake directly:

```
cmake -S . -B build -DBUILD_ROBOFLEX_TRANSPORT_ZENOH_PYTHON_EXT=ON
```

## Run Tests

Tests are plain C++ executables registered with CTest; no separate testing library is required.

```
cmake -S . -B build
cmake --build build --target test_zenoh_transport
ctest --test-dir build --output-on-failure
```

If Zenoh cannot open a local session in the current environment, the runtime test exits with CTest's configured skip code instead of failing the build.

## Example

See `examples/pub_sub_0_cpp.cpp` for a simple publisher/subscriber wiring:

```c++
auto session = transportzenoh::MakeZenohSession();
transportzenoh::ZenohPublisher pub(session, "roboflex/demo");
transportzenoh::ZenohSubscriber sub(session, "roboflex/demo");
sub.start();
```

`examples/pub_sub_config_cpp.cpp` shows a more explicit setup: setting mode/explicit endpoints, turning off multicast scouting, using express/priority publisher settings, and allowing any origin on the subscriber.

## Python

Python bindings mirror the C++ API (`ZenohSession`, `ZenohPublisher`, `ZenohSubscriber`). Build via `pip install .` from the repo root after building the C++ library, like the other transport packages.

## RPC / Request-Reply

For request-response workloads, use `ZenohRequestClient` and `ZenohRequestServer` instead of pub/sub. These are built on Zenoh query/queryable semantics.

```c++
auto session = transportzenoh::MakeZenohSession();

auto server = std::make_shared<transportzenoh::ZenohRequestServer>(
    session,
    "roboflex/rpc",
    "ZenohRequestServer",
    [](core::MessagePtr request) {
        return request;
    });
server->start();

transportzenoh::ZenohRequestClient client(
    session,
    "roboflex/rpc",
    "ZenohRequestClient",
    1000);

auto reply = client.call(request_message, 1000);
```

`ZenohRequestClient` is also a node: when it receives a message, it performs a blocking query and signals the first successful reply downstream.

## Configuration knobs

- `ZenohSession`: accepts an optional `config_json` (JSON/JSON5 matching the [Zenoh config schema](https://zenoh.io/docs/getting-started/configuration/)), explicit `mode` (`peer`, `client`, `router`), `connect_endpoints`, `listen_endpoints`, and `enable_multicast_scouting` toggle.
- `ZenohPublisher`: opts for `express` (no batching), `priority`, and `allowed_destination` (see `zc_locality_t`).
- `ZenohSubscriber`: accepts `allowed_origin` (also `zc_locality_t`).
- `ZenohRequestClient`: accepts query `target`, `express`, `priority`, `allowed_destination`, and timeout.
- `ZenohRequestServer`: accepts `allowed_origin` and queryable completeness.
- Key expressions follow Zenoh’s rules and support wildcards; see [Zenoh key expressions](https://zenoh.io/docs/manual/abstractions/#key-expressions).

## System Dependencies

None beyond what the build pulls automatically: `roboflex_core` and `zenoh-c` are fetched via CMake `FetchContent`.
