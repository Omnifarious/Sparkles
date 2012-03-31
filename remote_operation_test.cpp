// Required to make ::std::this_thread::sleep_for work.
#define _GLIBCXX_USE_NANOSLEEP

#include "test_operations.hpp"
#include "test_error.hpp"

#include <sparkles/errors.hpp>
#include <sparkles/remote_operation.hpp>
#include <sparkles/work_queue.hpp>

#include <boost/test/unit_test.hpp>

#include <thread>
#include <chrono>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(remote_operation_test)

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      finishedq_t finishedq;
      work_queue wq;
      operation<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                       finishedq, nullptr));
      operation<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                       finishedq, nullptr));
      auto adder = make_add<int, int>("adder", finishedq, nullptr, arg1, arg2);
      auto remote = remote_operation<int>::create(adder, wq);
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( basic_add )
{
   finishedq_t finishedq;
   work_queue wq;
   bool del_op1 = false;
   auto op1 = nodep_op<int>::create("arg1", finishedq, &del_op1);
   bool del_op2 = false;
   auto op2 = nodep_op<int>::create("arg2", finishedq, &del_op2);
   bool del_adder = false;
   auto adder = make_add<int, int>("adder",
                                   finishedq, &del_adder, op1, op2);
   auto remote = remote_operation<int>::create(adder, wq);
   BOOST_CHECK(!del_op1);
   BOOST_CHECK(!(op1->is_valid() || op1->finished()));
   BOOST_CHECK(!del_op2);
   BOOST_CHECK(!(op2->is_valid() || op2->finished()));
   BOOST_CHECK(!del_adder);
   BOOST_CHECK(!(adder->is_valid() || adder->finished()));
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   op1->set_result(5);
   BOOST_CHECK(!del_op1);
   BOOST_CHECK(op1->is_valid() && op1->finished());
   op1.reset();
   BOOST_CHECK(!del_op1);
   BOOST_CHECK(!del_op2);
   BOOST_CHECK(!(op2->is_valid() || op2->finished()));
   BOOST_CHECK(!del_adder);
   BOOST_CHECK(!(adder->is_valid() || adder->finished()));
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   op2->set_result(6);
   BOOST_CHECK(del_op1);
   BOOST_CHECK(!del_op2);
   BOOST_CHECK(op2->is_valid() && op2->finished());
   op2.reset();
   BOOST_CHECK(del_op2);
   BOOST_CHECK(!del_adder);
   BOOST_CHECK(adder->is_valid() && adder->finished());
   adder.reset();
   BOOST_CHECK(del_adder);
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));
   wq.dequeue()();
   BOOST_CHECK(remote->is_valid() && remote->finished());
   BOOST_CHECK_EQUAL(remote->result(), 11);
   {
      work_queue::work_item_t tmp;
      BOOST_CHECK(!wq.try_dequeue(tmp));
   }
   auto correct = {"arg1", "arg2", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( propagate_result )
{
   finishedq_t finishedq;
   work_queue wq;
   auto op1 = nodep_op<int>::create("arg1", finishedq, nullptr);
   auto remote = remote_operation<int>::create(op1, wq);
   BOOST_CHECK(!(op1->is_valid() || op1->finished()));
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   op1->set_result(5);
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK_EQUAL(op1->result(), 5);
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   wq.dequeue()();
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK(remote->is_valid() && remote->finished());
   BOOST_CHECK_EQUAL(op1->result(), 5);
   BOOST_CHECK_EQUAL(remote->result(), 5);
   {
      work_queue::work_item_t tmp;
      BOOST_CHECK(!wq.try_dequeue(tmp));
   }
}

BOOST_AUTO_TEST_CASE( propagate_error )
{
   finishedq_t finishedq;
   work_queue wq;
   auto op1 = nodep_op<int>::create("arg1", finishedq, nullptr);
   auto remote = remote_operation<int>::create(op1, wq);
   BOOST_CHECK(!(op1->is_valid() || op1->finished()));
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   const auto my_error = make_error_code(test_error::some_error);
   op1->set_bad_result(my_error);
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK_EQUAL(op1->error(), my_error);
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   wq.dequeue()();
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK(remote->is_valid() && remote->finished());
   BOOST_CHECK_EQUAL(op1->error(), my_error);
   BOOST_CHECK_EQUAL(remote->error(), my_error);
   {
      work_queue::work_item_t tmp;
      BOOST_CHECK(!wq.try_dequeue(tmp));
   }
}

BOOST_AUTO_TEST_CASE( propagate_exception )
{
   finishedq_t finishedq;
   work_queue wq;
   auto make_exception_ptr = []() -> ::std::exception_ptr {
      try {
         throw test_exception("This should be stored.");
      } catch (...) {
         return ::std::current_exception();
      }
   };
   auto op1 = nodep_op<int>::create("arg1", finishedq, nullptr);
   auto remote = remote_operation<int>::create(op1, wq);
   BOOST_CHECK(!(op1->is_valid() || op1->finished()));
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   op1->set_bad_result(make_exception_ptr());
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK_THROW(op1->result(), test_exception);
   BOOST_CHECK(!(remote->is_valid() || remote->finished()));

   wq.dequeue()();
   BOOST_CHECK(op1->is_valid() && op1->finished());
   BOOST_CHECK(remote->is_valid() && remote->finished());
   BOOST_CHECK_THROW(op1->result(), test_exception);
   BOOST_CHECK_THROW(remote->result(), test_exception);
   {
      work_queue::work_item_t tmp;
      BOOST_CHECK(!wq.try_dequeue(tmp));
   }
}

BOOST_AUTO_TEST_CASE( simple_threaded )
{
   class remote_thread {
    public:
      remote_thread() : finished_(false) {}

      void operator ()() {
         while (!finished_) {
            wq_.dequeue()();
         }
      }

      remote_operation<int>::ptr_t
      remote_add(work_queue &localq, int a, int b);

    private:
      work_queue wq_;
      bool finished_;
      finishedq_t finishedq_;
   };
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
