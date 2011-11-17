#include <sparkles/operation.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>
#include <algorithm>
#include <memory>

namespace sparkles {
namespace test {

BOOST_AUTO_TEST_SUITE(operation_test)

typedef ::std::vector< ::std::string> finishedq_t;

class opthunk : public operation_base {
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
         BOOST_CHECK_MESSAGE(depsfinished_ == numdeps_,
                             depsfinished_ << " > " << numdeps_
                             << "in \"" << name_
                             << "\"->i_dependency_finished");
         bool all_finished = true;
         for_each_dependency([&all_finished](const opbase_ptr_t &dep) -> void {
               if (!dep->finished()) all_finished = false;
            });
         BOOST_CHECK_MESSAGE(all_finished,
                             "!all_finished in \"" << name_
                             << "\"->i_dependency_finished");
         set_finished();
      }
   }

 private:
   const ::std::string name_;
   finishedq_t &finishedq_;
   bool * const deleted_;
   unsigned int numdeps_, depsfinished_;
};

BOOST_AUTO_TEST_CASE( construct_empty )
{
   auto nested = []() {
      finishedq_t finishedq;
      ::std::shared_ptr<opthunk> fred{opthunk::create("fred", finishedq,
                                                      nullptr, {})};
   };
   BOOST_CHECK_NO_THROW(nested());
}

BOOST_AUTO_TEST_CASE( finish_empty )
{
   using ::std::shared_ptr;
   finishedq_t finishedq;
   shared_ptr<opthunk> fred{opthunk::create("fred", finishedq, nullptr, {})};
   fred->set_finished();
   auto correct = {"fred"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
}

BOOST_AUTO_TEST_CASE( finish_chain )
{
   using ::std::shared_ptr;
   finishedq_t finishedq;
   shared_ptr<opthunk> top{opthunk::create("a", finishedq, nullptr, {})};
   shared_ptr<opthunk> element{opthunk::create("b", finishedq, nullptr, {top})};
   element = opthunk::create("c", finishedq, nullptr, {element});
   element = opthunk::create("d", finishedq, nullptr, {element});
   BOOST_CHECK(!top->finished());
   BOOST_CHECK(!element->finished());
   top->set_finished();
   auto correct = {"a", "b", "c", "d"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
   BOOST_CHECK(top->finished());
   BOOST_CHECK(element->finished());
}

BOOST_AUTO_TEST_CASE( destroy_dependent )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("a", finishedq, nullptr, {})};
   bool next_gone = false;
   op_ptr next{opthunk::create("b", finishedq, &next_gone, {top})};
   next.reset();
   BOOST_CHECK(next_gone);
   BOOST_CHECK(finishedq.empty());
   top->set_finished();
   auto correct = {"a"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
   BOOST_CHECK(top->finished());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
