#pragma once

#include <string>
#include <utility>

enum class Role {
    System,
    User,
    Assistant
};

class Message {
public:
    // Default: empty System message
    Message()
        : role_(Role::System),
          content_() {}

    // Construct a message with a role and text
    Message(Role role, std::string content)
        : role_(role),
          content_(std::move(content)) {}

    // Who sent this message
    Role role() const noexcept {
        return role_;
    }

    // Message text
    const std::string& content() const noexcept {
        return content_;
    }

private:
    Role role_;
    std::string content_;
};