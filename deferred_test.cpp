#include "test_operations.hpp"

#include <sparkles/errors.hpp>
#include <sparkles/deferred.hpp>

#include <boost/test/unit_test.hpp>

namespace {

void a_void_function()
{
}

void throws_exception(int)
{
   throw ::sparkles::test::test_exception("I refuse to work.");
}

void a_simpler_function(bool)
{
}

auto multiply_int(int a, int b) -> decltype(a * b)
{
   if (a == 42 || b == 42) {
      throw ::sparkles::test::test_exception("I won't multiply 42 by anything. It's already the answer.");
   }
   return a * b;
}

}

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(deferred_test)

#if 0  // This is a compile time test.
BOOST_AUTO_TEST_CASE( void_arg_and_result )
{
   using ::sparkles::defer;
   a_void_function_called = false;
   auto result = defer(a_void_function).until();
   BOOST_CHECK(result->finished());
}
#endif

BOOST_AUTO_TEST_CASE( bool_arg_and_result )
{
   finishedq_t q;
   using ::sparkles::defer;
   auto bool_op = nodep_op<bool>::create("bool", q, nullptr);
   bool_op->set_result(true);
   auto result = defer(a_simpler_function).until(bool_op);
   BOOST_CHECK(result->finished());
   BOOST_CHECK(!(result->is_error() || result->is_exception()));
   result->result();
}

BOOST_AUTO_TEST_CASE( bool_arg_and_result_lambda )
{
   typedef ::std::function<void(bool)> boolfunc_t;
   boolfunc_t innerfunc{ [](bool) -> void { } };
   finishedq_t q;
   using ::sparkles::defer;
   auto bool_op = nodep_op<bool>::create("bool", q, nullptr);
   bool_op->set_result(true);
   auto result = defer(innerfunc).until(bool_op);
   BOOST_CHECK(result->finished());
   BOOST_CHECK(!(result->is_error() || result->is_exception()));
   result->result();
}

BOOST_AUTO_TEST_CASE( multiply_int_test )
{
   finishedq_t q;
   using ::sparkles::defer;
   bool multiplicand_deleted = false;
   bool multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      multiplicand->set_result(1361);
      BOOST_CHECK(!result->finished());
      multiplier->set_result(1123);
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(!(result->is_error() || result->is_exception()));
      BOOST_CHECK_EQUAL(result->result(), 1528403);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
   multiplicand_deleted = false;
   multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      multiplier->set_result(1123);
      BOOST_CHECK(!result->finished());
      multiplicand->set_result(1361);
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(!(result->is_error() || result->is_exception()));
      BOOST_CHECK_EQUAL(result->result(), 1528403);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
}

BOOST_AUTO_TEST_CASE( multiply_int_arg_exception )
{
   finishedq_t q;
   using ::sparkles::defer;
   bool multiplicand_deleted = false;
   bool multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      try {
         throw test_exception("Just because I can.");
      } catch (...) {
         multiplicand->set_bad_result(::std::current_exception());
      }
      BOOST_CHECK(!multiplier->finished());
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
   multiplicand_deleted = false;
   multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      multiplicand->set_result(1361);
      BOOST_CHECK(!result->finished());
      try {
         throw test_exception("Just because I can.");
      } catch (...) {
         multiplier->set_bad_result(::std::current_exception());
      }
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
   multiplicand_deleted = false;
   multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      try {
         throw test_exception("Just because I can.");
      } catch (...) {
         multiplier->set_bad_result(::std::current_exception());
      }
      // Note, it is permissible here for result to be finished. But it is also
      // permissible for it not to be.
      multiplicand->set_result(1123);
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
   multiplicand_deleted = false;
   multiplier_deleted = false;
   {
      auto multiplicand = nodep_op<int>::create("multiplicand", q,
                                                &multiplicand_deleted);
      auto multiplier = nodep_op<int>::create("multiplier", q,
                                              &multiplier_deleted);
      auto result = defer(multiply_int).until(multiplicand, multiplier);
      BOOST_CHECK(!result->finished());
      multiplier->set_result(1361);
      BOOST_CHECK(!result->finished());
      try {
         throw test_exception("Just because I can.");
      } catch (...) {
         multiplicand->set_bad_result(::std::current_exception());
      }
      BOOST_CHECK(result->finished());
      multiplicand.reset();
      multiplier.reset();
      BOOST_CHECK(multiplicand_deleted);
      BOOST_CHECK(multiplier_deleted);
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(multiplicand_deleted);
   BOOST_CHECK(multiplier_deleted);
}

BOOST_AUTO_TEST_CASE( multiply_chain )
{
   finishedq_t q;
   using ::sparkles::defer;
   typedef operation<int>::ptr_t opint_ptr_t;

   bool op1_deleted = false;
   bool op2_deleted = false;
   bool op3_deleted = false;
   {
      auto op1 = nodep_op<int>::create("multiplicand", q,
                                       &op1_deleted);
      auto op2 = nodep_op<int>::create("multiplier", q,
                                       &op2_deleted);
      auto op3 = nodep_op<int>::create("multiplier", q,
                                       &op3_deleted);
      auto result = defer(multiply_int).until(op1, op2);
      result = defer(multiply_int).until(result, op3);
      BOOST_CHECK(!result->finished());
      op1->set_result(1123);
      BOOST_CHECK(!result->finished());
      op1.reset();
      BOOST_CHECK(!op1_deleted);
      op2->set_result(1361);
      BOOST_CHECK(!result->finished());
      op2.reset();
      op3->set_result(23);
      BOOST_CHECK(result->finished());
      op3.reset();
      BOOST_CHECK(op1_deleted);
      BOOST_CHECK(op2_deleted);
      BOOST_CHECK(op3_deleted);
      BOOST_CHECK_EQUAL(result->result(), 35153269);
   }
   BOOST_CHECK(op1_deleted);
   BOOST_CHECK(op2_deleted);
   BOOST_CHECK(op3_deleted);
}

BOOST_AUTO_TEST_CASE( multiply_chain_exception )
{
   finishedq_t q;
   using ::sparkles::defer;

   bool op1_deleted = false;
   bool op2_deleted = false;
   bool op3_deleted = false;
   {
      auto op1 = nodep_op<int>::create("multiplicand", q,
                                       &op1_deleted);
      auto op2 = nodep_op<int>::create("multiplier", q,
                                       &op2_deleted);
      auto op3 = nodep_op<int>::create("multiplier", q,
                                       &op3_deleted);
      auto result = defer(multiply_int).until(op1, op2);
      result = defer(multiply_int).until(result, op3);
      BOOST_CHECK(!result->finished());
      op1->set_result(1123);
      BOOST_CHECK(!result->finished());
      op1.reset();
      BOOST_CHECK(!op1_deleted);
      op2->set_result(42);
      BOOST_CHECK(result->finished());
      BOOST_CHECK(op1_deleted);
      op2.reset();
      op3.reset();
      BOOST_CHECK(op2_deleted);
      BOOST_CHECK(op3_deleted);
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(op1_deleted);
   BOOST_CHECK(op2_deleted);
   BOOST_CHECK(op3_deleted);
}

BOOST_AUTO_TEST_CASE( void_return_exception )
{
   finishedq_t q;
   using ::sparkles::defer;

   bool op1_deleted = false;
   {
      auto op1 = nodep_op<int>::create("junk", q,
                                       &op1_deleted);
      auto result = defer(throws_exception).until(op1);
      BOOST_CHECK(!result->finished());
      op1->set_result(1123);
      BOOST_CHECK(result->finished());
      BOOST_CHECK(result->is_exception());
      BOOST_CHECK_THROW(result->result(), test_exception);
   }
   BOOST_CHECK(op1_deleted);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
