#pragma once
#include <cstdint>

constexpr int MAX_NAME_LEN = 24;
constexpr int MAX_CHAT_TEXT = 96;

struct ChatMessage {
    uint32_t player_id;
    char name[MAX_NAME_LEN];
    char text[MAX_CHAT_TEXT];
};
