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
#include <vector>
#include <algorithm>

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
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, false);
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, true);
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, true);
   wq.enqueue(do_nothing, false);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(do_nothing, false);
   wq.enqueue(do_nothing, true);
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(wq.dequeue(false));
   BOOST_CHECK(!wq.dequeue(false));
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
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 0);
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 1);
   BOOST_CHECK(!wq.dequeue(false));
   wq.enqueue(bind(execute, 2));
   wq.enqueue(bind(execute, 3));
   wq.enqueue(bind(execute, 4), true);
   wq.enqueue(bind(execute, 5), true);
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 4);
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 5);
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 2);
   wq.dequeue(true).value()();
   BOOST_CHECK_EQUAL(which_executed, 3);
   BOOST_CHECK(!wq.dequeue(false));
}

BOOST_AUTO_TEST_CASE( dequeue_blocks )
{
   ::std::atomic<bool> before{false};
   ::std::atomic<bool> after{false};
   ::std::atomic<bool> ran{false};
   work_queue wq;
   auto reading_func = [&before, &after, &wq]() -> void {
      ::std::atomic_store(&before, true);
      work_queue::work_item_t item{wq.dequeue(true).value()};
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

BOOST_AUTO_TEST_CASE( stress_test )
{
   constexpr int num_enqueues = 1 << 20;
   using ::std::ref;
   auto execute = [](int &result, int val) -> void {
      result = val;
   };
   auto mass_enqueue = \
      [&execute](work_queue &wq, int &result,
                 int quantity, int start, int skip)
      -> void
      {
         for (int i = start; i < quantity; i += skip) {
            if ((i + 1) % 7 != 0) {
               wq.enqueue(::std::bind(execute, ref(result), i));
            } else {
               wq.enqueue(::std::bind(execute, ref(result), i + (1 << 24)),
                          true);
            }
         }
      };
   ::std::vector<int> results;
   int current_result;
   work_queue wq;
   results.reserve(num_enqueues);
   ::std::thread enqueueing_thread1(mass_enqueue, ref(wq),
                                    ref(current_result), num_enqueues, 0, 3);
   ::std::thread enqueueing_thread2(mass_enqueue, ref(wq),
                                    ref(current_result), num_enqueues, 1, 3);
   ::std::thread enqueueing_thread3(mass_enqueue, ref(wq),
                                    ref(current_result), num_enqueues, 2, 3);
   for (int i = 0; i < num_enqueues; ++i) {
      BOOST_TEST_CHECKPOINT("Dequeue #" << i);
      wq.dequeue(true).value()();
      results.push_back(current_result);
   }
   ::std::this_thread::yield();
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_CHECK(!wq.dequeue(false));
   {
      int last_result = -1;
      for (int &result: results) {
         if (result < last_result) {
            if (((result % 3) == (last_result % 3)) && \
                ((result >= (1 << 24)) || (last_result < (1 << 24))))
            {
               BOOST_CHECK_GT(result, last_result);
            }
         }
         last_result = result;
         result &= ((1 << 24) - 1);
      }
   }
   ::std::sort(results.begin(), results.end());
   {
      int last_num = -1;
      for (const int &result: results) {
         BOOST_CHECK_EQUAL(last_num + 1, result);
         last_num = result;
      }
   }

   BOOST_CHECK(!wq.dequeue(false));
   BOOST_REQUIRE(enqueueing_thread1.joinable());
   enqueueing_thread1.join();

   BOOST_CHECK(!wq.dequeue(false));
   BOOST_REQUIRE(enqueueing_thread2.joinable());
   enqueueing_thread2.join();

   BOOST_CHECK(!wq.dequeue(false));
   BOOST_REQUIRE(enqueueing_thread3.joinable());
   enqueueing_thread3.join();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
