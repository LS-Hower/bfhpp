// 2025-10-13  ls_test_BF.cpp

#include "ls_BF.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

using uchar = unsigned char;

constexpr std::size_t bf_buffer_size { 30000 };
constexpr std::size_t nearby_radius { 8 };

static_assert(0 < bf_buffer_size);

[[nodiscard]] auto fmt_byte_hex2(uchar byte) -> std::string
{
    static constexpr std::size_t byte_hex_len { (std::numeric_limits<uchar>::digits + 3) / 4 };
    return std::format("{:0{}x}", static_cast<unsigned int>(byte), byte_hex_len);
}

[[nodiscard]] auto get_command() -> std::string
{
    std::println(std::cout, "");
    std::println(std::cout, "Input command:");
    std::string command;
    std::getline(std::cin, command);
    return command;
}

[[nodiscard]] auto my_tolower(char ch) -> char
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

[[nodiscard]] auto is_exit_command(std::string_view command) -> bool
{
    using namespace std::string_view_literals;

    static constexpr std::array exit_commands {
        "quit"sv, "q"sv, "exit"sv, "e"sv, "stop"sv, "s"sv, ""sv
    };

    const auto lowered_cmd {
        command
        | std::views::transform(my_tolower)
        | std::ranges::to<std::string>()
    };

    return std::ranges::find(exit_commands, lowered_cmd) != exit_commands.end();
}

auto repl() -> void
{
    std::vector<uchar> buffer((bf_buffer_size));
    ls_hower::bf::VMView vm { buffer };
    using vm_t = decltype(vm); // access static member function be type makes clangd happy
    const auto& behaviors { vm_t::default_behavior() };

    std::println(std::cout, "Enter commands ('quit' or 'exit' or blank line to exit):");

    for (;;) {
        const auto content { vm.format_nearby(nearby_radius, fmt_byte_hex2) };
        const auto content_len { content.size() };
        const auto horizontal_edge { std::string(content_len, '-') };

        std::println(std::cout, "");
        std::println(std::cout, "");
        std::println(std::cout, "Current state:");
        std::println(std::cout, "+-{}-+", horizontal_edge);
        std::println(std::cout, "| {} |", content);
        std::println(std::cout, "+-{}-+", horizontal_edge);

        std::string cmd { get_command() };

        if (is_exit_command(cmd)) {
            break;
        }

        std::println(std::cout, "");
        std::println(std::cout, "Output:");
        try {
            vm.exec(vm_t::compile(cmd, behaviors));

        } catch (const std::exception& e) {
            std::println(std::cerr, "Function {}: Caught exception: {}", __func__, e.what());
        }
    }
}

} // namespace

auto main() -> int
try {
    repl();

} catch (const std::exception& e) {
    std::println(std::cerr, "Function {}: Caught exception: {}", __func__, e.what());
}
