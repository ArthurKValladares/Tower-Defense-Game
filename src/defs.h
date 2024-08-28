#pragma once

#define APP_NAME "TD Game"

#include <print>

static constexpr int seconds_to_nanoseconds(int seconds) {
    return seconds * 1000000000;
}

static int seconds_to_nanoseconds(float seconds) {
    return static_cast<int>(seconds * 1000000000);
}

#ifndef NDEBUG
#   define M_Assert(Expr, Msg) \
    __M_Assert(#Expr, Expr, __FILE__, __LINE__, Msg)
#else
#   define M_Assert(Expr, Msg) ;
#endif

static void __M_Assert(const char* expr_str, bool expr, const char* file, int line, const char* msg)
{
    if (!expr)
    {
        std::print("Assert failed:\t{}\nExpected:\t{}\nSource:\t{}, line:\t{}\n", msg, expr_str, file, line);
        abort();
    }
}