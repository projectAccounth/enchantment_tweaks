#pragma once

#include <curl/curl.h>
#include <iostream>
#include <string>
#include <variant>
#include <filesystem>

namespace enchantment_tweaks::app {

namespace fs = std::filesystem;

/// simple singleton wrapper for libcurl
class NetworkManager {
public:
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    NetworkManager(NetworkManager&&) = delete;
    NetworkManager& operator=(NetworkManager&&) = delete;

    static NetworkManager& getInstance() {
        static NetworkManager instance;
        return instance;
    }
    
    using DownloadTarget = std::variant<std::string*, fs::path>;

    bool download(const std::string& url, DownloadTarget target, bool showProgress = false) const {
        CURL* curl = curl_easy_init();
        if (!curl) return false;

        FILE* filePtr = nullptr;
        bool success = false;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "");

        if (showProgress) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        } else {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        }

        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, std::string*>) {
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, arg);
            } 
            else if constexpr (std::is_same_v<T, fs::path>) {
                filePtr = std::fopen(arg.string().c_str(), "wb");
                if (filePtr) {
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, filePtr);
                }
            }
        }, target);

        if (std::holds_alternative<std::string*>(target) || filePtr != nullptr) {
            CURLcode res = curl_easy_perform(curl);
            if (showProgress) std::cout << std::endl;

            if (res == CURLE_OK) {
                success = true;
            } else {
                std::cerr << "[Error]: " << curl_easy_strerror(res) << std::endl;
            }
        }

        if (filePtr) {
            std::fclose(filePtr);
        }
        curl_easy_cleanup(curl);
        
        return success;
    }

private:
    NetworkManager() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~NetworkManager() {
        curl_global_cleanup();
    }

    static size_t stringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        auto* buffer = static_cast<std::string*>(userp);
        buffer->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    static size_t fileWriteCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
        return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
    }

    /// s1mple progress bar
    static int progressCallback(
        [[maybe_unused]] void* clientp, 
        curl_off_t dltotal, 
        curl_off_t dlnow, 
        [[maybe_unused]] curl_off_t ultotal, 
        [[maybe_unused]] curl_off_t ulnow
    ) {
        if (dltotal <= 0) return 0;
        
        double percentage = (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0;
        int barWidth = 30;
        int progressWidth = static_cast<int>((percentage / 100.0) * barWidth);

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < progressWidth) std::cout << "=";
            else if (i == progressWidth) std::cout << ">";
            else std::cout << " ";
        }
        std::printf("] %.1f%%", percentage);
        std::cout.flush();
        return 0;
    }
};
}