#define BOOST_TEST_MODULE MultiQueueProcessorTest

#include <iostream>
#include "MultiQueueProcessor.h"
#include <boost/test/unit_test.hpp>

class Consumer : public IConsumer<std::string, std::string>
{
public:
    void Consume(const std::string & id, const std::string & value) noexcept override
    {
        ++consumed;
    }

    size_t Consumed() const {return consumed;}

private:
    size_t consumed = 0;
};

BOOST_AUTO_TEST_CASE(test_enque_deque)
{
    MultiQueueProcessor<std::string, std::string> processor;

    BOOST_CHECK(processor.Dequeue("key1").empty());

    processor.Enqueue("key1", "value1");
    BOOST_CHECK(processor.Dequeue("key1") == "value1");

    BOOST_CHECK(processor.Dequeue("key1").empty());
}

BOOST_AUTO_TEST_CASE(test_max_capacity)
{
    MultiQueueProcessor<std::string, std::string> processor;

    for (size_t i = 0; i < 1001; ++i)
        processor.Enqueue("key1", "value1");

    for (size_t i = 0; i < 1000; ++i)
    {
        BOOST_CHECK(!processor.Dequeue("key1").empty());
    }

    BOOST_CHECK(processor.Dequeue("key1").empty());
}

BOOST_AUTO_TEST_CASE(test_out_of_scope)
{
    {
        MultiQueueProcessor<std::string, std::string> processor;
        Consumer consumer;
        processor.Subscribe("key", &consumer);
        processor.Enqueue("key1", "value1");
    }
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(test_produce_consume)
{
    MultiQueueProcessor<std::string, std::string> processor;

    std::array<Consumer, 32> consumers;

    std::array<std::unique_ptr<std::thread>, 32> threads;
    for (size_t i = 0; i < threads.size(); ++i)
    {
        threads[i] = std::make_unique<std::thread>(
            [&, i]()
            {
                for (int item_id = 0; item_id < 765; ++item_id)
                {
                    processor.Enqueue("key" + std::to_string(i), "value" + std::to_string(item_id));
                    if (item_id % 5 == 0)
                        processor.Subscribe("key" + std::to_string(i), &consumers[i]);
                    if (item_id % 10 == 0)
                        processor.Unsubscribe("key" + std::to_string(i));
                }
            });
    }

    for (size_t i = 0; i < threads.size(); ++i)
        threads[i]->join();

    for (size_t i = 0; i < consumers.size(); ++i)
        processor.Subscribe("key" + std::to_string(i), &consumers[i]);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (size_t i = 0; i < consumers.size(); ++i)
        BOOST_CHECK(consumers[i].Consumed() == 765);

    for (size_t i = 0; i < consumers.size(); ++i)
        BOOST_CHECK(processor.Dequeue("key" + std::to_string(i)).empty());
}
