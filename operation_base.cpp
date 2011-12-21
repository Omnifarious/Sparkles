#include "sparkles/operation_base.hpp"

namespace sparkles {

void operation_base::add_dependent(const opbase_ptr_t &dependent)
{
   if (dependent != nullptr) {
      dependents_[dependent.get()] = weak_opbase_ptr_t(dependent);
   }
}

void operation_base::remove_dependent(const operation_base *dependent)
{
   if (dependent != nullptr) {
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

   if (cleanup_dependencies_) {
      ::std::unordered_set<opbase_ptr_t> saved_deps;
      saved_deps.swap(dependencies_);
      for (auto &dependency : saved_deps) {
         dependency->remove_dependent(this);
      }
   } else {
      dependencies_.clear();
   }

   // We can't use the swap trick here because informing a dependent that we've
   // finished may cause other dependents to de-register themselves.
   while (dependents_.size() > 0) {
      opbase_ptr_t dependent;
      {
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
      if (cleanup_dependencies_) {
         const opbase_ptr_t dep(*deppos);
         dependencies_.erase(deppos);
         dep->remove_dependent(this);
      } else {
         dependencies_.erase(deppos);
      }
   }
}

operation_base::~operation_base()
{
   if (cleanup_dependencies_) {
      // As a courtesy, tell all of our dependencies to forget about this object
      // a dependent as it's about to go away.
      for (auto &dependency : dependencies_) {
         dependency->remove_dependent(this);
      }
   }
}

} // namespace sparkles
