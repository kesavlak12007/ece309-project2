#include "core/conversation.h"

#include <stdexcept>
#include <utility>


// ------------------------------------------------------------
// Basic object lifetime
// ------------------------------------------------------------

Conversation::Conversation()
    : data_(nullptr),
      size_(0),
      capacity_(0) {}


Conversation::~Conversation() {
    delete[] data_;
}


// ------------------------------------------------------------
// Basic container operations
// ------------------------------------------------------------

std::size_t Conversation::size() const noexcept {
    return size_;
}


const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at index out of range");
    }

    return data_[i];
}


const Message* Conversation::begin() const noexcept {
    return data_;
}


const Message* Conversation::end() const noexcept {
    if (data_ == nullptr) {
        return nullptr;
    }

    return data_ + size_;
}


// ------------------------------------------------------------
// Append / growable array
// ------------------------------------------------------------

void Conversation::append(Message m) {
    // If there is no room for another Message, grow the array.
    if (size_ == capacity_) {
        const std::size_t new_capacity =
            (capacity_ == 0) ? 1 : capacity_ * 2;

        Message* new_data = new Message[new_capacity];

        // Transfer the existing messages into the new array.
        for (std::size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[i]);
        }

        // The new array now owns the messages.
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

    // Store the new message in the next available slot.
    data_[size_] = std::move(m);
    ++size_;
}


// ------------------------------------------------------------
// Copy semantics
// ------------------------------------------------------------

Conversation::Conversation(const Conversation& other)
    : data_(nullptr),
      size_(0),
      capacity_(0) {

    if (other.capacity_ == 0) {
        return;
    }

    data_ = new Message[other.capacity_];

    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            data_[i] = other.data_[i];
        }
    }
    catch (...) {
        delete[] data_;
        data_ = nullptr;
        throw;
    }

    size_ = other.size_;
    capacity_ = other.capacity_;
}


Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) {
        return *this;
    }

    Message* new_data = nullptr;

    if (other.capacity_ != 0) {
        new_data = new Message[other.capacity_];

        try {
            for (std::size_t i = 0; i < other.size_; ++i) {
                new_data[i] = other.data_[i];
            }
        }
        catch (...) {
            delete[] new_data;
            throw;
        }
    }

    // Only discard our old data after the copy succeeds.
    delete[] data_;

    data_ = new_data;
    size_ = other.size_;
    capacity_ = other.capacity_;

    return *this;
}


// ------------------------------------------------------------
// Move semantics
// ------------------------------------------------------------

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_),
      size_(other.size_),
      capacity_(other.capacity_) {

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}


Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    // Release whatever this Conversation currently owns.
    delete[] data_;

    // Steal other's allocation.
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    // Leave other valid and empty.
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;

    return *this;
}