#pragma once

#include "test_error.hpp"
#include <sparkles/operation.hpp>

namespace sparkles {
namespace test {

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
      try {
         baseclass_t::set_result(::std::move(result));
      } catch (...) {
         finishedq_.pop_back();
         throw;
      }
   }
   void set_bad_result(::std::exception_ptr exception)
   {
      finishedq_.push_back(name_);
      try {
         baseclass_t::set_bad_result(::std::move(exception));
      } catch (...) {
         finishedq_.pop_back();
         throw;
      }
   }
   void set_bad_result(::std::error_code error)
   {
      finishedq_.push_back(name_);
      try {
         baseclass_t::set_bad_result(::std::move(error));
      } catch (...) {
         finishedq_.pop_back();
         throw;
      }
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
      me_t::register_as_dependent(newthunk);
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
      me_t::register_as_dependent(newthunk);
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
            this->set_bad_result(arg1_->exception());
         } else if (arg1_->is_error()) {
            found_error = true;
            this->set_bad_result(arg1_->error());
         }
      } else if (dependency == arg2_) {
         if (arg2_->is_exception()) {
            found_error = true;
            this->set_bad_result(arg2_->exception());
         } else if (arg2_->is_error()) {
            found_error = true;
            this->set_bad_result(arg2_->error());
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
   decltype(::std::declval<operation<Arg1_t> >().result() +
            ::std::declval<operation<Arg1_t> >().result()),
   Arg1_t,
   Arg2_t>::ptr_t
make_add(::std::string name, finishedq_t &finishedq, bool *deleted,
         typename operation<Arg1_t>::ptr_t arg1,
         typename operation<Arg2_t>::ptr_t arg2)
{
   typedef op_add< decltype(::std::declval<operation<Arg1_t> >().result() +
                            ::std::declval<operation<Arg1_t> >().result()),
                   Arg1_t,
                   Arg2_t> result_t;
   return result_t::create(name, finishedq, deleted, arg1, arg2);
}

} // namespace test
} // namespace sparkles
