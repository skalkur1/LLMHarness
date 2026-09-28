// include/core/message.h
// Interface fixed by spec §3.1.

#pragma once
#include <string>

enum class Role { System, User, Assistant };

class Message {
public:
    // Default-constructs an empty System message with empty content.
    // Needed so Conversation can allocate raw array slots before
    // append() fills them in.
    Message();

    Message(Role role, std::string content);

    Role               role()    const noexcept;  // Who sent this message.
    const std::string& content() const noexcept;  // The message text.

private:
    Role        role_;
    std::string content_;
};

inline Message::Message() : role_(Role::System) {}
inline Message::Message(Role role, std::string content)
    : role_(role), content_(std::move(content)) {}
inline Role Message::role() const noexcept { return role_; }
inline const std::string& Message::content() const noexcept { return content_; }
