#include "core/sentinel_scanner.h"

#include <utility>


SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)),
      pending_() {}


SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    std::string combined = pending_;
    combined.append(chunk);

    const std::size_t pos = combined.find(sentinel_);

    // Sentinel found
    if (pos != std::string::npos) {
        std::string safe_text = combined.substr(0, pos);

        pending_.clear();

        return {
            std::move(safe_text),
            true
        };
    }

    // Sentinel not found
    const std::size_t hold = sentinel_.size() - 1;

    if (combined.size() > hold) {
        const std::size_t safe_count = combined.size() - hold;

        std::string safe_text = combined.substr(0, safe_count);
        pending_ = combined.substr(safe_count);

        return {
            std::move(safe_text),
            false
        };
    }

    pending_ = combined;

    return {
        "",
        false
    };
}


SentinelScanner::Out SentinelScanner::flush() {
    std::string safe_text = std::move(pending_);
    pending_.clear();

    return {
        std::move(safe_text),
        false
    };
}