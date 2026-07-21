#pragma once

#include "input.h"
#include "signal.h"
#include "signalresult.h"

#include <btl/shared.h>

#include <initializer_list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace bq::signal
{
    /** @brief A vector that several places can hold and mutate, and that
     * exports its contents as a signal.
     *
     * The state side of a changing list: forEach() consumes signal() and turns
     * it into an array of identified elements. Copies of a vector share one
     * set of contents, the way a `btl::shared` does, so handing a copy to a
     * widget and keeping one to mutate is the intended use.
     *
     * Contents are reached only through a scoped handle. read() takes a shared
     * lock and write() an exclusive one, so any number of readers may run
     * concurrently with each other and none of them with a writer.
     *
     * **The write scope is the unit of emission.** The signal is given the new
     * contents when a write handle is destroyed, not when an individual
     * mutation happens, so a hundred `push_back`s under one handle produce one
     * emission carrying the final state. A write scope that mutates nothing
     * emits too: what publishes is the scope, not the mutation.
     *
     * The contents and the signal are coherent. Publishing happens under the
     * same exclusive lock that the mutations happened under, so a read() that
     * begins after a write scope ends never observes contents the signal has
     * not already been given. What remains is the ordinary lag of a
     * change-driven graph: a SignalContext sees the new contents at its next
     * update pass, exactly as it would for a makeInput() written to from
     * another thread.
     *
     * A handle holds a lock for its lifetime, and the locks are neither
     * recursive nor upgradeable. Opening a second scope of any kind on a
     * thread that already holds one deadlocks, and so does calling anything
     * from inside a scope that opens one — do not hold a handle across a call
     * into code that might.
     *
     * The vector invokes no callback of its own, so the only caller-written
     * code that runs under the lock is `T`'s copy constructor and destructor,
     * from publishing. The only other lock taken under the write lock is the
     * input's, and nothing under *that* lock reaches back here: the input's
     * handle never leaves this vector, so no signal can be pushed into it and
     * no graph code can run while it is held.
     */
    template <typename T>
    class SharedVector
    {
    private:
        struct Control;

    public:
        /** @brief Scoped read access.
         *
         * Holds a shared lock for its lifetime and keeps the contents alive
         * even if every SharedVector naming them is destroyed meanwhile.
         */
        class ReadHandle
        {
        public:
            ReadHandle(ReadHandle const&) = delete;
            ReadHandle(ReadHandle&&) noexcept = default;

            /** @brief A scope is not re-seatable. */
            ReadHandle& operator=(ReadHandle const&) = delete;

            /** @overload */
            ReadHandle& operator=(ReadHandle&&) = delete;

            std::vector<T> const& operator*() const
            {
                return control_->data;
            }

            std::vector<T> const* operator->() const
            {
                return &control_->data;
            }

        private:
            friend class SharedVector;

            explicit ReadHandle(btl::shared<Control> control) :
                control_(std::move(control)),
                lock_(control_->mutex)
            {
            }

            btl::shared<Control> control_;
            std::shared_lock<std::shared_mutex> lock_;
        };

        /** @brief Scoped write access.
         *
         * Holds an exclusive lock for its lifetime and gives the vector out by
         * reference, so every mutating operation a `std::vector` has is
         * available. Destroying the handle publishes the contents to the
         * signal.
         *
         * Publishing copies the contents. If that copy throws — an allocation
         * failure, or a throwing element copy — the exception does not escape,
         * because the publication is a destructor: the mutations stand and the
         * signal keeps the contents it was last given. The next write scope
         * publishes the current state, and if there is no next write scope the
         * signal stays behind the contents for good, with nothing said about
         * it.
         */
        class WriteHandle
        {
        public:
            WriteHandle(WriteHandle const&) = delete;
            WriteHandle(WriteHandle&&) noexcept = default;

            /** @brief A scope is not re-seatable: assigning over a handle
             * would end its scope without saying so. */
            WriteHandle& operator=(WriteHandle const&) = delete;

            /** @overload */
            WriteHandle& operator=(WriteHandle&&) = delete;

            ~WriteHandle()
            {
                publish();
            }

            std::vector<T>& operator*()
            {
                return control_->data;
            }

            std::vector<T>* operator->()
            {
                return &control_->data;
            }

        private:
            friend class SharedVector;

            explicit WriteHandle(btl::shared<Control> control) :
                control_(std::move(control)),
                lock_(control_->mutex)
            {
            }

            void publish() noexcept
            {
                // Only the handle holding the lock publishes: a moved-from one
                // gave the scope away and must not end it too.
                if (!lock_.owns_lock())
                    return;

                try
                {
                    control_->input.handle.set(control_->data);
                }
                catch (...)
                {
                }
            }

            btl::shared<Control> control_;
            std::unique_lock<std::shared_mutex> lock_;
        };

        /** @brief Constructs an empty vector. */
        SharedVector() :
            SharedVector(std::vector<T>())
        {
        }

        /** @brief Constructs a vector holding `initial`. */
        explicit SharedVector(std::vector<T> initial) :
            control_(std::make_shared<Control>(std::move(initial)))
        {
        }

        /** @overload */
        SharedVector(std::initializer_list<T> initial) :
            SharedVector(std::vector<T>(initial))
        {
        }

        /** @brief Copies share one set of contents.
         *
         * There is no move: copying is one atomic increment, and a moved-from
         * vector would be a null state that every other method would have to
         * guard against for nothing. A vector is therefore always usable.
         */
        SharedVector(SharedVector const&) = default;

        /** @overload */
        SharedVector& operator=(SharedVector const&) = default;

        /** @brief Opens a read scope over the contents. */
        ReadHandle read() const
        {
            return ReadHandle(control_);
        }

        /** @brief Opens a write scope over the contents.
         *
         * Ending the scope publishes the contents to the signal, whether or
         * not anything was mutated.
         */
        WriteHandle write()
        {
            return WriteHandle(control_);
        }

        /** @brief The contents, as of the last write scope to end.
         *
         * The signal every copy of this vector exports is one signal, so a
         * SignalContext holding two of them keeps one set of state for both.
         *
         * It does not keep the contents writable: it outlives the last vector
         * naming them and then carries the last contents published, forever.
         */
        AnySignal<std::vector<T>> signal() const
        {
            return control_->sig;
        }

    private:
        struct Control
        {
            using InputType = Input<
                SignalResult<std::vector<T>>,
                SignalResult<std::vector<T>>>;

            explicit Control(std::vector<T> initial) :
                data(std::move(initial)),
                input(makeInput(data)),
                sig(input.signal)
            {
            }

            std::shared_mutex mutex;
            std::vector<T> data;
            InputType input;
            AnySignal<std::vector<T>> sig;
        };

        btl::shared<Control> control_;
    };
} // namespace bq::signal
