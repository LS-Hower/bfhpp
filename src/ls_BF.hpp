// 2025-10-13  ls_BF.hpp

#pragma once

#ifndef LS_BF_HPP_INCLUDED
#define LS_BF_HPP_INCLUDED

#include <algorithm>
#include <cassert>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <ranges>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// ---------------- declarations ----------------

namespace ls_hower::bf {

template <std::bidirectional_iterator I>
class BehaviorTable;

namespace detail {

    [[nodiscard]] inline auto my_isgraph(char ch) noexcept -> bool
    {
        return static_cast<bool>(std::isgraph(static_cast<unsigned char>(ch)));
    }

    [[nodiscard]] inline auto size_t_enumerate_like(std::ranges::bidirectional_range auto&& rng)
    {
        return std::views::zip(std::views::iota(0ZU), rng);
    }

    template <std::bidirectional_iterator I>
    using MoveCount = std::iter_difference_t<I>;

    // MoveCount represents multiple pointer moves. e.g. >>> is 3, and <<<<>< is -4.
    // Char represents an ordinary command.
    template <std::bidirectional_iterator I>
    class SingleCommand {
        using MoveCount = MoveCount<I>;
        using underlying_variant = std::variant<char, MoveCount>;

    public:
        explicit SingleCommand(underlying_variant cmd)
            : cmd_ { std::move(cmd) }
        {
        }

        [[nodiscard]] static auto make_ordinary_cmd(char c) noexcept -> SingleCommand
        {
            return SingleCommand { underlying_variant { std::in_place_index<0>, c } };
        }

        [[nodiscard]] static auto make_move_cmd(MoveCount count) noexcept -> SingleCommand
        {
            return SingleCommand { underlying_variant { std::in_place_index<1>, count } };
        }

        [[nodiscard]] auto is_ordinary_cmd() const noexcept -> bool
        {
            return cmd_.index() == 0;
        }

        [[nodiscard]] auto is_move_cmd() const noexcept -> bool
        {
            return cmd_.index() == 1;
        }

        [[nodiscard]] auto get_ordinary_cmd() const -> char
        {
            return std::get<0>(cmd_);
        }

        [[nodiscard]] auto get_move_cmd() const -> MoveCount
        {
            return std::get<1>(cmd_);
        }

    private:
        underlying_variant cmd_;
    };

    template <std::bidirectional_iterator I>
    using ProgramCounter = std::vector<SingleCommand<I>>::size_type;

    // Jump table built from a BF program.
    // Able to jump to the matching bracket,
    // either from open to close, or from close to open.
    template <std::bidirectional_iterator I>
    class BracketJumpTable {
        using MoveCount = MoveCount<I>;
        using ProgramCounter = ProgramCounter<I>;

    public:
        explicit BracketJumpTable(std::span<const SingleCommand<I>> program)
        {
            std::stack<ProgramCounter> open_inds {};

            for (const auto [ind, cmd] : size_t_enumerate_like(program)) {

                if (cmd.is_move_cmd()) {
                    continue;
                }

                switch (cmd.get_ordinary_cmd()) {
                case '[':
                    [[unlikely]] open_inds.push(ind);
                    break;

                case ']':
                    [[unlikely]]
                    if (open_inds.empty()) [[unlikely]] {
                        throw std::invalid_argument { "Unmatched close bracket." };
                    }
                    {
                        const ProgramCounter open_ind { open_inds.top() };
                        assert(open_ind < ind);
                        table_.emplace(open_ind, ind);
                        table_.emplace(ind, open_ind);
                    }
                    open_inds.pop();
                    break;

                default:
                    [[likely]] break;
                }
            }

            if (!open_inds.empty()) [[unlikely]] {
                throw std::invalid_argument { "Unmatched open bracket." };
            }
        }

        [[nodiscard]] auto open_to_close(ProgramCounter pc) const -> ProgramCounter
        {
            const auto res { table_.at(pc) };
            assert(res > pc);
            return res;
        }

        [[nodiscard]] auto close_to_open(ProgramCounter pc) const -> ProgramCounter
        {
            const auto res { table_.at(pc) };
            assert(res < pc);
            return res;
        }

    private:
        std::unordered_map<ProgramCounter, ProgramCounter> table_;
    };

    template <std::bidirectional_iterator I>
    auto make_repr(std::string_view source_code, const BehaviorTable<I>& table) -> std::vector<SingleCommand<I>>
    {
        using MoveCount = detail::MoveCount<I>;
        return source_code
            | std::views::filter([&table](char c) -> bool {
                  return table.contains_or_reserved(c);
              })
            | std::views::chunk_by([](char a, char b) -> bool {
                  return (a == '<' || a == '>') && (b == '<' || b == '>');
              })
            | std::views::transform([](auto&& chunk) -> SingleCommand<I> {
                  if (const char first { *chunk.begin() }; !(first == '<' || first == '>')) {
                      // ordinary command
                      return SingleCommand<I>::make_ordinary_cmd(first);
                  }
                  return SingleCommand<I>::make_move_cmd(
                      std::ranges::fold_left(
                          chunk
                              | std::views::transform([](char ch) -> MoveCount {
                                    // don't wanna use the feature that in ascii, '<' is 60 and '>' is 62
                                    if (ch == '<') {
                                        return -1;
                                    }
                                    if (ch == '>') {
                                        return 1;
                                    }
                                    [[unlikely]] assert(false);
                                }),
                          MoveCount {},
                          std::plus<MoveCount> {}));
              })
            | std::ranges::to<std::vector>();
    }
} // namespace detail

template <std::bidirectional_iterator I>
class CompiledProgram {
public:
    CompiledProgram(std::string_view source_code, BehaviorTable<I> behavior)
        : owning_source_code_ { source_code }
        , behavior_ { std::move(behavior) }
        , repr_ { detail::make_repr(source_code, behavior_) }
        , jump_table_ { repr_ }
    {
    }

    auto update_source_code(std::string_view source_code) -> void
    {
        owning_source_code_ = source_code;
        repr_ = detail::make_repr(source_code, behavior_);
        jump_table_ = detail::BracketJumpTable<I> { repr_ };
    }

private:
    std::string owning_source_code_;
    BehaviorTable<I> behavior_;
    std::vector<detail::SingleCommand<I>> repr_;
    detail::BracketJumpTable<I> jump_table_;

    template <std::bidirectional_iterator J>
    friend class VMView;
};

template <std::bidirectional_iterator I>
class BehaviorTable {
public:
    using key_type = char;
    using mapped_type = std::function<auto(I)->void>;

private:
    static_assert(std::numeric_limits<unsigned char>::digits == 8);
    static constexpr auto vec_length { static_cast<std::size_t>(std::numeric_limits<key_type>::max()) + 1 };

    // | contained <|> modifiable-empty |
    // |          modifiable            | reserved | other |
    // |                 std::isgraph              |
    enum class KeyCategory : std::uint8_t {
        modifiable = 0,
        reserved,
        other
    };

    [[nodiscard]] static auto get_key_category(key_type key) noexcept -> KeyCategory
    {
        if (!(detail::my_isgraph(key))) {
            return KeyCategory::other;
        }
        return key == '<' || key == '>' || key == '[' || key == ']'
            ? KeyCategory::reserved
            : KeyCategory::modifiable;
    }

    static auto guard_modifiable_key_and_throw(key_type key) -> void
    {
        switch (get_key_category(key)) {

        case KeyCategory::modifiable:
            [[likely]] break;

        case KeyCategory::reserved:
            [[unlikely]] throw std::invalid_argument("Key is reserved.");

        case KeyCategory::other:
            [[unlikely]] throw std::invalid_argument("Key doesn't satisfy `isgraph`.");

        default:
            [[unlikely]] assert(false);
        }
    }

public:
    BehaviorTable(std::initializer_list<std::pair<key_type, mapped_type>> il)
        : table_(vec_length)
    {
        for (const auto& [key, func] : il) {
            guard_modifiable_key_and_throw(key);
            // std::initializer_list only provides const access to elements, so move is not possible.
            table_[key] = func;
        }
    }

    [[nodiscard]] auto operator[](key_type key) -> mapped_type&
    {
        guard_modifiable_key_and_throw(key);
        return table_[key];
    }

    [[nodiscard]] auto operator[](key_type key) const -> const mapped_type&
    {
        guard_modifiable_key_and_throw(key);
        return table_[key];
    }

    auto erase(key_type key) -> void
    {
        guard_modifiable_key_and_throw(key);
        table_[key] = mapped_type {};
    }

    [[nodiscard]] auto contains(key_type key) const -> bool
    {
        guard_modifiable_key_and_throw(key);
        return static_cast<bool>(table_[key]);
    }

    [[nodiscard]] auto contains_or_reserved(key_type key) const -> bool
    {
        switch (get_key_category(key)) {
        case KeyCategory::reserved:
            return true;

        case KeyCategory::modifiable:
            return static_cast<bool>(table_[key]);

        case KeyCategory::other:
            return false;

        default:
            [[unlikely]] assert(false);
        }
    }

    [[nodiscard]] auto keys() const -> std::vector<char>
    {
        return detail::size_t_enumerate_like(table_)
            | std::views::filter([](char, const auto& v) -> bool { return v; })
            | std::views::keys
            | std::ranges::to<std::vector>();
    }

    [[nodiscard]] static auto preset() noexcept -> const BehaviorTable&
    {
        return preset_table;
    }

    template <typename Fn>
    auto for_each(Fn fn) -> void
        requires std::invocable<Fn, key_type, mapped_type&>
    {
        for (auto& [k, v] : detail::size_t_enumerate_like(table_)
                | std::views::filter([](char, auto& v) -> bool { return v; })) {
            std::invoke(fn, k, v);
        }
    }

    template <typename Fn>
    auto for_each(Fn fn) const -> void
        requires std::invocable<Fn, key_type, const mapped_type&>
    {
        for (const auto& [k, v] : detail::size_t_enumerate_like(table_)
                | std::views::filter([](char, const auto& v) -> bool { return v; })) {
            std::invoke(fn, k, v);
        }
    }

    static auto read_cin(I current) -> void
    {
        std::iter_value_t<I> val {};
        std::cin >> val;
        *current = val;
    }

    static auto write_cout(I current) -> void
    {
        std::cout << *current;
    }

    static auto incr_cell(I current) -> void
    {
        ++*current;
    }

    static auto decr_cell(I current) -> void
    {
        --*current;
    }

private:
    static const BehaviorTable preset_table;

    std::vector<mapped_type> table_;
};

template <std::bidirectional_iterator I>
inline const BehaviorTable<I> BehaviorTable<I>::preset_table {
    { '+', BehaviorTable<I>::incr_cell },
    { '-', BehaviorTable<I>::decr_cell },
    { ',', BehaviorTable<I>::read_cin },
    { '.', BehaviorTable<I>::write_cout },
};

template <std::bidirectional_iterator I>
class VMView : public std::ranges::view_interface<VMView<I>> {
    using MoveCount = detail::MoveCount<I>;
    using ProgramCounter = detail::ProgramCounter<I>;

public:
    using iterator = I;
    using sentinel = I;
    using difference_type = std::iter_difference_t<I>;
    using value_type = std::iter_value_t<I>;
    using reference = std::iter_reference_t<I>;
    using behavior_table = BehaviorTable<I>;
    using compiled_program = CompiledProgram<I>;

    explicit VMView(I begin, I end, I current)
        : begin_ { begin }
        , end_ { end }
        , current_ { current }
    {
    }

    explicit VMView(I begin, I end)
        : VMView { begin, end, begin }
    {
    }

    explicit VMView(std::ranges::bidirectional_range auto&& bound, I current)
        : VMView { std::ranges::begin(bound), std::ranges::end(bound), current }
    {
    }

    explicit VMView(std::ranges::bidirectional_range auto&& bound)
        : VMView { std::ranges::begin(bound), std::ranges::end(bound) }
    {
    }

    [[nodiscard]] auto begin() const noexcept -> I
    {
        return begin_;
    }

    [[nodiscard]] auto end() const noexcept -> I
    {
        return end_;
    }

    [[nodiscard]] auto current() const noexcept -> I
    {
        return current_;
    }

    template <typename Fn>
    [[nodiscard]] auto format_nearby(std::size_t radius, Fn formatter = default_formatter) const -> std::string
        requires std::same_as<std::invoke_result_t<Fn, const value_type&>, std::string>
    {
        std::string result {};
        auto inserter { std::back_inserter(result) };
        VMView walker { *this };

        const behavior_table walker_behaviors {
            { '|', [begin { begin_ }, &inserter](I curr) -> void {
                 *inserter++ = (curr == begin ? '|' : ' ');
             } },
            { '{', [current { current_ }, &inserter](I curr) -> void {
                 *inserter++ = (curr == current ? '[' : ' ');
             } },
            { '}', [current { current_ }, &inserter](I curr) -> void {
                 *inserter++ = (curr == current ? ']' : ' ');
             } },
            { 'F', [&formatter, &inserter](I curr) -> void {
                 std::ranges::copy(formatter(*curr), inserter);
             } }
        };

        auto p { compile("<", walker_behaviors) };
        walker.exec_multiple(p, radius);
        p.update_source_code("|{F}>");
        walker.exec_multiple(p, (2 * radius) + 1);
        p.update_source_code("|");
        walker.exec_multiple(p, 1);

        return result;
    }

    auto exec_multiple(const CompiledProgram<I>& precompiled, int times) -> void
    {
        if (times < 0) [[unlikely]] {
            throw std::invalid_argument { "Times must be non-negative." };
        }
        for (const auto i [[maybe_unused]] : std::views::iota(0, times)) {
            exec(precompiled);
        }
    }

    auto exec(const CompiledProgram<I>& precompiled) -> void
    {
        const ProgramCounter size { precompiled.repr_.size() };

        // Loop variable `pc` might be modified inside the loop body.
        // So do not use views::iota.
        for (ProgramCounter pc {}; pc != size; ++pc) {
            const auto command { precompiled.repr_.at(pc) };

            if (command.is_move_cmd()) {
                // '<' and '>' are handled here.
                this->it_advance(command.get_move_cmd());
                continue;
            }

            switch (const auto cmd { command.get_ordinary_cmd() }) {

            case '[':
                if (*current_ == 0) [[unlikely]] {
                    pc = precompiled.jump_table_.open_to_close(pc);
                }
                break;

            case ']':
                if (*current_ != 0) [[likely]] {
                    pc = precompiled.jump_table_.close_to_open(pc);
                }
                break;

            default:
                // Modifiable behaviors (',' '.' '+' '-' defaultly contained) are handled here.
                if (const auto func { precompiled.behavior_[cmd] }) [[likely]] {
                    func(current_);

                } else {
                    // useless characters and '<' '>' are already filtered out during precompilation.
                    assert(false);
                }
                break;
            }
        }
    }

    [[nodiscard]] static auto default_behavior() noexcept -> const behavior_table&
    {
        return behavior_table::preset();
    }

    [[nodiscard]] static auto compile(std::string_view source_code, const behavior_table& behavior = behavior_table::preset()) -> CompiledProgram<I>
    {
        return CompiledProgram<I> { source_code, behavior };
    }

private:
    auto it_incr() -> void
    {
        ++this->current_;

        if (this->current_ == end_) [[unlikely]] {
            this->current_ = begin_;
        }
    }

    auto it_decr() -> void
    {
        if (this->current_ == begin_) [[unlikely]] {
            this->current_ = end_;
        }
        --this->current_;
    }

    auto it_advance(difference_type n) -> void
    {
        if constexpr (std::random_access_iterator<I>) {
            // evaluate the result in constant time.
            const auto size { std::distance(begin_, end_) };
            const auto current_ind { std::distance(begin_, current_) };

            static_assert(std::is_same_v<decltype(size), const difference_type>);
            static_assert(std::is_same_v<decltype(current_ind), const difference_type>);

            auto new_ind { (current_ind + n % size) % size };

            if (new_ind < 0) {
                new_ind += size;
            }

            current_ = begin_ + new_ind;

        } else {
            if (n >= 0) {

                for (const auto i [[maybe_unused]] : std::views::iota(difference_type {}, n)) {
                    it_incr();
                }

            } else {
                for (const auto i [[maybe_unused]] : std::views::iota(difference_type {}, -n)) {
                    it_decr();
                }
            }
        }
    }

    static auto default_formatter(const value_type& cell) -> std::string
    {
        return std::format("{}", cell);
    }

    static const BehaviorTable<I> preset_behavior;
    I begin_;
    I end_;
    I current_;
};

template <std::bidirectional_iterator I>
inline const BehaviorTable<I> VMView<I>::preset_behavior { VMView<I>::behavior_table::preset() };

// Deduction guide for the constructor accepting only a range.
template <std::ranges::bidirectional_range R>
VMView(R) -> VMView<std::ranges::iterator_t<R>>;

} // namespace ls_hower::bf

#endif // LS_BF_HPP_INCLUDED
