// Copyright © 2020-2024 Dmitriy Lukovenko. All rights reserved.

#pragma once

#include <array>
#include <charconv>
#include <filesystem>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <spanstream>
#include <string_view>
#include <vector>

namespace memucpp
{
    namespace internal
    {
        template <typename Type>
        auto stoi(std::string_view const source) -> Type
        {
            Type out;
            std::from_chars_result result = std::from_chars(source.data(), source.data() + source.size(), out);
            if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
            {
                throw std::invalid_argument("An error occurred during type conversion");
            }
            return out;
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
        bool enabled;
    };

    struct ProcessInfo
    {
        std::string name;
    };

    struct VMConfig
    {
        uint32_t width;
        uint32_t height;
        uint16_t dpi;

        static auto Default() -> VMConfig
        {
            return VMConfig{.width = 720, .height = 1280, .dpi = 240};
        }
    };

    /*!
        \brief MEmuc executable path (C:/Program Files is default)
    */
    inline std::filesystem::path memuc_path = "C:/Program Files/Microvirt/MEmu/memuc.exe";

    class Memuc
    {
      public:
        Memuc(uint16_t const vm_index, VMConfig const& config);

        ~Memuc();

        Memuc(Memuc const&) = delete;

        Memuc(Memuc&& other);

        auto operator=(Memuc const&) -> Memuc& = delete;

        auto operator=(Memuc&& other) -> Memuc&;

        /*!
            \brief Returns list of the VMs
        */
        auto list_vms() const -> std::vector<VMInfo>;

        /*!
            \brief Reboots the VM
        */
        auto reboot() -> void;

        /*!
            \brief Starts the new application
            \param package_name the application package
        */
        auto start_app(std::string_view const package_name) -> void;

        /*!
            \brief Stops the application
            \param package_name the application package
        */
        auto stop_app(std::string_view const package_name) -> void;

        /*!
            \brief Triggers the device key
            \param key_code the keycode
        */
        auto trigger_key(KeyCode const key_code) -> void;

        /*!
            \brief Triggers the device swipe
            \param start_position start of the swipe position (tuple x, y)
            \param start_position end of the swipe position (tuple x, y)
            \param speed the swipe speed
        */
        auto trigger_swipe(std::tuple<uint32_t, uint32_t> const start_position,
                           std::tuple<uint32_t, uint32_t> const end_position, uint32_t const speed) -> void;

        /*!
            \brief Triggers the device click
            \param position the screen position
        */
        auto trigger_click(std::tuple<uint32_t, uint32_t> const position) -> void;

        /*!
            \brief Returns list of the running VM's processes
        */
        auto list_process() const -> std::vector<ProcessInfo>;

        /*!
            \brief Returns the screen capture screenshot
            \return span of the bitmap image (BMP format)
        */
        auto screen_cap() -> std::span<uint8_t const>;

      private:
        uint16_t vm_index;
        std::vector<uint8_t> image_buffer;
    };
} // namespace memucpp