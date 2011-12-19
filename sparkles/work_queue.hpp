#pragma once

#include <functional>
#include <memory>

namespace sparkles {

//! Multithreaded multiple writer, one reader queue.
//
// Currently uses semaphores and mutexes. Hopefully this will use a lock-free
// implementation in the future.
//
// Having multiple threads dequeueing things from this at the same time will
// result in undefined behavior.
//
// Destroying the queue while someone is trying to read from it or write to it
// in another thread also results in undefined behavior.
class work_queue {
 public:
   typedef ::std::function<void ()> work_item_t;

   work_queue(const work_queue &) = delete;
   work_queue(work_queue &&) = delete;
   const work_queue &operator =(const work_queue &) = delete;
   const work_queue &operator =(work_queue &&) = delete;

   work_queue();
   ~work_queue();

   //! Enqueue a work item, possibly one that should be handled out of band.
   //
   // \param[in] item        The work item to be queued.
   // \param[in] out_of_band This work item should be handled before all the
   //                        regular ones because it's a cancellation of a
   //                        previous work item or something similar.
   void enqueue(work_item_t item, bool out_of_band = false);

   //! Deqeue a work item, will block of none available.
   //
   // \return The work item that was dequed.
   //
   // This will throw an exception for various kinds of errors. If an exception
   // is thrown, the queue is guaranteed to not be altered (well, unless that
   // exception was thrown because the semaphore underlying the queue was
   // destroyed, in which case, that means the queue itself was destroyed).
   work_item_t dequeue();

   //! Deqeue a work item if there are any in the queue.
   //
   // \param[out] dest Where to put the dequeued work item if one was dequeued.
   // \return true if an item was dequeued, false if not.
   bool try_dequeue(work_item_t &dest);

 private:
   class impl_t;
   class node_t;
   union impl_data {
      long long alignment;
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
