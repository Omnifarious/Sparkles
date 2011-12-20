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

typedef ::std::vector< ::std::string> finishedq_t;

template <typename ResultType>
class base_testop : public operation<ResultType> {
   typedef operation<ResultType> baseclass_t;

 public:
   typedef typename baseclass_t::result_t result_t;
   typedef typename baseclass_t::opbase_ptr_t opbase_ptr_t;

   virtual ~base_testop() { if (deleted_) { *deleted_ = true; } }

   const ::std::string &name() const { return name_; }

 protected:
   base_testop(::std::string name, finishedq_t &finishedq, bool *deleted,
               const ::std::initializer_list<opbase_ptr_t> &lst)
        : baseclass_t(lst.begin(), lst.end()),
          name_(::std::move(name)), finishedq_(finishedq), deleted_(deleted)
   {
   }

   void set_result(result_t result) {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(result));
   }
   void set_bad_result(::std::exception_ptr exception)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_bad_result(::std::move(exception));
   }
   void set_bad_result(::std::error_code error)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_bad_result(::std::move(error));
   }

 private:
   const ::std::string name_;
   finishedq_t &finishedq_;
   bool * const deleted_;
};

template <typename ResultType>
class nodep_op : public base_testop<ResultType> {
   struct privclass {
   };
   typedef base_testop<ResultType> baseclass_t;

 public:
   typedef ::std::shared_ptr<nodep_op<ResultType> > ptr_t;
   typedef typename baseclass_t::result_t result_t;
   typedef typename baseclass_t::opbase_ptr_t opbase_ptr_t;

   nodep_op(const privclass &,
            ::std::string name, finishedq_t &finishedq, bool *deleted)
        : baseclass_t(::std::move(name), finishedq, deleted, {})
   {
   }


   void set_result(result_t result) {
      baseclass_t::set_result(::std::move(result));
   }
   void set_bad_result(::std::exception_ptr result) {
      baseclass_t::set_bad_result(::std::move(result));
   }
   void set_bad_result(::std::error_code result) {
      baseclass_t::set_bad_result(::std::move(result));
   }

   static ptr_t
   create(const ::std::string &name, finishedq_t &finishedq, bool *deleted)
   {
      typedef nodep_op<ResultType> me_t;
      ptr_t newthunk{
         ::std::make_shared<me_t>(privclass{}, name, finishedq, deleted)
            };
      register_as_dependent(newthunk);
      return ::std::move(newthunk);
   }

 private:
   virtual void i_dependency_finished(const opbase_ptr_t &)
   {
      throw ::std::runtime_error("This object should have no dependencies.");
   }
};

template <typename ResultType, typename Arg1_t, typename Arg2_t>
class op_add : public base_testop<ResultType>
{
   struct privclass {
   };
   typedef base_testop<ResultType> baseclass_t;

 public:
   typedef ::std::shared_ptr<op_add<ResultType, Arg1_t, Arg2_t> > ptr_t;
   typedef ::std::shared_ptr<operation<Arg1_t> > arg1_ptr_t;
   typedef ::std::shared_ptr<operation<Arg2_t> > arg2_ptr_t;
   typedef typename baseclass_t::result_t result_t;
   typedef typename baseclass_t::opbase_ptr_t opbase_ptr_t;

   op_add(const privclass &,
          const ::std::string &name, finishedq_t &finishedq, bool *deleted,
          arg1_ptr_t arg1, arg2_ptr_t arg2)
        : baseclass_t(name, finishedq, deleted, {arg1, arg2}),
          arg1_(::std::move(arg1)), arg2_(::std::move(arg2))
   {
      if (arg1_->finished()) {
         this->i_dependency_finished(arg1_);
      }
      if (!this->finished() && arg2_->finished()) {
         this->i_dependency_finished(arg2_);
      }
   }

   static ptr_t
   create(const ::std::string &name, finishedq_t &finishedq, bool *deleted,
          arg1_ptr_t arg1, arg2_ptr_t arg2)
   {
      typedef op_add<ResultType, Arg1_t, Arg2_t> me_t;
      ptr_t newthunk{
         ::std::make_shared<me_t>(privclass{}, name, finishedq, deleted,
                                  arg1, arg2)
            };
      register_as_dependent(newthunk);
      return ::std::move(newthunk);
   }

 private:
   arg1_ptr_t arg1_;
   arg2_ptr_t arg2_;

   virtual void i_dependency_finished(const opbase_ptr_t &dependency)
   {
      bool found_error = false;
      if (dependency == arg1_) {
         if (arg1_->is_exception()) {
            found_error = true;
            set_bad_result(arg1_->exception());
         } else if (arg1_->is_error()) {
            found_error = true;
            set_bad_result(arg1_->error());
         }
      } else if (dependency == arg2_) {
         if (arg2_->is_exception()) {
            found_error = true;
            set_bad_result(arg2_->exception());
         } else if (arg2_->is_error()) {
            found_error = true;
            set_bad_result(arg2_->error());
         }
      } else {
         throw ::std::runtime_error("A dependency I don't have finished.");
      }
      if (!found_error && arg1_->finished() && arg2_->finished()) {
         try {
            result_t result = arg1_->result() + arg2_->result();
            this->set_result(::std::move(result));
         } catch (...) {
            this->set_bad_result(::std::current_exception());
         }
      }
      if (this->finished()) {
         arg1_.reset();
         arg2_.reset();
      }
   }
};

template <typename Arg1_t, typename Arg2_t>
typename op_add<
   decltype(::std::declval<operation<Arg1_t> >().result() +             \
            ::std::declval<operation<Arg1_t> >().result()),
   Arg1_t,
   Arg2_t>::ptr_t
make_add(::std::string name, finishedq_t &finishedq, bool *deleted,
         typename operation<Arg1_t>::ptr_t arg1,
         typename operation<Arg2_t>::ptr_t arg2)
{
   typedef op_add< decltype(::std::declval<operation<Arg1_t> >().result() +  \
                            ::std::declval<operation<Arg1_t> >().result()),
                   Arg1_t,
                   Arg2_t> result_t;
   return result_t::create(name, finishedq, deleted, arg1, arg2);
}

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
