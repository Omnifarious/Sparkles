#include "test_operations.hpp"
#include "test_error.hpp"

#include <sparkles/errors.hpp>
#include <sparkles/operation.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>
#include <memory>
#include <utility>
#include <exception>
#include <stdexcept>
#include <string>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(operation_T_test)

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      finishedq_t finishedq;
      operation<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                       finishedq, nullptr));
      operation<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                       finishedq, nullptr));
      auto adder = make_add<int, int>("adder", finishedq, nullptr, arg1, arg2);
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( no_result )
{
   finishedq_t finishedq;
   operation<int>::ptr_t op(nodep_op<int>::create("op", finishedq, nullptr));
   BOOST_CHECK_THROW(op->result(), invalid_result);
   BOOST_CHECK_THROW(op->error(), invalid_result);
   BOOST_CHECK_THROW(op->exception(), invalid_result);
}

BOOST_AUTO_TEST_CASE( bad_result )
{
   finishedq_t finishedq;
   nodep_op<int>::ptr_t op(nodep_op<int>::create("op", finishedq, nullptr));
   BOOST_CHECK_THROW(op->set_bad_result(nullptr), ::std::invalid_argument);
   BOOST_CHECK_THROW(op->set_bad_result(::std::error_code()),
                     ::std::invalid_argument);
}

BOOST_AUTO_TEST_CASE( double_set )
{
   auto make_exception_ptr = []() -> ::std::exception_ptr {
      try {
         throw test_exception("This should be stored.");
      } catch (...) {
         return ::std::current_exception();
      }
   };
   const auto the_error = make_error_code(test_error::some_error);
   finishedq_t finishedq;
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op1", finishedq, nullptr));
      op->set_result(1);
      BOOST_CHECK_THROW(op->set_bad_result(make_exception_ptr()),
                        invalid_result);
      BOOST_CHECK_EQUAL(op->result(), 1);
   }
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op2", finishedq, nullptr));
      op->set_result(2);
      BOOST_CHECK_THROW(op->set_bad_result(the_error), invalid_result);
      BOOST_CHECK_EQUAL(op->result(), 2);
   }
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op3", finishedq, nullptr));
      op->set_bad_result(make_exception_ptr());
      BOOST_CHECK_THROW(op->set_result(3), invalid_result);
      BOOST_CHECK_THROW(op->result(), test_exception);
      BOOST_CHECK(op->exception() != nullptr);
   }
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op4", finishedq, nullptr));
      op->set_bad_result(make_exception_ptr());
      BOOST_CHECK_THROW(op->set_bad_result(the_error), invalid_result);
      BOOST_CHECK_THROW(op->result(), test_exception);
      BOOST_CHECK(op->exception() != nullptr);
   }
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op5", finishedq, nullptr));
      op->set_bad_result(the_error);
      BOOST_CHECK_THROW(op->set_result(5), invalid_result);
      BOOST_CHECK_THROW(op->result(), ::std::system_error);
      BOOST_CHECK_EQUAL(op->error(), the_error);
   }
   {
      nodep_op<int>::ptr_t op(nodep_op<int>::create("op6", finishedq, nullptr));
      op->set_bad_result(the_error);
      BOOST_CHECK_THROW(op->set_bad_result(make_exception_ptr()),
                        invalid_result);
      BOOST_CHECK_THROW(op->result(), ::std::system_error);
      BOOST_CHECK_EQUAL(op->error(), the_error);
   }
   auto correct = {"op1", "op2", "op3", "op4", "op5", "op6"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( test_normal )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                   finishedq, &arg1_gone));
   bool arg2_gone = false;
   nodep_op<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                   finishedq, &arg2_gone));
   bool adder_gone = false;
   auto adder = make_add<int, int>("adder", finishedq, &adder_gone, arg1, arg2);
   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   arg1->set_result(5);
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   BOOST_CHECK_EQUAL(arg1->result(), 5);
   arg2->set_result(7);
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(arg2->finished());
   BOOST_CHECK(adder->finished());
   BOOST_CHECK_EQUAL(arg2->result(), 7);
   BOOST_CHECK_EQUAL(adder->result(), 12);
   BOOST_CHECK_THROW(adder->error(), invalid_result);
   BOOST_CHECK_THROW(adder->exception(), invalid_result);
   BOOST_CHECK(!arg1_gone);
   arg1.reset();
   BOOST_CHECK(arg1_gone);
   BOOST_CHECK(!arg2_gone);
   arg2.reset();
   BOOST_CHECK(arg2_gone);
   BOOST_CHECK(!adder_gone);
   adder.reset();
   BOOST_CHECK(adder_gone);
   auto correct = {"arg1", "arg2", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( test_except_arg1 )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                   finishedq, &arg1_gone));
   bool arg2_gone = false;
   nodep_op<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                   finishedq, &arg2_gone));
   bool adder_gone = false;
   auto adder = make_add<int, int>("adder", finishedq, &adder_gone, arg1, arg2);

   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   try {
      throw test_exception("This should be stored.");
   } catch (...) {
      BOOST_CHECK(!arg1->finished());
      BOOST_CHECK(!arg1->is_valid());
      arg1->set_bad_result(::std::current_exception());
   }
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(adder->finished());

   arg1.reset();
   BOOST_CHECK(arg1_gone);
   arg2.reset();
   BOOST_CHECK(arg2_gone);
   BOOST_CHECK(adder->is_exception());
   BOOST_CHECK(!adder->is_error());
   BOOST_CHECK_THROW(adder->result(), test_exception);
   BOOST_CHECK_THROW(adder->error(), invalid_result);
   BOOST_CHECK_NO_THROW(adder->exception());
   auto correct = {"arg1", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( test_except_arg2 )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                   finishedq, &arg1_gone));
   bool arg2_gone = false;
   nodep_op<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                   finishedq, &arg2_gone));
   bool adder_gone = false;
   auto adder = make_add<int, int>("adder", finishedq, &adder_gone, arg1, arg2);

   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   try {
      throw test_exception("This should be stored.");
   } catch (...) {
      BOOST_CHECK(!arg2->finished());
      BOOST_CHECK(!arg2->is_valid());
      arg2->set_bad_result(::std::current_exception());
   }
   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(arg2->finished());
   BOOST_CHECK(adder->finished());

   arg1.reset();
   BOOST_CHECK(arg1_gone);
   arg2.reset();
   BOOST_CHECK(arg2_gone);
   BOOST_CHECK(adder->is_exception());
   BOOST_CHECK(!adder->is_error());
   BOOST_CHECK_THROW(adder->result(), test_exception);
   BOOST_CHECK_THROW(adder->error(), invalid_result);
   BOOST_CHECK_NO_THROW(adder->exception());
   auto correct = {"arg2", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

struct except_on_add {
};

except_on_add operator +(const except_on_add &, const except_on_add &)
{
   throw test_exception("Adding failed!");
}

BOOST_AUTO_TEST_CASE( test_except_adder )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<except_on_add>::ptr_t arg1(
      nodep_op<except_on_add>::create("arg1", finishedq, &arg1_gone)
      );
   bool arg2_gone = false;
   nodep_op<except_on_add>::ptr_t arg2(
      nodep_op<except_on_add>::create("arg2", finishedq, &arg2_gone)
      );
   bool adder_gone = false;
   auto adder = make_add<except_on_add, except_on_add>("adder",
                                                       finishedq,
                                                       &adder_gone,
                                                       arg1, arg2);

   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   arg1->set_result(except_on_add{});
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   arg2->set_result(except_on_add());
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(arg2->finished());
   BOOST_CHECK(adder->finished());
   BOOST_CHECK_THROW(adder->result(), test_exception);
   BOOST_CHECK_THROW(adder->result(), test_exception);
   BOOST_CHECK_THROW(adder->error(), invalid_result);
   BOOST_CHECK_NO_THROW(adder->exception());
   auto correct = {"arg1", "arg2", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( test_error_arg1 )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                   finishedq, &arg1_gone));
   bool arg2_gone = false;
   nodep_op<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                   finishedq, &arg2_gone));
   bool adder_gone = false;
   auto adder = make_add<int, int>("adder", finishedq, &adder_gone, arg1, arg2);

   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   arg1->set_bad_result(make_error_code(test_error::some_error));
   BOOST_CHECK(arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(adder->finished());

   arg1.reset();
   BOOST_CHECK(arg1_gone);
   arg2.reset();
   BOOST_CHECK(arg2_gone);
   BOOST_CHECK(!adder->is_exception());
   BOOST_CHECK(adder->is_error());
   BOOST_CHECK_THROW(adder->result(), ::std::system_error);
   BOOST_CHECK_EQUAL(adder->error(), make_error_code(test_error::some_error));
   BOOST_CHECK_THROW(adder->exception(), invalid_result);
   auto correct = {"arg1", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( test_error_arg2 )
{
   finishedq_t finishedq;
   bool arg1_gone = false;
   nodep_op<int>::ptr_t arg1(nodep_op<int>::create("arg1",
                                                   finishedq, &arg1_gone));
   bool arg2_gone = false;
   nodep_op<int>::ptr_t arg2(nodep_op<int>::create("arg2",
                                                   finishedq, &arg2_gone));
   bool adder_gone = false;
   auto adder = make_add<int, int>("adder", finishedq, &adder_gone, arg1, arg2);

   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(!arg2->finished());
   BOOST_CHECK(!adder->finished());
   arg2->set_bad_result(make_error_code(test_error::some_error));
   BOOST_CHECK(!arg1->finished());
   BOOST_CHECK(arg2->finished());
   BOOST_CHECK(adder->finished());

   arg1.reset();
   BOOST_CHECK(arg1_gone);
   arg2.reset();
   BOOST_CHECK(arg2_gone);
   BOOST_CHECK(!adder->is_exception());
   BOOST_CHECK(adder->is_error());
   BOOST_CHECK_THROW(adder->result(), ::std::system_error);
   BOOST_CHECK_EQUAL(adder->error(), make_error_code(test_error::some_error));
   BOOST_CHECK_THROW(adder->exception(), invalid_result);
   auto correct = {"arg2", "adder"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
