#include "winbintest.hpp"
#include "../vendor/gunzip/gunzip.hpp"
#include "../vendor/nlohmann/json.hpp"

#include <Windows.h>
#include <urlmon.h>

#include <exception>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "urlmon.lib")

winbintest_t::winbintest_t(const std::string_view binary_name) {
    std::vector<std::uint8_t> manifest_json_gzipped{};
    if (!download(std::format("https://winbindex.m417z.com/data/by_filename_compressed/{}.json.gz", binary_name), manifest_json_gzipped)) {
        throw std::runtime_error("Failed to download manifest JSON for binary: " + std::string(binary_name)); 
    }
    auto manifest_json_vec = gunzip(manifest_json_gzipped);
    manifest_json_gzipped.clear();
    
    nlohmann::json manifest_json = std::string(manifest_json_vec.begin(), manifest_json_vec.end());
    manifest_json_vec.clear();

    
    // parse the JSON and download all the files to /cache
}

bool winbintest_t::download(const std::string_view url, std::vector<uint8_t>& out) {
    namespace fs = std::filesystem;

    const fs::path cache_dir = fs::current_path() / "cache";
    const fs::path destination = cache_dir / fs::path(url).filename();

    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        return false;
    }

    const std::string url_string = std::string(url);
    const std::string destination_string = destination.string();

    const HRESULT result = URLDownloadToFileA(nullptr, url_string.c_str(), destination_string.c_str(), 0, nullptr);
    if (FAILED(result)) {
        return false;
    }

    std::ifstream file(destination, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    
    const auto size = file.tellg();
    if (size < 0) {
        return false;
    }

    out.resize(static_cast<std::size_t>(size));

    file.seekg(0, std::ios::beg);

    if (!out.empty()) {
        file.read(reinterpret_cast<char*>(out.data()), size);
        
        if (!file) {
            out.clear();
            return false;
        }
    }
    return true;
}

bool winbintest_t::download(const std::string_view url, const std::string_view path) {
    namespace fs = std::filesystem;

    const fs::path cache_dir = fs::current_path() / "cache";
    const fs::path destination = path; 

    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        return false;
    }

    const std::string url_string = std::string(url);
    const std::string destination_string = destination.string();

    const HRESULT result = URLDownloadToFileA(nullptr, url_string.c_str(), destination_string.c_str(), 0, nullptr);

    return !FAILED(result);
}

void winbintest_t::register_callback(const std::string& name, callback_fn fn) {
    callback_entry entry{};
    entry.name = name;
    entry.fn = fn;
    entry.cb_ctx = {};
    entry.enabled = true;
    
    entries.emplace_back(std::move(entry));
}

bool winbintest_t::run() {
    //TODO;
    return true;
}