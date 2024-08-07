// Copyright © 2020-2024 Dmitriy Lukovenko. All rights reserved.

#include "memucpp.hpp"
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

namespace memucpp
{
    namespace internal
    {
        auto subprocess_execute(std::span<std::string const> const arguments) -> std::optional<std::vector<uint8_t>>
        {
            std::array<uint8_t, 256> buffer;
            std::vector<uint8_t> result;

            std::stringstream stream;

            bool first = true;
            for (auto const& argument : arguments)
            {
                if (!first)
                {
                    stream << " ";
                }
                stream << argument;
                first = false;
            }

            FILE* pipe = ::_popen(stream.str().c_str(), "rb");
            if (!pipe)
            {
                return std::nullopt;
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
                ::_pclose(pipe);
                return result;
            }
            catch (...)
            {
                ::_pclose(pipe);
                return std::nullopt;
            }
        }

        auto to_utf_8(std::span<uint8_t const> const source) -> std::string
        {
            size_t size = ::MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<char const*>(source.data()),
                                                static_cast<int32_t>(source.size()), nullptr, 0);

            std::wstring wsource(size, '\0');
            ::MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<char const*>(source.data()),
                                  static_cast<int32_t>(source.size()), wsource.data(),
                                  static_cast<int32_t>(wsource.size()));

            size = WideCharToMultiByte(CP_UTF8, 0, wsource.data(), static_cast<int32_t>(wsource.size()), nullptr, 0,
                                       nullptr, nullptr);

            std::string out(size, '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, wsource.data(), static_cast<int32_t>(wsource.size()), out.data(),
                                  static_cast<int32_t>(out.size()), nullptr, nullptr);
            return out;
        }
    } // namespace internal

    Memuc::Memuc(uint16_t const vm_index, VMConfig const& config) : vm_index(vm_index), image_buffer(8 * 1024 * 1024)
    {
        // Sets the VM Config
        {
            std::vector<std::pair<std::string, std::string>> const parameters{
                {"is_customed_resolution", "1"},
                {"resolution_width", std::to_string(config.width)},
                {"resolution_height", std::to_string(config.height)},
                {"vbox_dpi", std::to_string(config.dpi)}};

            for (auto const& parameter : parameters)
            {
                std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()),
                                                         "setconfigex",
                                                         "-i",
                                                         std::to_string(vm_index),
                                                         parameter.first,
                                                         parameter.second};
                auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

                if (output.find("SUCCESS") == std::string::npos)
                {
                    throw error("MEmuc is not connected");
                }
            }
        }

        // Starts the VM
        {
            std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "start", "-i",
                                                     std::to_string(vm_index)};
            auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

            if (output.find("SUCCESS") == std::string::npos)
            {
                throw error("An error occurred when starting the VM");
            }
        }
    }

    Memuc::~Memuc()
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "stop", "-i",
                                                 std::to_string(vm_index)};
        // internal::subprocess_execute(arguments);
    }

    Memuc::Memuc(Memuc&& other) : vm_index(other.vm_index), image_buffer(std::move(other.image_buffer))
    {
    }

    auto Memuc::operator=(Memuc&& other) -> Memuc&
    {
        vm_index = other.vm_index;
        image_buffer = std::move(other.image_buffer);
        return *this;
    }

    auto Memuc::list_vms() const -> std::vector<VMInfo>
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "listvms"};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        auto lines = output | std::views::split('\n') |
                     std::views::filter([&](auto const& element) { return element.size() > 1; });

        std::vector<VMInfo> out;

        for (auto const& line : lines)
        {
            auto args =
                line | std::views::split(',') |
                std::views::transform([](auto&& element) { return std::string_view(element.data(), element.size()); }) |
                std::ranges::to<std::vector>();

            VMInfo vm_info{.index = internal::stoi<uint16_t>(args[0]),
                           .name = std::string(args[1].data(), args[1].size()),
                           .enabled = static_cast<bool>(internal::stoi<uint16_t>(args[3]))};
            out.push_back(std::move(vm_info));
        }
        return out;
    }

    auto Memuc::reboot() -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "reboot", "-i",
                                                 std::to_string(vm_index)};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::start_app(std::string_view const package_name) -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "-i",
                                                 std::to_string(vm_index), "startapp", std::string(package_name)};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::stop_app(std::string_view const package_name) -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()), "-i",
                                                 std::to_string(vm_index), "stopapp", std::string(package_name)};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("SUCCESS") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_key(KeyCode const key_code) -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()),
                                                 "-i",
                                                 std::to_string(vm_index),
                                                 "adb",
                                                 "shell",
                                                 "input",
                                                 "keyevent",
                                                 std::to_string(static_cast<uint32_t>(key_code))};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_swipe(std::tuple<uint32_t, uint32_t> const start_position,
                              std::tuple<uint32_t, uint32_t> const end_position, uint32_t const speed) -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()),
                                                 "-i",
                                                 std::to_string(vm_index),
                                                 "adb",
                                                 "shell",
                                                 "input",
                                                 "swipe",
                                                 std::to_string(std::get<0>(start_position)),
                                                 std::to_string(std::get<1>(start_position)),
                                                 std::to_string(std::get<0>(end_position)),
                                                 std::to_string(std::get<1>(end_position)),
                                                 std::to_string(speed)};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::trigger_click(std::tuple<uint32_t, uint32_t> const position) -> void
    {
        std::vector<std::string> const arguments{std::format("\"{}\"", memuc_path.string()),
                                                 "-i",
                                                 std::to_string(vm_index),
                                                 "adb",
                                                 "shell",
                                                 "input",
                                                 "tap",
                                                 std::to_string(std::get<0>(position)),
                                                 std::to_string(std::get<1>(position))};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        if (output.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }
    }

    auto Memuc::list_process() const -> std::vector<ProcessInfo>
    {
        std::vector<std::string> const arguments{
            std::format("\"{}\"", memuc_path.string()), "-i", std::to_string(vm_index), "adb", "shell", "ps"};
        auto output = internal::to_utf_8(internal::subprocess_execute(arguments).value());

        std::string_view const message(output.data(), output.data() + 40);

        if (message.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }

        std::string_view const data(output.data() + 40, output.data() + output.size());

        auto lines =
            data | std::views::split('\n') |
            std::views::transform([](auto&& element) { return std::string_view(element.data(), element.size()); }) |
            std::views::filter([](auto&& element) { return element.find("com.") != std::string::npos; }) |
            std::ranges::to<std::vector>() | std::views::drop(1) | std::views::reverse | std::views::drop(1) |
            std::views::reverse;

        std::vector<ProcessInfo> out;

        for (auto const& line : lines)
        {
            ProcessInfo process_info{.name = std::string(line.begin() + line.find("com."), line.begin() + line.size())};
            out.push_back(process_info);
        }
        return out;
    }

    auto Memuc::screen_cap() -> std::span<uint8_t const>
    {
        std::vector<std::string> const arguments{
            std::format("\"{}\"", memuc_path.string()), "-i", std::to_string(vm_index), "adb", "exec-out", "screencap"};
        auto output = internal::subprocess_execute(arguments).value();

        auto message = internal::to_utf_8(std::span<uint8_t const>(output.data(), output.data() + 40));

        if (message.find("connected") == std::string::npos)
        {
            throw error("MEmuc is not connected");
        }

        uint32_t width;
        uint32_t height;
        uint64_t offset;
        {
            std::basic_ispanstream<uint8_t> stream(
                std::span<uint8_t>(output.data() + 40, output.data() + output.size()), std::ios::binary);

            stream.read(reinterpret_cast<uint8_t*>(&width), sizeof(uint32_t));
            stream.read(reinterpret_cast<uint8_t*>(&height), sizeof(uint32_t));
            stream.seekg(sizeof(uint32_t), std::ios::cur);

            offset = static_cast<uint64_t>(stream.tellg()) + 40;
        }

        size_t bytes;
        {
            std::basic_ospanstream<uint8_t> stream(std::span<uint8_t>(image_buffer.data(), image_buffer.size()),
                                                   std::ios::binary);

            uint32_t const alignment = 4;
            uint32_t const mask = alignment - 1;

            uint32_t const row_size = width * 3;
            uint32_t const align_width = (row_size + mask) & ~mask;

            uint32_t const bitmap_size = align_width * height;

            BITMAPFILEHEADER file_header{.bfType = 0x4D42,
                                         .bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
            BITMAPINFOHEADER info_header{.biSize = sizeof(BITMAPINFOHEADER),
                                         .biWidth = static_cast<LONG>(width),
                                         .biHeight = static_cast<LONG>(height),
                                         .biPlanes = 1,
                                         .biBitCount = 24};
            file_header.bfSize = bitmap_size + file_header.bfOffBits;

            stream.write(reinterpret_cast<uint8_t const*>(&file_header), sizeof(BITMAPFILEHEADER));
            stream.write(reinterpret_cast<uint8_t const*>(&info_header), sizeof(BITMAPINFOHEADER));

            // Move at end bitmap buffer for vertical mirror
            offset += width * height * 4;

            for (uint32_t const j : std::views::iota(0u, height))
            {
                for (uint32_t const i : std::views::iota(0u, width))
                {
                    stream.write(output.data() + offset + i * 4 + 2, sizeof(uint8_t)); // R
                    stream.write(output.data() + offset + i * 4 + 1, sizeof(uint8_t)); // G
                    stream.write(output.data() + offset + i * 4 + 0, sizeof(uint8_t)); // B
                }

                // Add padding if the image has 24 bit per pixel
                stream.seekp(align_width - width * 3, std::ios::cur);

                offset -= width * 4;
            }

            bytes = stream.tellp();
        }
        return std::span<uint8_t const>(image_buffer.data(), bytes);
    }
} // namespace memucpp