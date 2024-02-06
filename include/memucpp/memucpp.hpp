#pragma once

#include <array>
#include <charconv>
#include <filesystem>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <spanstream>
#include <stb_image_write.h>
#include <string_view>
#include <vector>

#define NOMINMAX
#define _WINSOCKAPI_
#include <windows.h>
#undef NOMINMAX

namespace memucpp
{
    size_t const MAX_INSTANCES = 50;

    namespace internal
    {
        inline auto process_execute(std::string_view const command) -> std::vector<uint8_t>
        {
            std::array<uint8_t, 256> buffer;
            std::vector<uint8_t> result;

            FILE* pipe = ::_popen(std::string(command.data(), command.size()).c_str(), "rb");
            if (!pipe)
            {
                throw std::runtime_error("popen() failed!");
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

        inline auto to_utf_8(std::span<uint8_t const> const source) -> std::string
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

        template <typename Type>
        inline auto to(std::string_view const source) -> Type
        {
            Type dest;
            std::from_chars_result result = std::from_chars(source.data(), source.data() + source.size(), dest);
            if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
            {
                throw std::invalid_argument("Invalid argument!");
            }
            return dest;
        }

        inline auto from_bytes(std::span<uint8_t const> const source) -> uint32_t
        {
            uint32_t dest;
            std::memcpy(&dest, source.data(), sizeof(uint32_t));
            return dest;
        }
    } // namespace internal

    class error : public std::exception
    {
      public:
        error(std::string_view const what_arg) : message(what_arg.data(), what_arg.size())
        {
        }

        auto what() const -> char const* override
        {
            return message.c_str();
        }

      private:
        std::string message;
    };

    enum class KeyCode : uint32_t
    {
        Menu = 0,
        Home = 3,
        Back = 4,
        VolumeUp = 24,
        VolumeDown = 25
    };

    struct VMInfo
    {
        uint16_t index;
        std::string name;
        uint16_t status;
    };

    struct ProcessInfo
    {
        std::string name;
    };

    class Memuc
    {
      public:
        Memuc() = default;

        Memuc(Memuc const&) = delete;

        Memuc(Memuc&& other) : memuc_path(std::move(other.memuc_path)), image_data(std::move(other.image_data))
        {
        }

        auto operator=(Memuc const&) -> Memuc& = delete;

        auto operator=(Memuc&& other) -> Memuc&
        {
            memuc_path = std::move(other.memuc_path);
            image_data = std::move(other.image_data);
            return *this;
        }

        auto set_path(std::filesystem::path const& dir_path) -> void
        {
            memuc_path = dir_path;
        }

        auto list_vms() -> std::vector<VMInfo>
        {
            std::string command = std::format("\"{}/memuc.exe\" listvms", memuc_path.string());
            auto output = internal::to_utf_8(internal::process_execute(command));

            auto lines = output | std::views::split('\n') |
                         std::views::filter([&](auto const& element) { return element.size() > 1; });

            std::vector<VMInfo> out;

            for (auto const& line : lines)
            {
                auto args = line | std::views::split(',') | std::views::transform([](auto&& element) {
                                return std::string_view(element.data(), element.size());
                            }) |
                            std::ranges::to<std::vector>();

                auto vm_info = VMInfo{.index = internal::to<uint16_t>(args[0]),
                                      .name = std::string(args[1].data(), args[1].size()),
                                      .status = internal::to<uint16_t>(args[3])};

                out.push_back(std::move(vm_info));
            }
            return out;
        }

        auto start_vm(uint16_t const vm_index) -> void
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

        auto stop_vm(uint16_t const vm_index) -> void
        {
            std::string command = std::format("\"{}/memuc.exe\" stop -i {}", memuc_path.string(), vm_index);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") != std::string::npos)
            {
                image_data[vm_index].clear();
            }
        }

        auto reboot_vm(uint16_t const vm_index) -> void
        {
            std::string command = std::format("\"{}/memuc.exe\" reboot -i {}", memuc_path.string(), vm_index);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto start_app(uint16_t const vm_index, std::string_view const package_name) -> void
        {
            std::string command =
                std::format("\"{}/memuc.exe\" -i {} startapp {}", memuc_path.string(), vm_index, package_name);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto stop_app(uint16_t const vm_index, std::string_view const package_name) -> void
        {
            std::string command =
                std::format("\"{}/memuc.exe\" -i {} stopapp {}", memuc_path.string(), vm_index, package_name);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto trigger_key(uint16_t const vm_index, KeyCode const key_code) -> void
        {
            std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input keyevent {}", memuc_path.string(),
                                              vm_index, static_cast<uint32_t>(key_code));
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("connected") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto trigger_swipe(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const start,
                           std::tuple<uint32_t, uint32_t> const end, uint32_t const speed) -> void
        {
            std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input swipe {} {} {} {} {}",
                                              memuc_path.string(), vm_index, std::get<0>(start), std::get<1>(start),
                                              std::get<0>(end), std::get<1>(end), speed);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("connected") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto trigger_zoom(uint16_t const vm_index, int32_t const factor) -> void
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

        auto trigger_click(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const pos) -> void
        {
            std::string command = std::format("\"{}/memuc.exe\" -i {} adb shell input tap {} {}", memuc_path.string(),
                                              vm_index, std::get<0>(pos), std::get<1>(pos));
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("connected") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        template <typename Type>
        auto set_config(uint16_t const vm_index, std::string_view const param, Type&& value)
        {
            std::string command =
                std::format("\"{}/memuc.exe\" setconfigex -i {} {} {}", memuc_path.string(), vm_index, param, value);
            auto output = internal::to_utf_8(internal::process_execute(command));

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("MEmuc is not connected");
            }
        }

        auto list_process(uint16_t const vm_index) -> std::vector<ProcessInfo>
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

        auto screen_cap(uint16_t const vm_index) -> std::span<uint8_t const>
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

      private:
        std::filesystem::path memuc_path;
        std::array<std::vector<uint8_t>, MAX_INSTANCES> image_data;
    };
} // namespace memucpp