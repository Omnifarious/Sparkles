#include "test_operations.hpp"
#include "test_error.hpp"

#include <sparkles/errors.hpp>
#include <sparkles/remote_operation.hpp>
#include <sparkles/work_queue.hpp>

#include <boost/test/unit_test.hpp>

#include <system_error>

namespace sparkles {
namespace test {

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
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
