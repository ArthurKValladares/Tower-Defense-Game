#pragma once

#define APP_NAME "TD Game"

constexpr int seconds_to_nanoseconds(int seconds) {
    return seconds * 1000000000;
}

int seconds_to_nanoseconds(float seconds) {
    return static_cast<int>(seconds * 1000000000);
}