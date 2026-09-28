#pragma once

#include <cstddef>

#include "core/message.h"

class Conversation {
public:
    // Empty conversation
    Conversation();

    // Release owned Message storage
    ~Conversation();

    // Deep-copy semantics
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Move semantics: steal other's buffer
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Add a message to the end of the conversation
    void append(Message m);

    // Number of messages currently stored
    std::size_t size() const noexcept;

    // Bounds-checked access
    const Message& at(std::size_t i) const;

    // Iteration support
    const Message* begin() const noexcept;
    const Message* end() const noexcept;

private:
    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};