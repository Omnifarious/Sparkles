#pragma once

#include <functional>
#include <memory>
#if __has_include(<optional>)
#  include <optional>
#elif __has_include(<experimental/optional>)
#  include <experimental/optional>
#  define has_experimental_optional 1
#else
#  error Some form of optional values from the C++17 spec is required.
#endif

namespace sparkles {

/*! \brief Multithreaded multiple writer, one reader queue.
 *
 * Currently uses semaphores and mutexes. Hopefully this will use a lock-free
 * implementation in the future.
 *
 * Having multiple threads dequeueing things from this at the same time will
 * result in undefined behavior.
 *
 * Destroying the queue while someone is trying to read from it or write to it
 * in another thread also results in undefined behavior.
 */
class work_queue {
 public:
   //! A work item is a function-like object with a void (*)(void) signature.
   typedef ::std::function<void ()> work_item_t;
 #ifndef has_experimental_optional
   typedef ::std::optional<work_item_t> possible_work_item_t;
 #else
   typedef ::std::experimental::optional<work_item_t> possible_work_item_t;
 #endif

   work_queue(const work_queue &) = delete;
   work_queue(work_queue &&) = delete;
   const work_queue &operator =(const work_queue &) = delete;
   const work_queue &operator =(work_queue &&) = delete;

   //! Construct a new work_queue
   work_queue();
   //! Destroy the work_queue
   ~work_queue();

   /*! \brief Enqueue a work item, possibly one that should be handled out of
    * band.
    *
    * \param[in] item        The work item to be queued.
    * \param[in] out_of_band This work item should be handled before all the
    *                        regular ones because it's a cancellation of a
    *                        previous work item or something similar.
    */
   void enqueue(work_item_t item, bool out_of_band = false);

   /*! \brief Deqeue a work item, blocking or not as requested.
    *
    * \param[in] block Wait for a work item to be available.
    * \return The work item that was dequed, if any. Will always contain a value
    *         if block is true.
    *
    * This will throw an exception for various kinds of errors. If an exception
    * is thrown, the queue is guaranteed to not be altered (well, unless that
    * exception was thrown because the semaphore underlying the queue was
    * destroyed, in which case, that means the queue itself was destroyed).
    */
   possible_work_item_t dequeue(bool block);

 private:
   struct impl_t;
   struct node_t;
   //! Ugly private thing to make Fast Pimpl work.
   union impl_data {
      long long alignment1;
      void *alignment2;
      char data[192];
   };

   //! Implements the Fast Pimpl idiom from http://www.gotw.ca/gotw/028.htm
   impl_data storage_;

   inline impl_t &impl_();
   inline const impl_t &impl_() const;
   inline node_t *make_new_node(impl_t &imp);
   inline void free_queue(node_t *head);
   inline node_t *remove_from_queue(node_t *&head, node_t *&tail);
   work_item_t real_dequeue(impl_t &impl);
};

} // namespace sparkles
