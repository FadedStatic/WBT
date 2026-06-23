#pragma once

#include <any>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct callback_ctx {
    std::uint8_t* const buffer;
    std::uintptr_t size{0};
    std::any& cb_ctx;
};

using callback_fn = std::function<void(callback_ctx&)>;

struct callback_entry {
    std::string name; // might be useful later for debugging or something
    callback_fn fn;
    std::any cb_ctx;
    bool enabled{true};
};

struct winbintest_t {
    winbintest_t(const std::string_view binary_name);
    bool download(const std::string_view url, std::vector<uint8_t>& out);
    bool download(const std::string_view url, const std::string_view path);

    template <typename user_ctx>
    inline void register_callback(const std::string& name, callback_fn fn, user_ctx&& ctx) {
        entries.emplace_back(std::move(name), std::move(fn), std::any(std::forward<user_ctx>(ctx)), true);
    }

    void register_callback(const std::string& name, callback_fn fn);

    bool run();
private:
    std::vector<callback_entry> entries{};
};
