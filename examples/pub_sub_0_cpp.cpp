#include <iostream>
#include "roboflex_core/core_messages/core_messages.h"
#include "roboflex_core/core_nodes/core_nodes.h"
#include "roboflex_core/util/utils.h"
#include "roboflex_transport_zenoh/zenoh_nodes.h"

using namespace roboflex;

int main()
{
    nodes::FrequencyGenerator frequency_generator(2.0);

    auto tensor_creator = nodes::MapFun([](core::MessagePtr m) {
        xt::xtensor<double, 2> d = xt::ones<double>({2, 3}) * m->message_counter();
        return core::TensorMessage<double, 2>::Ptr(d);
    });

    auto zenoh_session = transportzenoh::MakeZenohSession();
    auto zenoh_pub = transportzenoh::ZenohPublisher(zenoh_session, "roboflex/demo");
    auto zenoh_sub = transportzenoh::ZenohSubscriber(zenoh_session, "roboflex/demo");

    auto message_printer1 = nodes::MessagePrinter("SENDING MESSAGE:");
    auto message_printer2 = nodes::MessagePrinter("RECEIVED MESSAGE:");

    auto tensor_printer = nodes::CallbackFun([](core::MessagePtr m) {
        xt::xtensor<double, 2> d = core::TensorMessage<double, 2>(*m).value();
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
