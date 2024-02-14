// Copyright © 2022-2024 Dmitriy Lukovenko. All rights reserved.

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

namespace memucpp
{
    inline size_t constexpr MAX_INSTANCES = 50;

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

        // Set path to MEmuc executable
        auto set_path(std::filesystem::path const& dir_path) -> void
        {
            memuc_path = dir_path;
        }

        // Get list VM's of MEmu
        auto list_vms() -> std::vector<VMInfo>;

        // Start selected VM
        auto start_vm(uint16_t const vm_index) -> void;

        // Stop selected VM
        auto stop_vm(uint16_t const vm_index) -> void;

        // Reboot selected VM
        auto reboot_vm(uint16_t const vm_index) -> void;

        // Start installed application (Pass package name, not just name application)
        auto start_app(uint16_t const vm_index, std::string_view const package_name) -> void;

        // Stop installed application (Pass package name, not just name application)
        auto stop_app(uint16_t const vm_index, std::string_view const package_name) -> void;

        // Trigger device key in VM
        auto trigger_key(uint16_t const vm_index, KeyCode const key_code) -> void;

        // Trigger swipe content in VM
        auto trigger_swipe(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const start,
                           std::tuple<uint32_t, uint32_t> const end, uint32_t const speed) -> void;

        // Trigger zoom content in VM (is factor -1 - unzoom, 1 - zoom)
        auto trigger_zoom(uint16_t const vm_index, int32_t const factor) -> void;

        // Trigger click content at position in VM
        auto trigger_click(uint16_t const vm_index, std::tuple<uint32_t, uint32_t> const pos) -> void;

        // Set VM configuration (see https://www.memuplay.com/blog/memucommand-reference-manual.html)
        template <typename Type>
        auto set_config(uint16_t const vm_index, std::string_view const param, Type&& value)
        {
            set_config_internal(vm_index, param, std::to_string(value));
        }

        // Get list of running processes in VM
        auto list_process(uint16_t const vm_index) -> std::vector<ProcessInfo>;

        // Screen capture (Output binary data in BMP format)
        auto screen_cap(uint16_t const vm_index) -> std::span<uint8_t const>;

      private:
        std::filesystem::path memuc_path;
        std::array<std::vector<uint8_t>, MAX_INSTANCES> image_data;

        auto set_config_internal(uint16_t const vm_index, std::string_view const param, std::string const& value)
            -> void;
    };
} // namespace memucpp