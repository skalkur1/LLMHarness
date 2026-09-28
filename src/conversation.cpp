// src/conversation.cpp

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() {
    delete[] data_;  // delete[] on nullptr is a no-op (empty / moved-from)
}

Conversation::Conversation(const Conversation& other) {
    if (other.size_ == 0) return;  // nothing to copy, stay unallocated

    Message* fresh = new Message[other.size_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];  // std::string copy may throw
        }
    } catch (...) {
        delete[] fresh;
        throw;
    }
    data_ = fresh;
    size_ = other.size_;
    capacity_ = other.size_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;

    // Copy first, then swap: if the copy throws, *this is untouched.
    Conversation tmp(other);
    std::swap(data_, tmp.data_);
    std::swap(size_, tmp.size_);
    std::swap(capacity_, tmp.capacity_);
    return *this;  // tmp's destructor frees our old buffer
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;

    delete[] data_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::grow() {
    std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;

    // Only the allocation can throw; moving a Message (enum + std::string)
    // is noexcept, so once we have the new buffer nothing else can fail.
    Message* fresh = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        fresh[i] = std::move(data_[i]);
    }
    delete[] data_;
    data_ = fresh;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (m.role() == Role::System && size_ != 0) {
        throw std::invalid_argument(
            "Conversation::append: System message must be first");
    }
    if (size_ == capacity_) grow();
    data_[size_] = std::move(m);
    ++size_;
}

std::size_t Conversation::size() const noexcept { return size_; }

std::size_t Conversation::capacity() const noexcept { return capacity_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) throw std::out_of_range("Conversation::at: index out of range");
    return data_[i];
}

const Message* Conversation::begin() const noexcept { return data_; }
const Message* Conversation::end() const noexcept { return data_ + size_; }
