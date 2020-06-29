#pragma once

#include <list>
#include <unordered_map>
#include "DispatchQueue.h"

const size_t MaxCapacity = 1000;

template<typename Key, typename Value>
struct IConsumer
{
    virtual void Consume(const Key & id, const Value & value) noexcept = 0;
};

template<typename Key, typename Value>
class MultiQueueProcessor
{
public:
    virtual ~MultiQueueProcessor() {}

    virtual void Subscribe(const Key & id, IConsumer<Key, Value> * consumer) noexcept
    {
        dispatch_queue.dispatch(
            [this, id = std::move(id), consumer]() mutable noexcept
            {
                auto & queue = queues[id];
                if (!queue.consumer)
                {
                    queue.consumer = consumer;
                    consume(std::forward<Key>(id));
                }
            });
    }

    virtual void Unsubscribe(const Key & id) noexcept
    {
        dispatch_queue.dispatch(
            [this, id]() noexcept
            {
                auto queue_it = queues.find(id);
                if (queue_it == queues.end())
                    return;

                auto & queue = queue_it->second;

                queue.consumer = nullptr;
                if (queue.values.empty())
                    queues.erase(queue_it);
            });
    }

    template<typename T>
    void Enqueue(const Key & id, T && value) noexcept
    {
        dispatch_queue.dispatch(
            [this, id = std::move(id), value = std::move(value)]() mutable noexcept
            {
                auto & queue = queues[id];

                if (queue.values.size() >= MaxCapacity)
                    return;

                queue.values.emplace_back(std::forward<Value>(value));
                if (queue.values.size() == 1)
                    consume(std::forward<Key>(id));
            });
    }

    virtual Value Dequeue(const Key & id) noexcept
    {
        return dispatch_queue.dispatch_sync(
            [this, id]() noexcept
            {
                auto queue_it = queues.find(id);
                if (queue_it == queues.end())
                    return Value{};

                auto & queue = queue_it->second;

                if (queue.values.empty())
                    return Value{};

                auto result = std::move(queue.values.front());
                queue.values.pop_front();

                return result;
            });
    }

protected:
    void consume(Key && id) noexcept
    {
        auto queue_it = queues.find(id);
        if (queue_it == queues.end())
            return;

        auto & queue = queue_it->second;

        if (!queue.consumer || queue.values.empty())
            return;

        queue.consumer->Consume(id, queue.values.front());
        queue.values.pop_front();

        if (!queue.values.empty())
            dispatch_queue.dispatch([this, id = std::move(id)]() mutable noexcept {consume(std::forward<Key>(id));});
    }

protected:
    struct Queue
    {
        std::list<Value> values;
        IConsumer<Key, Value> * consumer = nullptr;
    };

    std::unordered_map<Key, Queue> queues;
    DispatchQueue dispatch_queue;
};
