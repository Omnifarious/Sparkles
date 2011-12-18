// Required to make ::std::this_thread::sleep_for work.
#define _GLIBCXX_USE_NANOSLEEP
// Required to make ::std::this_thread::yield work.
#define _GLIBCXX_USE_SCHED_YIELD

#include <sparkles/work_queue.hpp>

#include <boost/test/unit_test.hpp>

#include <functional>
#include <atomic>
#include <thread>
#include <utility>
#include <chrono>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(work_queue_test)

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      work_queue wq;
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( add_items )
{
   auto do_nothing = []() -> void {
   };
   work_queue wq;
   BOOST_TEST_CHECKPOINT("Created work queue");
   for (int i = 0; i < 6; ++i) {
      wq.enqueue(do_nothing, false);
      BOOST_TEST_CHECKPOINT("Added normal item #" << i);
   }
   for (int i = 0; i < 3; ++i) {
      wq.enqueue(do_nothing, true);
      BOOST_TEST_CHECKPOINT("Added oob item #" << i);
   }
}

BOOST_AUTO_TEST_CASE( add_remove_items )
{
   auto do_nothing = []() -> void {
   };
   work_queue wq;
   work_queue::work_item_t item;
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, false);
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, true);
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, true);
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
   wq.enqueue(do_nothing, false);
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(wq.try_dequeue(item));
   BOOST_CHECK(!wq.try_dequeue(item));
}

BOOST_AUTO_TEST_CASE( oob_first )
{
   using ::std::bind;
   int which_executed = -1;
   auto execute = [&which_executed](int which) -> void {
      which_executed = which;
   };
   work_queue wq;
   wq.enqueue(bind(execute, 0));
   wq.enqueue(bind(execute, 1));
   BOOST_CHECK_EQUAL(which_executed, -1);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 0);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 1);
   {
      work_queue::work_item_t item;
      BOOST_CHECK(!wq.try_dequeue(item));
   }
   wq.enqueue(bind(execute, 2));
   wq.enqueue(bind(execute, 3));
   wq.enqueue(bind(execute, 4), true);
   wq.enqueue(bind(execute, 5), true);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 4);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 5);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 2);
   wq.dequeue()();
   BOOST_CHECK_EQUAL(which_executed, 3);
   {
      work_queue::work_item_t item;
      BOOST_CHECK(!wq.try_dequeue(item));
   }
}

BOOST_AUTO_TEST_CASE( dequeue_blocks )
{
   ::std::atomic<bool> before{false};
   ::std::atomic<bool> after{false};
   ::std::atomic<bool> ran{false};
   work_queue wq;
   auto reading_func = [&before, &after, &wq]() -> void {
      ::std::atomic_store(&before, true);
      work_queue::work_item_t item{wq.dequeue()};
      ::std::atomic_store(&after, true);
      item();
   };
   auto run = [&ran]() -> void {
      ::std::atomic_store(&ran, true);
   };
   ::std::thread reading_thread(reading_func);
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_CHECK(::std::atomic_load(&before));
   BOOST_CHECK(!::std::atomic_load(&after));
   BOOST_CHECK(!::std::atomic_load(&ran));
   wq.enqueue(run);
   ::std::this_thread::yield();
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_CHECK(::std::atomic_load(&before));
   BOOST_CHECK(::std::atomic_load(&after));
   BOOST_CHECK(::std::atomic_load(&ran));
   BOOST_REQUIRE(reading_thread.joinable());
   reading_thread.join();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles