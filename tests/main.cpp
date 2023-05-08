#include "memucpp/memucpp.hpp"

auto main(uint32_t argc, char** argv) -> int32_t {
    memucpp::Memuc memuc;
    memuc.set_path("D:/Program Files/Microvirt/MEmu");
    auto vms = memuc.list_vms();

    for(auto const& vm : vms) {
        std::cout << std::format("vm {} {} {}", vm.index, vm.name, vm.status) << std::endl;
    }
}