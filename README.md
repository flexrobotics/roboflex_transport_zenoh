# roboflex.transport.zenoh

Roboflex transport nodes powered by [Zenoh](https://github.com/eclipse-zenoh/zenoh). Zenoh gives us pub/sub plus automatic discovery, so we can move `roboflex` messages across threads, processes, and machines without needing to manage a broker manually.

```
any node -> ZenohPublisher ==THE_NETWORK==> ZenohSubscriber -> any node
```

## Status

This transport mirrors the ZMQ/MQTT bindings but uses `zenoh-c` under the hood, with Zenoh discovery/scouting enabled by default. Nodes:

- `ZenohPublisher`: publishes raw `roboflex` messages to a Zenoh key expression.
- `ZenohSubscriber`: subscribes to a key expression and signals incoming messages. It runs its own thread (call `start()`).

## Build (C++)

```
mkdir build && cd build
cmake ..
make
```

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

## Configuration knobs

- `ZenohSession`: accepts an optional `config_json` (JSON/JSON5 matching the [Zenoh config schema](https://zenoh.io/docs/getting-started/configuration/)), explicit `mode` (`peer`, `client`, `router`), `connect_endpoints`, `listen_endpoints`, and `enable_multicast_scouting` toggle.
- `ZenohPublisher`: opts for `express` (no batching), `priority`, and `allowed_destination` (see `zc_locality_t`).
- `ZenohSubscriber`: accepts `allowed_origin` (also `zc_locality_t`).
- Key expressions follow Zenoh’s rules and support wildcards; see [Zenoh key expressions](https://zenoh.io/docs/manual/abstractions/#key-expressions).

## System Dependencies

None beyond what the build pulls automatically: `roboflex_core` and `zenoh-c` are fetched via CMake `FetchContent`.
