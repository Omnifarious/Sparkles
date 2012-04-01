// Required to make ::std::this_thread::sleep_for work.
#define _GLIBCXX_USE_NANOSLEEP
// Required to make ::std::this_thread::yield work.
#define _GLIBCXX_USE_SCHED_YIELD

#include "test_error.hpp"

#include <sparkles/errors.hpp>
#include <sparkles/remote_operation.hpp>
#include <sparkles/work_queue.hpp>

#include <boost/test/unit_test.hpp>

#include <system_error>
#include <exception>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace sparkles {
namespace test {

namespace {
::std::exception_ptr make_exception_ptr()
{
   try {
      throw test_exception("This should be stored.");
   } catch (...) {
      return ::std::current_exception();
   }
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(remote_operation_test)

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      work_queue wq;
      auto fred = remote_operation<int>::create(wq);
      auto barney = remote_operation<void>::create(wq);
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( int_valid )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<int>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_result(6);
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_EQUAL(fred.first->result(), 6);
   BOOST_CHECK_THROW(fred.second->set_result(5), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( int_error )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<int>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_bad_result(the_error);
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_EQUAL(fred.first->error(), the_error);
   BOOST_CHECK_THROW(fred.second->set_result(5), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( int_exception )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<int>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_bad_result(make_exception_ptr());
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_THROW(fred.first->result(), test_exception);
   BOOST_CHECK(fred.first->exception() != nullptr);
   BOOST_CHECK_THROW(fred.second->set_result(5), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( int_bad_sets )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<int>::create(wq);
   BOOST_CHECK_THROW(fred.second->set_bad_result(::std::error_code()),
                     ::std::invalid_argument);
   BOOST_CHECK_THROW(fred.second->set_bad_result(nullptr),
                     ::std::invalid_argument);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_result(6);
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_EQUAL(fred.first->result(), 6);
   BOOST_CHECK_THROW(fred.second->set_result(5), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( void_valid )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<void>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_result();
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_NO_THROW(fred.first->result());
   BOOST_CHECK_THROW(fred.second->set_result(), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( void_error )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<void>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_bad_result(the_error);
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_EQUAL(fred.first->error(), the_error);
   BOOST_CHECK_THROW(fred.second->set_result(), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( void_exception )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<void>::create(wq);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_bad_result(make_exception_ptr());
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_THROW(fred.first->result(), test_exception);
   BOOST_CHECK(fred.first->exception() != nullptr);
   BOOST_CHECK_THROW(fred.second->set_result(), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( void_bad_sets )
{
   const auto the_error = make_error_code(test_error::some_error);
   work_queue wq;
   auto fred = remote_operation<void>::create(wq);
   BOOST_CHECK_THROW(fred.second->set_bad_result(::std::error_code()),
                     ::std::invalid_argument);
   BOOST_CHECK_THROW(fred.second->set_bad_result(nullptr),
                     ::std::invalid_argument);
   BOOST_CHECK(!fred.first->finished());
   fred.second->set_result();
   BOOST_CHECK(!fred.first->finished());
   wq.dequeue()();
   BOOST_CHECK(fred.first->finished());
   BOOST_CHECK_NO_THROW(fred.first->result());
   BOOST_CHECK_THROW(fred.second->set_result(), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(the_error), invalid_result);
   BOOST_CHECK_THROW(fred.second->set_bad_result(make_exception_ptr()), invalid_result);
}

BOOST_AUTO_TEST_CASE( inter_thread )
{
   work_queue wq;
   remote_operation<int>::ptr_t op;
   remote_operation<int>::promise::ptr_t promise;
   ::std::tie(op, promise) = remote_operation<int>::create(wq);
   auto run = [promise]() -> void {
      promise->set_result(6);
   };
   promise = nullptr;
   ::std::thread promise_thread(run);
   BOOST_CHECK(!op->finished());
   wq.dequeue()();
   BOOST_CHECK(op->finished());
   BOOST_CHECK_EQUAL(op->result(), 6);
   ::std::this_thread::yield();
   ::std::this_thread::sleep_for(::std::chrono::milliseconds(20));
   BOOST_REQUIRE(promise_thread.joinable());
   promise_thread.join();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
