#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ai { namespace questsync
{
    // Versioned, bounded data messages, independent of human-readable bot chat.
    constexpr char Prefix[] = "PFQBOT";
    constexpr size_t MaxRequest = 240;
    constexpr size_t ChunkSize = 128;
    constexpr size_t MaxChunks = 192;
    constexpr size_t MaxRows = 50; // 25 actual slots plus 25 requested history IDs.

    inline std::vector<std::string> Split(std::string const& text, char separator)
    {
        std::vector<std::string> out;
        size_t start = 0, end;
        while ((end = text.find(separator, start)) != std::string::npos)
        {
            out.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        out.push_back(text.substr(start));
        return out;
    }

    inline bool Number(std::string const& value, uint32_t& result)
    {
        if (value.empty() || value.size() > 10)
            return false;
        uint64_t number = 0;
        for (char c : value)
        {
            if (c < '0' || c > '9') return false;
            number = number * 10 + (c - '0');
            if (number > UINT32_MAX) return false;
        }
        result = uint32_t(number);
        return true;
    }

    struct Request
    {
        std::string token, bot, hash;
        std::vector<uint32_t> questIds;
    };

    inline bool ParseRequest(std::string const& message, std::string const& bot, Request& result)
    {
        if (message.size() > MaxRequest) return false;
        auto fields = Split(message, '|');
        uint32_t token;
        if (fields.size() != 6 || fields[0] != "Q" || fields[1] != "1" ||
            !Number(fields[2], token) || !token || fields[3] != bot || bot.empty() || bot.size() > 48)
            return false;
        if (fields[4] != "0" && (fields[4].size() != 16 ||
            fields[4].find_first_not_of("0123456789abcdef") != std::string::npos)) return false;
        Request parsed{fields[2], bot, fields[4], {}};
        std::set<uint32_t> seen;
        if (!fields[5].empty())
        {
            for (auto const& id : Split(fields[5], ','))
            {
                uint32_t value;
                if (!Number(id, value) || !value || !seen.insert(value).second || seen.size() > 25)
                    return false;
                parsed.questIds.push_back(value);
            }
        }
        result = parsed;
        return true;
    }

    inline bool Admit(std::atomic<time_t>& lastRequest, time_t now)
    {
        time_t last = lastRequest.load();
        if (last && (now < last || now - last < 5)) return false;
        return lastRequest.compare_exchange_strong(last, now);
    }

    inline std::string Hex(std::string const& text)
    {
        static char const digits[] = "0123456789abcdef";
        std::string result;
        for (unsigned char c : text)
        {
            result += digits[c >> 4]; result += digits[c & 15];
        }
        return result;
    }

    inline std::string Hash(std::string const& body)
    {
        uint64_t value = UINT64_C(14695981039346656037);
        for (unsigned char c : body) { value ^= c; value *= UINT64_C(1099511628211); }
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << value;
        return out.str();
    }

    inline std::vector<std::string> Pack(Request const& request, std::string const& body)
    {
        std::string hash = Hash(body);
        std::string header = "|1|" + request.token + "|" + request.bot + "|" + hash;
        if (hash == request.hash) return {"U" + header};
        size_t count = std::max<size_t>(1, (body.size() + ChunkSize - 1) / ChunkSize);
        if (count > MaxChunks) return {"E|1|" + request.token + "|" + request.bot + "|TOO_LARGE"};
        std::vector<std::string> messages;
        for (size_t i = 0; i < count; ++i)
            messages.push_back("S" + header + "|" + std::to_string(i + 1) + "|" +
                std::to_string(count) + "|" + body.substr(i * ChunkSize, ChunkSize));
        return messages;
    }
}}
