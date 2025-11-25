#include <string>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include "roboflex_core/core.h"
#include "roboflex_core/pybindings.h"
#include "roboflex_transport_zenoh/zenoh_nodes.h"

namespace py = pybind11;

using namespace roboflex;
using namespace roboflex::transportzenoh;

PYBIND11_MODULE(roboflex_transport_zenoh_ext, m) {
    m.doc() = "roboflex_transport_zenoh_ext";

    py::class_<ZenohSession, std::shared_ptr<ZenohSession>>(m, "ZenohSession")
        .def(py::init([](const std::string& config_json,
                         const std::string& mode,
                         const std::vector<std::string>& connect_endpoints,
                         const std::vector<std::string>& listen_endpoints,
                         bool enable_multicast_scouting) {
                return MakeZenohSession(config_json, mode, connect_endpoints, listen_endpoints, enable_multicast_scouting);
            }),
            "Create a Zenoh session. Uses default discovery/scouting by default.",
            py::arg("config_json") = "",
            py::arg("mode") = "",
            py::arg("connect_endpoints") = std::vector<std::string>{},
            py::arg("listen_endpoints") = std::vector<std::string>{},
            py::arg("enable_multicast_scouting") = true)
    ;

    py::class_<ZenohPublisher, core::Node, std::shared_ptr<ZenohPublisher>>(m, "ZenohPublisher")
        .def(py::init<ZenohSessionPtr,
                      const std::string&,
                      const std::string&,
                      bool,
                      z_priority_t,
                      zc_locality_t>(),
            "Creates a ZenohPublisher on a key expression.",
            py::arg("session"),
            py::arg("key_expression"),
            py::arg("name") = "ZenohPublisher",
            py::arg("express") = false,
            py::arg("priority") = Z_PRIORITY_DEFAULT,
            py::arg("allowed_destination") = zc_locality_default())
        .def_property_readonly("key_expression", &ZenohPublisher::get_key_expression)
        .def("publish", &ZenohPublisher::publish)
        .def("publish", [](std::shared_ptr<ZenohPublisher> a, py::object m) {
            a->publish(dynoflex_from_object(m));
        })
    ;

    py::class_<ZenohSubscriber, core::RunnableNode, std::shared_ptr<ZenohSubscriber>>(m, "ZenohSubscriber")
        .def(py::init<ZenohSessionPtr,
                      const std::string&,
                      const std::string&,
                      size_t,
                      int,
                      zc_locality_t>(),
            "Creates a ZenohSubscriber listening on a key expression.",
            py::arg("session"),
            py::arg("key_expression"),
            py::arg("name") = "ZenohSubscriber",
            py::arg("max_queued_msgs") = 1000,
            py::arg("wait_timeout_milliseconds") = 50,
            py::arg("allowed_origin") = zc_locality_default())
        .def("pull", &ZenohSubscriber::pull,
            py::arg("timeout_milliseconds") = 10)
        .def("produce", &ZenohSubscriber::produce,
            py::arg("timeout_milliseconds") = 10)
        .def_property_readonly("key_expression", &ZenohSubscriber::get_key_expression)
        .def_property_readonly("max_queued_msgs", &ZenohSubscriber::get_max_queued_msgs)
        .def_property_readonly("wait_timeout_milliseconds", &ZenohSubscriber::get_wait_timeout_milliseconds)
    ;
}
