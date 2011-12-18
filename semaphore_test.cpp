// Required to make ::std::this_thread::sleep_for work.
#define _GLIBCXX_USE_NANOSLEEP

#include <sparkles/semaphore.hpp>

#include <boost/test/unit_test.hpp>
#include <thread>
#include <atomic>
#include <chrono>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(semaphore_test)

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      semaphore s;
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( try_acquire )
{
   semaphore s(1);
   BOOST_CHECK(s.try_acquire());
   BOOST_CHECK(!s.try_acquire());
   BOOST_CHECK_NO_THROW(s.release());
   BOOST_CHECK(s.try_acquire());
   BOOST_CHECK(!s.try_acquire());
}

BOOST_AUTO_TEST_CASE( ping_pong_one )
{
   ::std::atomic<int> ping{1};
   semaphore ping_sem;
   ::std::atomic<int> pong{0};
   semaphore pong_sem(1);
   ::std::atomic_flag ping_finished = ATOMIC_FLAG_INIT;
   auto ping_func = [&]() {
      for (int i = 0; i < 10000; ++i) {
         ping_sem.acquire();
         atomic_fetch_add(&ping, 1);
         atomic_fetch_sub(&pong, 1);
         BOOST_REQUIRE_EQUAL(ping, 1);
         BOOST_REQUIRE_EQUAL(pong, 0);
         pong_sem.release();
      }
      ping_finished.test_and_set();
   };
   BOOST_REQUIRE_EQUAL(ping, 1);
   BOOST_REQUIRE_EQUAL(pong, 0);
   ::std::thread ping_thread(ping_func);
   ping_thread.detach();
   for (int i = 0; i < 10000; ++i) {
      pong_sem.acquire();
      atomic_fetch_sub(&ping, 1);
      atomic_fetch_add(&pong, 1);
      BOOST_REQUIRE_EQUAL(ping, 0);
      BOOST_REQUIRE_EQUAL(pong, 1);
      ping_sem.release();
   }
   // Yes, it's a race condition. You figure out how to check this properly
   // without a race condition using the Boost unit testing framework.
   //
   // I'd like to call join with a timeout, but there's no Boost method to do
   // that that I can find.
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_REQUIRE(ping_finished.test_and_set());
}

BOOST_AUTO_TEST_CASE( ping_pong_five )
{
   ::std::atomic<int> ping{5};
   semaphore ping_sem;
   ::std::atomic<int> pong{0};
   semaphore pong_sem(5);
   ::std::atomic_flag ping_finished = ATOMIC_FLAG_INIT;
   auto ping_func = [&]() {
      for (int i = 0; i < 10000; ++i) {
         ping_sem.acquire();
         int myping = atomic_fetch_add(&ping, 1) + 1;
         int mypong = atomic_fetch_sub(&pong, 1) - 1;
         BOOST_REQUIRE_LE(myping, 5);
         BOOST_REQUIRE_GT(myping, 0);
         BOOST_REQUIRE_LT(mypong, 5);
         BOOST_REQUIRE_GE(mypong, 0);
         pong_sem.release();
      }
      ping_finished.test_and_set();
   };
   BOOST_REQUIRE_EQUAL(ping, 5);
   BOOST_REQUIRE_EQUAL(pong, 0);
   ::std::thread ping_thread(ping_func);
   ping_thread.detach();
   for (int i = 0; i < 10000; ++i) {
      pong_sem.acquire();
      int myping = atomic_fetch_sub(&ping, 1) - 1;
      int mypong = atomic_fetch_add(&pong, 1) + 1;
      BOOST_REQUIRE_LT(myping, 5);
      BOOST_REQUIRE_GE(myping, 0);
      BOOST_REQUIRE_LE(mypong, 5);
      BOOST_REQUIRE_GT(mypong, 0);
      ping_sem.release();
   }
   // Yes, it's a race condition. You figure out how to check this properly
   // without a race condition using the Boost unit testing framework.
   //
   // I'd like to call join with a timeout, but there's no Boost method to do
   // that that I can find.
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_REQUIRE(ping_finished.test_and_set());
   {
      int myping = atomic_load(&ping);
      int mypong = atomic_load(&pong);
      BOOST_REQUIRE_LE(myping, 5);
      BOOST_REQUIRE_GE(myping, 0);
      BOOST_REQUIRE_LE(mypong, 5);
      BOOST_REQUIRE_GE(mypong, 0);
      BOOST_REQUIRE_EQUAL(myping + mypong, 5);
      BOOST_REQUIRE_EQUAL(myping, pong_sem.getvalue());
      BOOST_REQUIRE_EQUAL(mypong, ping_sem.getvalue());
   }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
