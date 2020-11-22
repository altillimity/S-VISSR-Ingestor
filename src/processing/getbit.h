#pragma once

template <typename T>
inline bool getBit(T data, int bit)
{
    return (data >> bit) & 1;
}