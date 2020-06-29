#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <future>

class DispatchQueue
{
public:
    DispatchQueue()
        : io_service_worker{std::make_unique<boost::asio::io_service::work>(io_service)}
        , thread{[this]() noexcept {io_service.run();}}
    {}

    virtual ~DispatchQueue()
    {
        io_service_worker.reset();
        thread.join();
    }

    template<typename T>
    void dispatch(T && task) noexcept
    {
        io_service.post([this, task = std::move(task)]() mutable noexcept {task();});
    }

    template<typename T>
    auto dispatch_sync(T && task) noexcept
    {
        if (std::this_thread::get_id() == thread.get_id())
            return task();

        std::packaged_task<decltype(task())()> packaged_task(std::move(task));
        dispatch([&packaged_task]() noexcept {packaged_task();});
        return packaged_task.get_future().get();
    }

private:
    boost::asio::io_service io_service;
    std::unique_ptr<boost::asio::io_service::work> io_service_worker;
    std::thread thread;
};
