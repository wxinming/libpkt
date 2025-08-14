#include <exception>
#include <iostream>
#include <stdint.h>
#include <thread>
#include "json.hpp"
#include "libpkt.h"

#define MAX_THREADS_COUNT 1
#define MAX_CYCLE_COUNT 2

int main()
{
    std::cout << "begin!" << std::endl;

    Packet pkt(BIG_ENDIAN_MODE, 0xffaa, 2, 4, 2, 0x12345678);
    std::thread* arrs[6] = { nullptr };

    auto task = [&](int id){
        for (int i = 0; i < MAX_CYCLE_COUNT; ++i) {
            nlohmann::json json;
            json["app_name"] = "a.out";
            json["app_size"] = 1024 + i;
            pkt.set_command(i);
            pkt.append_payload(json);
            pkt.append_payload(i);
            pkt.serialize();

            auto data = pkt.data();
            auto data_size = pkt.data_length();
            for (int i = 0; i < data_size; ++i) {
                if (i == 0) {
                    printf("data:%02x ", data[i]);
                }
                else if (i != data_size -1) {
                    printf("%02x ", data[i]);
                }
                else {
                    printf("%02x\n", data[i]);
                }
            }

            auto payload = pkt.payload();
            auto payload_size = pkt.payload_length();

            for (int i = 0; i < payload_size; ++i) {
                if (i == 0) {
                    printf("payload:%02x ", payload[i]);
                }
                else if (i != payload_size -1) {
                    printf("%02x ", payload[i]);
                }
                else {
                    printf("%02x\n", payload[i]);
                }
            }

            pkt.set_data(data, data_size);
            pkt.unserialize();
            try {
                json = pkt.fetch_payload<nlohmann::json>();
                auto i = pkt.fetch_payload<int>();
                std::cout << id << "->" << pkt.command() << "->" << json.dump() << "->" << i << std::endl;
            }
            catch(const std::exception& e) {
                std::cout << e.what() << std::endl;
            }
        }
    };

    for (int i = 0; i < MAX_THREADS_COUNT; ++i) {
        auto t = new std::thread(task, i);
        arrs[i] = t;
    }

    for (int i = 0; i < MAX_THREADS_COUNT; ++i) {
        arrs[i]->join();
        delete arrs[i];
    }


    std::cout << "end!" << std::endl;
    return 0;
}

