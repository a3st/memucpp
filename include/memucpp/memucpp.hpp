#pragma once

#include <iostream>
#include <vector>
#include <array>
#include <span>
#include <format>
#include <filesystem>
#include <string_view>
#include <ranges>
#include <charconv>

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

namespace memucpp {

size_t const MAX_INSTANCES = 50;

namespace internal {

inline auto process_execute(std::string_view const command) -> std::vector<uint8_t> {
    std::array<uint8_t, 256> buffer;
    std::vector<uint8_t> result;

    FILE* pipe = ::_popen(std::string(command.data(), command.size()).c_str(), "rb");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    try {
        while(pipe) {
            size_t const read_bytes = ::fread(buffer.data(), sizeof(uint8_t), 128, pipe);
            if(read_bytes > 0) {
                result.insert(result.end(), buffer.begin(), std::next(buffer.begin(), read_bytes));
            } else {
                break;
            }
        }
        _pclose(pipe);
    } catch(...) {
        _pclose(pipe);
    }
    return result;
}

inline auto to_utf_8(std::span<uint8_t const> const source) -> std::string {
    size_t size = MultiByteToWideChar(
        CP_ACP, 
        0, 
        reinterpret_cast<char const*>(source.data()), 
        static_cast<int32_t>(source.size()), 
        nullptr, 
        0
    );

    std::wstring wstr(size, '\0');
    MultiByteToWideChar(
        CP_ACP, 
        0, 
        reinterpret_cast<char const*>(source.data()), 
        static_cast<int32_t>(source.size()), 
        wstr.data(), 
        static_cast<int32_t>(wstr.size())
    );

    size = WideCharToMultiByte(
        CP_UTF8, 
        0, 
        wstr.data(), 
        static_cast<int32_t>(wstr.size()), 
        nullptr, 
        0, 
        nullptr, 
        nullptr
    );

    std::string dest(size, '\0');
    WideCharToMultiByte(
        CP_UTF8, 
        0, 
        wstr.data(), 
        static_cast<int32_t>(wstr.size()), 
        dest.data(), 
        static_cast<int32_t>(dest.size()), 
        nullptr, 
        nullptr
    );
    return dest;
}

inline auto to_lf_from_crlf(std::span<uint8_t const> const source) -> std::vector<uint8_t> {
    std::vector<uint8_t> out(source.size());
    size_t offset = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        if(source.size() > i + 1 && source[i] == 0x0d && source[i + 1] == 0x0a) {
            out[offset] = 0x0a;
            i++;
        } else {
            out[offset] = source[i];
        }
        offset += 1;
    }
    return out;
}

template<typename Type>
inline auto to(std::string_view const source) -> Type {
    Type dest;
    std::from_chars_result result = std::from_chars(source.data(), source.data() + source.size(), dest);
    if(result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
        throw std::invalid_argument("Invalid argument!");
    }
    return dest;
}

inline auto from_bytes(std::span<uint8_t const> const source) -> uint32_t {
    uint32_t dest;
    std::memcpy(&dest, source.data(), sizeof(uint32_t));
    return dest;
}

}

class error : public std::exception {
public:
    error(std::string_view const what_arg) : message(what_arg.data(), what_arg.size()) { }

    auto what() const -> char const* override {
        return message.c_str();
    }

private:
    std::string message;
};

enum class KeyCode : uint32_t {
    Menu = 0,
    Home = 3,
    Back = 4,
    VolumeUp = 24,
    VolumeDown = 25
};

struct VMInfo {
    uint16_t index;
    std::string name;
    uint16_t status;
};

struct ProcessInfo {
    std::string name;
};

class Memuc {
public:
    Memuc() = default;

    Memuc(Memuc const&) = delete;

    Memuc(Memuc&& other) : memuc_path(std::move(other.memuc_path)), image_data(std::move(other.image_data)) { }

    auto operator=(Memuc const&) -> Memuc& = delete;

    auto operator=(Memuc&& other) -> Memuc& {
        memuc_path = std::move(other.memuc_path);
        image_data = std::move(other.image_data);
        return *this;
    }

    auto set_path(std::filesystem::path const& dir_path) -> void {
        memuc_path = dir_path;
    }

    auto list_vms() -> std::vector<VMInfo> {
        std::string command = std::format("\"{}/memuc.exe\" listvms", memuc_path.string());
        auto output = internal::to_utf_8(internal::process_execute(command));

        auto lines = output | 
            std::ranges::views::split('\n') | 
            std::ranges::views::filter([&](auto const& element) { return element.size() > 1; });

        std::vector<VMInfo> out;

        for(auto const& line : lines) {
            auto args = line | std::ranges::views::split(',') | std::ranges::views::transform([](auto&& element) {
                return std::string_view(element.data(), element.size());
            }) | std::ranges::to<std::vector>();

            auto vm_info = VMInfo {
                .index = internal::to<uint16_t>(args[0]),
                .name = std::string(args[1].data(), args[1].size()),
                .status = internal::to<uint16_t>(args[3])
            };

            out.push_back(std::move(vm_info));
        }
        return out;
    }

    auto start_vm(uint16_t const vm_index) -> void {
        std::string command = std::format("\"{}/memuc.exe\" start -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("SUCCESS") != std::string::npos) {
            image_data[vm_index] = std::make_unique<uint8_t[]>(8 * 1024 * 1024);
        } else {
            throw error("MEmuc is not connected");
        }
    }

    auto stop_vm(uint16_t const vm_index) -> void {
        std::string command = std::format("\"{}/memuc.exe\" stop -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already") != std::string::npos) {
            image_data[vm_index].reset();
        } else {
            throw error("MEmuc is not connected");
        }
    }

    auto reboot_vm(uint16_t const vm_index) -> void {
        std::string command = std::format("\"{}/memuc.exe\" reboot -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already connected") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto start_app(uint16_t const vm_index, std::string_view const package_name) -> void {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell monkey -p {} 1", memuc_path.string(), vm_index, package_name);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already connected") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto stop_app(uint16_t const vm_index, std::string_view const package_name) -> void {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell monkey -p {} 0", memuc_path.string(), vm_index, package_name);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto trigger_key(uint16_t const vm_index, KeyCode const key_code) -> void {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input keyevent {}", memuc_path.string(), vm_index, static_cast<uint32_t>(key_code));
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto trigger_swipe(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const start, std::tuple<uint32_t, uint32_t> const end, uint32_t const speed) -> void {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input swipe {} {} {} {} {}", 
            memuc_path.string(), vm_index, std::get<0>(start), std::get<1>(start), std::get<0>(end), std::get<1>(end), speed);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto trigger_click(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const pos) -> void {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input tap {} {}", memuc_path.string(), vm_index, std::get<0>(pos), std::get<1>(pos));
        auto output = internal::to_utf_8(internal::process_execute(command));

        if(output.find("already") == std::string::npos) {
            throw error("MEmuc is not connected");
        }
    }

    auto screen_cap(uint16_t const vm_index) -> std::span<uint8_t const> {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell screencap", memuc_path.string(), vm_index);
        auto output = internal::process_execute(command);

        auto message = internal::to_utf_8(std::span<uint8_t const>(output.data(), output.data() + 40));
        if(message.find("already") == std::string::npos) {
            throw error("MEmuc is not connected");
        }

        auto data = internal::to_lf_from_crlf(std::span<uint8_t const>(output.data() + 40, output.size()));

        size_t offset = 0;

        uint32_t const width = internal::from_bytes(std::span<uint8_t const>(data.data() + offset, 4));
        offset += sizeof(uint32_t);
        uint32_t const height = internal::from_bytes(std::span<uint8_t const>(data.data() + offset, 4));
        offset += sizeof(uint32_t);
        uint32_t const pf = internal::from_bytes(std::span<uint8_t const>(data.data() + offset, 4));
        offset += sizeof(uint32_t);

        auto bfh = BITMAPFILEHEADER {};
        bfh.bfType = 0x4D42;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            
        auto bih = BITMAPINFOHEADER {};
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = width;
        bih.biHeight = height;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        bih.biSizeImage = width * height * 3;

        uint32_t const alignment = 4;
        uint32_t const mask = alignment - 1;
        uint32_t const align_width = (width * 3) + (-int32_t(width * 3) & mask);

        bfh.bfSize = align_width * height + bfh.bfOffBits;

        std::vector<uint8_t> buffer(width * 3);

        for(uint32_t i = 0; i < height; ++i) {
            for(uint32_t j = 0; j < width; ++j) {
                buffer[(j * 3) + 0] = data[offset + j * 4 + 2];
                buffer[(j * 3) + 1] = data[offset + j * 4 + 1];
                buffer[(j * 3) + 2] = data[offset + j * 4 + 0];
            }

            std::memcpy(image_data[vm_index].get() + bfh.bfOffBits + ((height - 1) - i) * align_width, buffer.data(), buffer.size());
            offset += width * 4;
        }

        std::memcpy(image_data[vm_index].get(), &bfh, sizeof(BITMAPFILEHEADER));
        std::memcpy(image_data[vm_index].get() + sizeof(BITMAPFILEHEADER), &bih, sizeof(BITMAPINFOHEADER));

        return std::span<uint8_t const>(image_data[vm_index].get(), bfh.bfSize);
    }

private:
    std::filesystem::path memuc_path;
    std::array<std::unique_ptr<uint8_t[]>, MAX_INSTANCES> image_data;
};

}