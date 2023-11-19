#pragma once

#include "../generator.hpp"
#include "sequencer.hpp"

#include <algorithm>
#include <any>
#include <chrono>
#include <concepts>
#include <functional>
#include <iostream>
#include <regex>


namespace asyncpp::interleaving {


struct deadlock_error : std::runtime_error {
    deadlock_error() : std::runtime_error("deadlock") {}
};


using interleaving = std::vector<std::pair<std::string, sequencer::state>>;


struct interleaving_printer {
    const interleaving& il;
    bool detail = false;
};


std::ostream& operator<<(std::ostream& os, const interleaving_printer& il);


class filter {
public:
    filter() : filter(".*") {}
    explicit filter(std::string_view file_regex) : m_files(file_regex.begin(), file_regex.end()) {}

    bool operator()(const sequence_point& point) const;

private:
    std::regex m_files;
};


generator<interleaving> run_all(std::function<std::any()> fixture,
                                std::vector<std::function<void(std::any&)>> threads,
                                std::vector<std::string_view> names = {},
                                filter filter_ = {});


template <class Fixture, class Input>
    requires std::convertible_to<Fixture&, Input>
generator<interleaving> run_all(std::function<Fixture()> fixture,
                                std::vector<std::function<void(Input)>> threads,
                                std::vector<std::string_view> names = {},
                                filter filter_ = {}) {
    std::function<std::any()> wrapped_init = [fixture = std::move(fixture)]() -> std::any {
        if constexpr (!std::is_void_v<Fixture>) {
            return std::any(fixture());
        }
        return {};
    };

    std::vector<std::function<void(std::any&)>> wrapped_threads;
    std::ranges::transform(threads, std::back_inserter(wrapped_threads), [](auto& thread) {
        return std::function<void(std::any&)>([thread = std::move(thread)](std::any& fixture) {
            return thread(std::any_cast<Fixture&>(fixture));
        });
    });

    return run_all(std::move(wrapped_init), std::move(wrapped_threads), std::move(names), filter_);
}


inline generator<interleaving> run_all(std::vector<std::function<void()>> threads,
                                       std::vector<std::string_view> names = {},
                                       filter filter_ = {}) {
    std::vector<std::function<void(std::any&)>> wrapped_threads;
    std::ranges::transform(threads, std::back_inserter(wrapped_threads), [](auto& thread) {
        return std::function<void(std::any&)>([thread = std::move(thread)](std::any&) {
            return thread();
        });
    });
    return run_all([] { return std::any(); }, std::move(wrapped_threads), std::move(names), filter_);
}

} // namespace asyncpp::interleaving
