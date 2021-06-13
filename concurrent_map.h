#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(Value& value, std::mutex& m)
            : ref_to_value(value)
            , m_outer(m) {
        }

        Value& ref_to_value;
        std::mutex& m_outer;

        ~Access() {
            m_outer.unlock();
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : bucket_count_(bucket_count)
        , mutexes_(bucket_count) {

        if (bucket_count_ == 0u) {
            throw std::invalid_argument("Invalid bucket_count"s);
        }
        super_map_.resize(bucket_count);
    };

    Access operator[](const Key& key) {
        std::atomic<uint64_t> map_number = static_cast<uint64_t>(key) % bucket_count_;

        mutexes_[map_number].lock();

        auto& link = super_map_[map_number][key];

        return { link, mutexes_[map_number] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        std::atomic_int count = 0;

        for (auto& it : super_map_) {
            mutexes_[count].lock();
            result.merge(it);
            mutexes_[count].unlock();
            ++count;
        }

        return result;
    }

private:
    std::vector<std::map<Key, Value>> super_map_;
    size_t bucket_count_;

    Value value_;
    std::vector<std::mutex> mutexes_;
};