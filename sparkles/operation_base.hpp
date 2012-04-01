#pragma once

#include <sparkles/errors.hpp>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace sparkles {

/*! \brief The base class for all operations that have dependency relationships
 * with other operations.
 *
 * This implements most of the dependency tracking that's used by the derived
 * classes. Inadvertant dependency cycles are prevented by never allowing new
 * dependencies to be added after the class is constructed. They may be removed
 * however.
 *
 * It's also useful for when you would like to refer to an operation and you
 * don't actually care about its return type. The most common case for this, of
 * course, is inside the Sparkles library itself.
 *
 * The most important function for clients of this class is
 * i_dependency_finished(const opbase_ptr_t &).
 */
class operation_base : public ::std::enable_shared_from_this<operation_base>
{
 public:
   typedef ::std::shared_ptr<operation_base> opbase_ptr_t;

   //! Can't be copy constructed.
   operation_base(const operation_base &) = delete;
   //! Can't be copy assigned.
   operation_base &operator =(const operation_base &) = delete;

   //! Can't be move constructed
   operation_base(operation_base &&other) = delete;
   //! Can't be move assigned.
   operation_base &operator =(operation_base &&other) = delete;

   //! Destroy and remove this from the list of dependents for its dependencies.
   virtual ~operation_base();

   //! Is this operation completed?
   bool finished() const { return finished_; }

   /*! \brief Register given object as a dependent with all its dependencies.
    *
    * This method should be harmless for anybody to call at any time.  You would
    * think the constructor should call this method, but there needs to be a
    * valid shared_ptr to this object before it can be registered as a
    * dependent.
    *
    * This means the constructing static method should call it instead.  And if
    * you don't have one of those, then you'll have to call it by hand once you
    * have a proper shared_ptr.
    */
   static void register_as_dependent(const opbase_ptr_t &op) {
      op->for_each_dependency([op](const opbase_ptr_t &dep) {
            dep->add_dependent(op);
         });
   }

 protected:
   /*! \brief Construct from a batch of dependencies.
    *
    * These dependencies can only be specified at construction time. The list
    * can be subtracted from, but not added to during the lifetime of the
    * object. This makes it require significant effort to construct cycles of
    * dependencies.
    */
   template <class InputIterator>
   operation_base(InputIterator begin,
                  const InputIterator &end)
        : finished_(false), multithreaded_dependencies_(false),
          dependencies_(begin, end)
   {
   }

   //! Set this operation as being finished.
   void set_finished();

   /*! \brief Set whether or not any of my dependencies may live in another
    * thread.
    *
    * The only thing that will affect depencies that's not under the control of
    * the thread this operation is in is the destructor.  It's almost certainly
    * called from the thread this operation is in, and if dependencies may live
    * in another thread, it's important that the destructor not do anything to
    * modify them (like remove this operation from their list of dependents).
    */

   bool set_mulithreaded_dependencies(bool newval) {
      const bool curval = multithreaded_dependencies_;
      multithreaded_dependencies_ = newval;
      return curval;
   }

   //! Execute a function repeatedly with a shared_ptr to each dependency.
   template <class UnaryFunction>
   UnaryFunction for_each_dependency(UnaryFunction f) const {
      return ::std::for_each(dependencies_.begin(),
                             dependencies_.end(),
                             f);
   }

   /*! \brief I no longer depend on this operation.
    *
    * \param[in] dependency The operation you no longer depend on.
    *
    * \throws bad_dependency if this operation does not currently depend on
    * dependency.
    *
    * Removes an operation from the list of operations this operation depends
    * on.  This is safe to do after the operation has been created (even though
    * adding isn't) because deleting edges cannot be used to create a cycle,
    * wheres adding them can.
    *
    * If you've removed the last dependency, realize that there will be no
    * trigger for finishing your operation and you may want to consider
    * finishing it yourself at that point.
    */
   void remove_dependency(const opbase_ptr_t &dependency) {
      auto deppos = dependencies_.find(dependency);
      if (deppos == dependencies_.end()) {
         throw bad_dependency("Tried to remove a dependency I didn't have.");
      } else {
         remove_dependency(deppos);
      }
   }

   //! How many dependencies are there?
   ::std::unordered_set<opbase_ptr_t>::size_type num_dependencies() {
      return dependencies_.size();
   }

   /*! \brief A dependency has gone from not finished to finished.
    *
    * This is THE function to override. This will allow you to determine that
    * you have all the information available to carry out your computation and
    * tell \b your dependents that you've finished.
    *
    * It's also polite to tell all the operations you're depending on that you
    * no longer depend on them once you consider your computation finished.
    */
   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;

 private:
   typedef ::std::weak_ptr<operation_base> weak_opbase_ptr_t;

   bool finished_;
   bool multithreaded_dependencies_;
   ::std::unordered_set<opbase_ptr_t> dependencies_;
   ::std::unordered_map<operation_base *, weak_opbase_ptr_t> dependents_;

   void dependency_finished(const opbase_ptr_t &dependency);

   void remove_dependency(::std::unordered_set<opbase_ptr_t>::iterator &deppos);
   void add_dependent(const opbase_ptr_t &dependent);
   void remove_dependent(const operation_base *dependent);
};

} // namespace teleotask
