#include "sparkles/operation_base.hpp"

namespace sparkles {

void operation_base::add_dependent(const opbase_ptr_t &dependent)
{
   if (dependent != nullptr) {
      ::std::lock_guard<decltype(dependents_mutex_)> lock(dependents_mutex_);
      dependents_[dependent.get()] = weak_opbase_ptr_t(dependent);
   }
}

void operation_base::remove_dependent(const operation_base *dependent)
{
   if (dependent != nullptr) {
      ::std::lock_guard<decltype(dependents_mutex_)> lock(dependents_mutex_);
      auto depiter = dependents_.find(const_cast<operation_base *>(dependent));
      if (depiter != dependents_.end()) {
         dependents_.erase(depiter);
      }
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
   // This is both so there is something to pass to dependency_changed, and to
   // guarantee that this object sticks around.
   const opbase_ptr_t me(shared_from_this());
   finished_ = true;

   {
      for (auto &dependency : dependencies_) {
         dependency->remove_dependent(this);
      }
      ::std::unordered_set<opbase_ptr_t> empty;
      empty.swap(dependencies_);
   }

   // We can't use the swap trick here because informing a dependent that we've
   // finished may cause other dependents to de-register themselves.
   while (dependents_.size() > 0) {
      opbase_ptr_t dependent;
      {
         ::std::lock_guard<decltype(dependents_mutex_)> lock(dependents_mutex_);
         auto first = dependents_.begin();
         dependent = first->second.lock();
         dependents_.erase(first);
      }
      if (dependent != nullptr) {
         dependent->dependency_finished(me);
      }
   }
}

void operation_base::remove_dependency(
   ::std::unordered_set<opbase_ptr_t>::iterator &deppos
   )
{
   const opbase_ptr_t me(shared_from_this());
   if (deppos != dependencies_.end()) {
      const opbase_ptr_t dep(*deppos);
      dep->remove_dependent(this);
      dependencies_.erase(deppos);
   }
}

operation_base::~operation_base()
{
   // As a courtesy, tell all of our dependencies to forget that this object is
   // a dependent as it's about to go away.
   for (auto &dependency : dependencies_) {
      dependency->remove_dependent(this);
   }
}

} // namespace sparkles
