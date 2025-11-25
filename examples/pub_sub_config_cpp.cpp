#include <iostream>
#include "roboflex_core/core_messages/core_messages.h"
#include "roboflex_core/core_nodes/core_nodes.h"
#include "roboflex_core/util/utils.h"
#include "roboflex_transport_zenoh/zenoh_nodes.h"
#include <vector>

using namespace roboflex;

int main()
{
    // Explicit config: peer mode, connect + listen endpoints, multicast scouting disabled.
    auto session = std::make_shared<transportzenoh::ZenohSession>(
        "",
        "peer",
        std::vector<std::string>{"tcp/127.0.0.1:7447"},
        std::vector<std::string>{"tcp/0.0.0.0:7447"},
        false);

    nodes::FrequencyGenerator frequency_generator(1.0);

    auto tensor_creator = nodes::MapFun([](core::MessagePtr m) {
        xt::xtensor<double, 1> d = xt::ones<double>({3}) * m->message_counter();
        return core::TensorMessage<double, 1>::Ptr(d);
    });

    // Express publisher with elevated priority, and subscriber allowing any origin.
    auto zenoh_pub = transportzenoh::ZenohPublisher(
        session, "roboflex/config_demo", "ZenohPublisherExpress", true, Z_PRIORITY_INTERACTIVE_HIGH);
    auto zenoh_sub = transportzenoh::ZenohSubscriber(
        session, "roboflex/config_demo", "ZenohSubscriberAny", 1000, 50, ZC_LOCALITY_ANY);

    auto message_printer1 = nodes::MessagePrinter("SENDING MESSAGE:");
    auto message_printer2 = nodes::MessagePrinter("RECEIVED MESSAGE:");

    auto tensor_printer = nodes::CallbackFun([](core::MessagePtr m) {
        xt::xtensor<double, 1> d = core::TensorMessage<double, 1>(*m).value();
        std::cout << "RECEIVED TENSOR:" << std::endl << d << std::endl;
    });

    frequency_generator > tensor_creator > message_printer1 > zenoh_pub;
    zenoh_sub > message_printer2 > tensor_printer;

    frequency_generator.start();
    zenoh_sub.start();

    sleep_ms(3000);

    frequency_generator.stop();
    zenoh_sub.stop();

    std::cout << "DONE" << std::endl;
    return 0;
}
