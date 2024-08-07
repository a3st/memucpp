// Copyright © 2020-2024 Dmitriy Lukovenko. All rights reserved.

#include "memucpp.hpp"
#include <fstream>
#include <thread>

using namespace memucpp;

auto main(uint32_t argc, char** argv) -> int32_t
{
    memucpp::memuc_path = "D:/Program Files/Microvirt/MEmu/memuc.exe";
    Memuc memuc(0, VMConfig::Default());

    for (auto const& vm : memuc.list_vms())
    {
        std::cout << std::format("VM: {} {} {}", vm.index, vm.name, vm.enabled) << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto buffer = memuc.screen_cap();

    std::basic_ofstream<uint8_t> fs("test.bmp", std::ios::binary);
    fs.write(buffer.data(), buffer.size());
}