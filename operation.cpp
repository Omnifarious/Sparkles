#include "sparkles/operation.hpp"

namespace sparkles {

void operation_base::add_dependent(const opbase_ptr_t &dependent)
{
   if (dependent != nullptr) {
      dependents_[dependent.get()] = weak_opbase_ptr_t(dependent);
   }
}

void operation_base::remove_dependent(const opbase_ptr_t &dependent)
{
   if (dependent != nullptr) {
      dependents_.erase(dependent.get());
   }
}

void operation_base::dependency_finished(const opbase_ptr_t &dependency)
{
   if (dependencies_.find(dependency) == dependencies_.end()) {
      throw bad_dependency("Unknown dependency finished!");
   } else {
      i_dependency_finished(dependency);
   }
}

void operation_base::set_finished()
{
   // This is both so there is something to pass to dependency_changed, and so
   // this object is guaranteed to stick around.
   const opbase_ptr_t me(shared_from_this());
   finished_ = true;

   for (auto &i: dependents_) {
      auto locked_i = i.second.lock();
      if (locked_i != nullptr) {
         locked_i->dependency_finished(me);
      }
   }
}

void operation_base::cancel_dependency(const opbase_ptr_t &dependency)
{
   const opbase_ptr_t me(shared_from_this());
   dependency->remove_dependent(me);
   if (dependencies_.erase(dependency) <= 0) {
      throw bad_dependency("Attempt to cancel dependency I don't have.");
   }
}

} // namespace sparkles
