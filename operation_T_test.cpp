#include <sparkles/operation.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>
#include <algorithm>
#include <memory>
#include <utility>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(operation_T_test)

typedef ::std::vector< ::std::string> finishedq_t;

template <typename ResultType>
class nodep_op : public operation<ResultType> {
   struct privclass {
   };
   typedef operation<ResultType> baseclass_t;

 public:
   typedef ::std::shared_ptr<nodep_op<ResultType>> ptr_t;
   typedef typename baseclass_t::result_t result_t;
   typedef typename baseclass_t::opbase_ptr_t opbase_ptr_t;

   nodep_op(const privclass &,
            ::std::string name, finishedq_t &finishedq, bool *deleted)
        : operation_base({}),
          name_(::std::move(name)), finishedq_(finishedq), deleted_(deleted)
   {
   }
   virtual ~nodep_op() { if (deleted_) { *deleted_ = true; } }

   ::std::string name() const { return name_; }

   void set_result(result_t result) {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(result));
   }
   void set_result(::std::exception_ptr exception)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(exception));
   }
   void set_result(::std::error_code error)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(error));
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
   virtual void i_dependency_finished(const opbase_ptr_t &dependency)
   {
      throw ::std::runtime_error("This object should have no dependencies.");
   }

   const ::std::string name_;
   finishedq_t &finishedq_;
   bool * const deleted_;
};

template <typename ResultType, typename Arg1_t, typename Arg2_t>
class op_add : public operation<ResultType>
{
   struct privclass {
   };
   typedef operation<ResultType> baseclass_t;

 public:
   typedef ::std::shared_ptr<op_add<ResultType, Arg1_t, Arg2_t>> ptr_t;
   typedef ::std::shared_ptr<operation<Arg1_t>> arg1_ptr_t;
   typedef ::std::shared_ptr<operation<Arg2_t>> arg2_ptr_t;
   typedef typename baseclass_t::result_t result_t;
   typedef typename baseclass_t::opbase_ptr_t opbase_ptr_t;

   op_add(const privclass &,
          const ::std::string &name, finishedq_t &finishedq, bool *deleted,
          arg1_ptr_t arg1, arg2_ptr_t arg2)
        : operation_base({arg1, arg2}),
          name_(name), finishedq_(finishedq), deleted_(deleted),
          arg1_(::std::move(arg1)), arg2_(::std::move(arg2))
   {
      bool found_error = false;
      if (arg1_->finished()) {
         if (arg1_->is_exception()) {
            found_error = true;
            set_result(arg1_->exception());
         } else if (arg1_->is_error()) {
            found_error = true;
            set_result(arg1_->error());
         }
      } else if (arg2_->finished()) {
         if (arg2_->is_exception()) {
            found_error = true;
            set_result(arg2_->exception());
         } else if (arg2_->is_error()) {
            found_error = true;
            set_result(arg2_->error());
         }
      }
      if (!found_error && arg1_->finished() && arg2_->finished()) {
         set_result(arg1_->result() + arg2_->result());
      }
      if (this->finished()) {
         arg1_->reset();
         arg2_->reset();
      }
   }

   virtual ~op_add() { if (deleted_) { *deleted_ = true; } }

   void set_result(result_t result) {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(result));
   }
   void set_result(::std::exception_ptr exception)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(exception));
   }
   void set_result(::std::error_code error)
   {
      finishedq_.push_back(name_);
      baseclass_t::set_result(::std::move(error));
   }

 private:
   const ::std::string name_;
   finishedq_t &finishedq_;
   bool * const deleted_;
   arg1_ptr_t arg1_;
   arg2_ptr_t arg2_;
};

#if 0
template <typename ResultType>
class  : public operation<ResultType> {
   struct privclass {
   };

 public:
   opthunk(const privclass &,
           const ::std::string &name, finishedq_t &finishedq, bool *deleted,
           const ::std::initializer_list<opbase_ptr_t> &lst)
        : operation_base(lst.begin(), lst.end()),
          name_(name), finishedq_(finishedq), deleted_(deleted),
          numdeps_(lst.size()),
          depsfinished_(::std::count_if(lst.begin(), lst.end(),
                                        [](const opbase_ptr_t &dep) -> bool {
                                           return dep->finished();
                                        }
                           ))
   {
   }
   virtual ~opthunk() { if (deleted_) { *deleted_ = true; } }

   ::std::string name() const { return name_; }

   void set_finished() {
      finishedq_.push_back(name_);
      operation_base::set_finished();
   }

   void remove_dependency(const opbase_ptr_t &dependency) {
      if (dependency != nullptr) {
         operation_base::remove_dependency(dependency);
         if (!dependency->finished()) {
            if (++depsfinished_ >= numdeps_) {
               BOOST_CHECK_EQUAL(depsfinished_, numdeps_);
               bool all_finished = true;
               for_each_dependency(
                  [&all_finished](const opbase_ptr_t &dep) -> void {
                     if (!dep->finished()) all_finished = false;
                  });
               BOOST_CHECK(all_finished);
               BOOST_CHECK(!finished());
               if (!finished()) {
                  set_finished();
               }
            }
         }
      }
   }

   static ::std::shared_ptr<opthunk>
   create(const ::std::string &name, finishedq_t &finishedq, bool *deleted,
          const ::std::initializer_list<opbase_ptr_t> &lst)
   {
      ::std::shared_ptr<opthunk> newthunk{
         ::std::make_shared<opthunk>(privclass{}, name, finishedq, deleted, lst)
            };
      register_as_dependent(newthunk);
      return newthunk;
   }

 private:
   virtual void i_dependency_finished(const opbase_ptr_t &dependency) {
      BOOST_REQUIRE_MESSAGE(dependency->finished(),
                            "\"" << name_
                            << "\"->i_dependency_finished called with "
                            "unfinished dependency.");
      if (++depsfinished_ >= numdeps_) {
         BOOST_CHECK_EQUAL(depsfinished_, numdeps_);
         bool all_finished = true;
         for_each_dependency([&all_finished](const opbase_ptr_t &dep) -> void {
               if (!dep->finished()) all_finished = false;
            });
         BOOST_CHECK(all_finished);
         set_finished();
      }
   }

 private:
   const ::std::string name_;
   finishedq_t &finishedq_;
   bool * const deleted_;
   unsigned int numdeps_, depsfinished_;
};
#endif

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
