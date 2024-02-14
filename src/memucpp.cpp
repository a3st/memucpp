// Copyright © 2022-2024 Dmitriy Lukovenko. All rights reserved.

#include "memucpp.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

namespace memucpp
{
    namespace internal
    {
        auto process_execute(std::string_view const command) -> std::vector<uint8_t>
        {
            std::array<uint8_t, 256> buffer;
            std::vector<uint8_t> result;

            FILE* pipe = ::_popen(std::string(command.data(), command.size()).c_str(), "rb");
            if (!pipe)
            {
                throw std::runtime_error("popen() failed");
            }

            try
            {
                while (pipe)
                {
                    size_t const read_bytes = ::fread(buffer.data(), sizeof(uint8_t), 128, pipe);
                    if (read_bytes > 0)
                    {
                        result.insert(result.end(), buffer.begin(), std::next(buffer.begin(), read_bytes));
                    }
                    else
                    {
                        break;
                    }
                }
                _pclose(pipe);
            }
            catch (...)
            {
                _pclose(pipe);
            }
            return result;
        }

        auto to_utf_8(std::span<uint8_t const> const source) -> std::string
        {
            size_t size = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<char const*>(source.data()),
                                              static_cast<int32_t>(source.size()), nullptr, 0);

            std::wstring wstr(size, '\0');
            MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<char const*>(source.data()),
                                static_cast<int32_t>(source.size()), wstr.data(), static_cast<int32_t>(wstr.size()));

            size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int32_t>(wstr.size()), nullptr, 0, nullptr,
                                       nullptr);

            std::string dest(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int32_t>(wstr.size()), dest.data(),
                                static_cast<int32_t>(dest.size()), nullptr, nullptr);
            return dest;
        }

        auto to_uint32_t(std::string_view const source) -> uint32_t
        {
            uint32_t dest;
            std::from_chars_result result = std::from_chars(source.data(), source.data() + source.size(), dest);
            if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
            {
                throw std::invalid_argument("Invalid argument");
            }
            return dest;
        }

        auto from_bytes(std::span<uint8_t const> const source) -> uint32_t
        {
            uint32_t dest;
            std::memcpy(&dest, source.data(), sizeof(uint32_t));
            return dest;
        }
    } // namespace internal

    auto Memuc::list_vms() -> std::vector<VMInfo>
    {
        std::string command = std::format("\"{}/memuc.exe\" listvms", memuc_path.string());
        auto output = internal::to_utf_8(internal::process_execute(command));

        auto lines = output | std::views::split('\n') |
                     std::views::filter([&](auto const& element) { return element.size() > 1; });

        std::vector<VMInfo> out;

        for (auto const& line : lines)
        {
            auto args =
                line | std::views::split(',') |
                std::views::transform([](auto&& element) { return std::string_view(element.data(), element.size()); }) |
                std::ranges::to<std::vector>();

            auto vm_info = VMInfo{.index = static_cast<uint16_t>(internal::to_uint32_t(args[0])),
                                  .name = std::string(args[1].data(), args[1].size()),
                                  .status = static_cast<uint16_t>(internal::to_uint32_t(args[3]))};

            out.push_back(std::move(vm_info));
        }
        return out;
    }

    auto Memuc::start_vm(uint16_t const vm_index) -> void
    {
        std::string command = std::format("\"{}/memuc.exe\" start -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") != std::string::npos)
        {
            image_data[vm_index].resize(8 * 1024 * 1024);
        }
        else
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::stop_vm(uint16_t const vm_index) -> void
    {
        std::string command = std::format("\"{}/memuc.exe\" stop -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") != std::string::npos)
        {
            image_data[vm_index].clear();
        }
    }

    auto Memuc::reboot_vm(uint16_t const vm_index) -> void
    {
        std::string command = std::format("\"{}/memuc.exe\" reboot -i {}", memuc_path.string(), vm_index);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::start_app(uint16_t const vm_index, std::string_view const package_name) -> void
    {
        std::string command =
            std::format("\"{}/memuc.exe\" -i {} startapp {}", memuc_path.string(), vm_index, package_name);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::stop_app(uint16_t const vm_index, std::string_view const package_name) -> void
    {
        std::string command =
            std::format("\"{}/memuc.exe\" -i {} stopapp {}", memuc_path.string(), vm_index, package_name);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_key(uint16_t const vm_index, KeyCode const key_code) -> void
    {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input keyevent {}", memuc_path.string(),
                                          vm_index, static_cast<uint32_t>(key_code));
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_swipe(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const start,
                              std::tuple<uint32_t, uint32_t> const end, uint32_t const speed) -> void
    {
        std::string command =
            std::format("\"{}/memuc.exe\" -i {} adb shell input swipe {} {} {} {} {}", memuc_path.string(), vm_index,
                        std::get<0>(start), std::get<1>(start), std::get<0>(end), std::get<1>(end), speed);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_zoom(uint16_t const vm_index, int32_t const factor) -> void
    {
        if (factor > 0)
        {
            std::string command = std::format("\"{}/memuc.exe\" zoomin -i {}", memuc_path.string(), vm_index);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }
        else
        {
            std::string command = std::format("\"{}/memuc.exe\" zoomout -i {}", memuc_path.string(), vm_index);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }
    }

    auto Memuc::trigger_click(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const pos) -> void
    {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input tap {} {}", memuc_path.string(),
                                          vm_index, std::get<0>(pos), std::get<1>(pos));
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::set_config_internal(uint16_t const vm_index, std::string_view const param, std::string const& value)
        -> void
    {
        std::string command =
            std::format("\"{}/memuc.exe\" setconfigex -i {} {} {}", memuc_path.string(), vm_index, param, value);
        auto output = internal::to_utf_8(internal::process_execute(command));

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::list_process(uint16_t const vm_index) -> std::vector<ProcessInfo>
    {
        std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell ps", memuc_path.string(), vm_index);
        auto output = internal::process_execute(command);

        auto message = internal::to_utf_8(std::span<uint8_t const>(output.data(), output.data() + 40));

        if (message.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }

        auto data = internal::to_utf_8(std::span<uint8_t const>(output.data() + 40, output.data() + output.size()));

        auto lines =
            data | std::views::split('\n') |
            std::views::transform([](auto&& element) { return std::string_view(element.data(), element.size()); }) |
            std::views::filter([](auto&& element) { return element.find("com.") != std::string::npos; }) |
            std::ranges::to<std::vector>() | std::views::drop(1) | std::views::reverse | std::views::drop(1) |
            std::views::reverse;

        std::vector<ProcessInfo> out;

        for (auto const& line : lines)
        {
            auto process_info =
                ProcessInfo{.name = std::string(line.begin() + line.find("com."), line.begin() + line.size())};

            out.push_back(process_info);
        }
        return out;
    }

    auto Memuc::screen_cap(uint16_t const vm_index) -> std::span<uint8_t const>
    {
        std::string command =
            std::format("\"{}/memuc.exe\" -i {} adb exec-out screencap", memuc_path.string(), vm_index);
        auto output = internal::process_execute(command);

        auto message = internal::to_utf_8(std::span<uint8_t const>(output.data(), output.data() + 40));

        if (message.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }

        std::spanstream stream(std::span<char>(reinterpret_cast<char*>(output.data()) + 40,
                                               reinterpret_cast<char*>(output.data()) + output.size()),
                               std::ios::in | std::ios::binary);

        std::vector<uint8_t> buffer(1024);

        stream.read(reinterpret_cast<char*>(buffer.data()), sizeof(uint32_t));
        uint32_t const width = internal::from_bytes(buffer);

        stream.read(reinterpret_cast<char*>(buffer.data()), sizeof(uint32_t));
        uint32_t const height = internal::from_bytes(buffer);

        stream.read(reinterpret_cast<char*>(buffer.data()), sizeof(uint32_t));
        uint32_t const pf = internal::from_bytes(buffer);

        auto context = std::pair<uint8_t*, size_t>(image_data[vm_index].data(), 0);

        stbi_write_bmp_to_func(
            [](void* context, void* data, int size) -> void {
                auto ctx = reinterpret_cast<std::pair<uint8_t*, size_t>*>(context);
                std::memcpy(ctx->first, data, size);
                ctx->first = ctx->first + size;
                ctx->second += size;
            },
            &context, width, height, 4, output.data() + 40 + stream.tellg());

        return std::span<uint8_t const>(image_data[vm_index].data(), context.second);
    }
} // namespace memucpp