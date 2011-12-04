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
               if (!finished()) {
                  set_finished();
               } else {
                  BOOST_CHECK_EQUAL(num_dependencies(), 0);
                  finishedq_.push_back(name_);
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

BOOST_AUTO_TEST_CASE( destroy_dependent_chain )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("a", finishedq, nullptr, {})};
   bool b_gone = false;
   op_ptr next{opthunk::create("b", finishedq, &b_gone, {top})};
   bool c_gone = false;
   next = opthunk::create("b", finishedq, &c_gone, {next});
   BOOST_CHECK(!b_gone);
   BOOST_CHECK(!c_gone);
   next.reset();
   BOOST_CHECK(b_gone);
   BOOST_CHECK(c_gone);
   BOOST_CHECK(finishedq.empty());
   top->set_finished();
   auto correct = {"a"};
   BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                 correct.begin(), correct.end());
   BOOST_CHECK(top->finished());
}

BOOST_AUTO_TEST_CASE( forked_chain )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("top", finishedq, nullptr, {})};
   op_ptr chain_a{opthunk::create("a.a", finishedq, nullptr, {top})};
   chain_a = opthunk::create("a.b", finishedq, nullptr, {chain_a});
   op_ptr chain_b{opthunk::create("b.a", finishedq, nullptr, {top})};
   chain_b = opthunk::create("b.b", finishedq, nullptr, {chain_b});
   BOOST_CHECK(!top->finished());
   BOOST_CHECK(!chain_a->finished());
   BOOST_CHECK(!chain_b->finished());
   BOOST_CHECK(finishedq.empty());
   top->set_finished();
   BOOST_CHECK_EQUAL(finishedq.size(), 5);
   if (finishedq.at(1) == "a.a") {
      auto correct = {"top", "a.a", "a.b", "b.a", "b.b"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   } else {
      auto correct = {"top", "b.a", "b.b", "a.a", "a.b"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
   BOOST_CHECK(top->finished());
   BOOST_CHECK(chain_a->finished());
   BOOST_CHECK(chain_b->finished());
}

BOOST_AUTO_TEST_CASE( check_v )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top_a{opthunk::create("top_a", finishedq, nullptr, {})};
   op_ptr top_b{opthunk::create("top_b", finishedq, nullptr, {})};
   op_ptr bottom{opthunk::create("bottom", finishedq, nullptr, {top_a, top_b})};

   BOOST_CHECK(!top_a->finished());
   BOOST_CHECK(!top_b->finished());
   BOOST_CHECK(!bottom->finished());
   BOOST_CHECK(finishedq.empty());
   top_a->set_finished();
   BOOST_CHECK(top_a->finished());
   BOOST_CHECK(!top_b->finished());
   BOOST_CHECK(!bottom->finished());
   {
      auto correct = {"top_a"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
   top_b->set_finished();
   BOOST_CHECK(top_a->finished());
   BOOST_CHECK(top_b->finished());
   BOOST_CHECK(bottom->finished());
   {
      auto correct = {"top_a", "top_b", "bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
}

BOOST_AUTO_TEST_CASE( remove_dep_bad )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("top", finishedq, nullptr, {})};
   op_ptr bottom{opthunk::create("bottom", finishedq, nullptr, {top})};
   BOOST_CHECK_THROW(top->remove_dependency(bottom), bad_dependency);
   BOOST_CHECK(finishedq.empty());
}

BOOST_AUTO_TEST_CASE( remove_dep_good )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("top", finishedq, nullptr, {})};
   op_ptr bottom{opthunk::create("bottom", finishedq, nullptr, {top})};
   BOOST_CHECK_NO_THROW(bottom->remove_dependency(top));
   {
      auto correct = {"bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
}

BOOST_AUTO_TEST_CASE( remove_dep_good_v_part_a )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top_a{opthunk::create("top_a", finishedq, nullptr, {})};
   op_ptr top_b{opthunk::create("top_b", finishedq, nullptr, {})};
   op_ptr bottom{opthunk::create("bottom", finishedq, nullptr, {top_a, top_b})};
   BOOST_CHECK_NO_THROW(bottom->remove_dependency(top_a));
   BOOST_CHECK(!top_b->finished());
   BOOST_CHECK(!bottom->finished());
   BOOST_CHECK(finishedq.empty());
   top_b->set_finished();
   BOOST_CHECK(top_b->finished());
   BOOST_CHECK(bottom->finished());
   {
      auto correct = {"top_b", "bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
}

BOOST_AUTO_TEST_CASE( remove_dep_good_v_part_b )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top_a{opthunk::create("top_a", finishedq, nullptr, {})};
   op_ptr top_b{opthunk::create("top_b", finishedq, nullptr, {})};
   op_ptr bottom{opthunk::create("bottom", finishedq, nullptr, {top_a, top_b})};
   top_b->set_finished();
   BOOST_CHECK(top_b->finished());
   BOOST_CHECK(!bottom->finished());
   {
      auto correct = {"top_b"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
   BOOST_CHECK_NO_THROW(bottom->remove_dependency(top_a));
   BOOST_CHECK(top_b->finished());
   BOOST_CHECK(bottom->finished());
   {
      auto correct = {"top_b", "bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
}

BOOST_AUTO_TEST_CASE( diamond )
{
   typedef ::std::shared_ptr<opthunk> op_ptr;
   finishedq_t finishedq;
   op_ptr top{opthunk::create("top", finishedq, nullptr, {})};
   op_ptr bottom;
   {
      op_ptr left{opthunk::create("left", finishedq, nullptr, {top})};
      op_ptr right{opthunk::create("right", finishedq, nullptr, {top})};
      bottom = opthunk::create("bottom", finishedq, nullptr, {left, right});
   }
   BOOST_CHECK(!top->finished());
   BOOST_CHECK(!bottom->finished());
   top->set_finished();
   BOOST_CHECK(top->finished());
   BOOST_CHECK(bottom->finished());
   BOOST_REQUIRE_EQUAL(finishedq.size(), 4);
   if (finishedq.at(1) == "left")
   {
      auto correct = {"top", "left", "right", "bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   } else {
      auto correct = {"top", "right", "left", "bottom"};
      BOOST_CHECK_EQUAL_COLLECTIONS(finishedq.begin(), finishedq.end(),
                                    correct.begin(), correct.end());
   }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace test
} // namespace sparkles
