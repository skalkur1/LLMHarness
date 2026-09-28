// include/core/conversation.h
// Interface fixed by spec §3.2. No std::vector allowed.

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Empty conversation: size() == 0, no allocation yet.
    Conversation();

    // Releases all owned Message storage. No effect if already empty
    // (e.g. moved-from).
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    // this->begin() must differ from other.begin() afterward.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer — no per-element copying. Afterward, other
    // must be left valid and empty (safe to destroy or reassign).
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed. Amortized O(1):
    // capacity doubles (0 -> 1 -> 2 -> 4 -> ...) whenever it runs out.
    // Throws std::invalid_argument if m is a System message and the
    // conversation is not empty — the system message is pinned first.
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept;

    // Allocated slots (>= size()). Exposed so tests can check growth.
    std::size_t capacity() const noexcept;

    // Bounds-checked access. Throws std::out_of_range if i >= size().
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first. begin() == end() when
    // size() == 0.
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    void grow();  // doubles capacity_, moving existing messages over

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
