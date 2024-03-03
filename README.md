# async++

![Language](https://img.shields.io/badge/Language-C++20-blue)
[![License](https://img.shields.io/badge/License-MIT-blue)](#license)
[![Build & test](https://github.com/petiaccja/asyncpp/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/petiaccja/asyncpp/actions/workflows/build_and_test.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=petiaccja_asyncpp&metric=alert_status)](https://sonarcloud.io/dashboard?id=petiaccja_asyncpp)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=petiaccja_asyncpp&metric=coverage)](https://sonarcloud.io/dashboard?id=petiaccja_asyncpp)

`asyncpp` is a C++20 coroutine library that enables you to write asynchronous and parallel code with a clean syntax.

## Features

**Coroutines**:
- [task](#feature_task)
- [shared_task](#feature_task)
- [generator](#feature_generator)
- [stream](#feature_stream)

**Synchronization primitives**:
- [event](#feature_event)
- [broadcast_event](#feature_event)
- [mutex](#feature_mutex)
- [shared_mutex](#feature_mutex)

**Utilities**:
- [join](#feature_join)
- [sleep_for, sleep_until](#feature_sleep)

**Schedulers**:
- [Introduction](#feature_scheduler)
- [thread_pool](#feature_thread_pool)

**Extra**:
- [Allocator awareness](#feature_allocator)
- [Integration with other coroutine libraries](#feature_integration)
- [Extending asyncpp](#feature_extension)


## Coroutines crash course

If you're new to coroutines, the rest of the documentation may be difficult to understand. This crash course on C++20 coroutines will hopefully give you enough bases to get started using this library.

### The architectural layers of C++ coroutines

In C++, coroutine support is split into three distinct layers:
1. Compiler layer: what `asyncpp` is built on
2. Library layer: `asyncpp` itself
3. Application layer: your application built using `asyncpp`

#### Compiler layer

C++20 introduces three new keywords into the language:
```c++
co_await <awaitable>;
co_yield <value>;
co_return <value>;
```

These keywords are different than the ones you are used to, as their behaviour can be "scripted" by implementing a few interfaces. Imagine as if the compiler exposed an API for `throw` and you had to implement stack unwinding yourself. As you can guess, using the compiler layer directly would be really cumbersome, that's why we have coroutine libraries.

#### Library layer

`asyncpp` (and other coroutine libraries) implement the interfaces of the compiler layer and define higher level primitives such as `task<T>`, `generator<T>`, or `mutex`. These primitives are intuitive, and therefore enable practical applications of coroutines.

A small excerpt of what the library layer code might look like:

```c++
template <class T>
class task {
	awaitable operator co_await() { /* ... */ }
};
```

#### Application layer

You can use the higher level primitives of the coroutine libraries to implement intuitive asynchronous and parallel code:

```c++
// With coroutines:
asyncpp::task<int> add(int a, int b) {
	co_return a + b;
}

// With OS threading:
std::future<int> add(int a, int b) {
	return std::async([a, b]{ return a + b; });
}
```


### How coroutines work: a practical approach

Consider the following code using OS threading:

```c++
void work(std::mutex& mtx) {
	std::cout << "init section" << std::endl;
	mtx.lock();
	std::cout << "critical section" << std::endl;
	mtx.unlock();
}

void my_main(std::mutex& mtx) {
	work(mtx);
	std::cout << "post section" << std::endl;
}
```

When you run `my_main`, the following will happen in order:
- Entering `my_main`
	- Entering `work`
		- Executing *init section*
		- Blocking current thread (assuming mutex not free)
		- ... another thread unlocks the mutex
		- Resuming current thread
		- Executing critical section
	- Exiting `work`
	- Executing *post section*
- Exiting `my_main`

As such, the text printed will in **all cases** be:
```
init section
critical section
post section
```

Let's look at a very similar piece of code implemented using coroutines:

```c++
example::task<void> work(example::mutex& mtx) {
	std::cout << "init section" << std::endl;
	co_await mtx;
	std::cout << "critical section" << std::endl;
	mtx.unlock();
}

example::task<void>void my_main(example::mutex& mtx) {
	example::task<void> tsk = work(mtx);
	std::cout << "post section" << std::endl;
	co_await tsk;
}

```

In this case, executing `my_main` results in the following:

**Thread #1**:
- Entering `my_main`
	- Entering `work` @ function preamble
		- Executing *init section*
		- Suspend coroutine `work` (assuming mutex not free)
	- Return control to `my_main`
	- Executing *post section*
	- Suspend coroutine `my_main` (assuming `work` has not finished yet on another thread)
- Return control to caller of `my_main`

**Thread #2** (some time later...):
- Unlock mutex
- Entering `work` @ after `co_await mtx`
	- Executing *critical section*
	- Returning control to caller and specifying next coroutine to run
- Entering `my_main` @ after `co_await tsk`
- Return control to caller in thread #2

The application now may print (as described above):
```
init section
post section
critical section
```

But could also print, given different thread interleavings:
```
init section
critical section
post section
```

This looks rather complicated, but the key idea is that when you call `work(mtx)`, it does not run immediately in the current thread, like a function, but runs asynchronously on any thread. Consequently, you also can't just access its return value, you must synchronize using `co_await`.

When it comes to `co_await`, it is not a thread synchronization primitive like `std::condition_variable`. `co_await` does not actually block the current thread, it merely suspends the coroutine at a so called *suspension point*. Later, the thread that completes the coroutine that is being `co_await`ed (in this case, `work`), is responsible for continuing the suspended coroutine (in this case, `my_main`). You can view this as cooperative multi-tasking as opposed to the operating system's preemptive multi-tasking. Suspension points are introduced by the `co_await` and `co_yield` statements, though coroutines also have an initial and final suspension point. The latter two are more important for library developers.

### Thinking in coroutines

When approaching coroutines from a procedural programming standpoint, it's logical to think about coroutines as special function that can be suspended mid-execution. I'd rather suggest the other way around: consider functions as special coroutines that have zero suspension points. This is especially useful for C++, where we can implement different types of coroutines via coroutine libraries, because we can regard plain old functions, as well as tasks, generators, streams, etc., as a "subclass" of a general coroutine. 

## Using asyncpp

### <a name="feature_task"></a> Task & shared_task

Tasks are coroutines that can `co_await` other tasks and `co_return` a single value:

```c++
// A compute-heavy asynchronous computation.
task<float> det(float a, float b, float c) {
	co_return std::sqrt(b*b - 4*a*c);
}

// An asynchronous computation that uses the previous one.
task<float> solve(float a, float b, float c) {
	task<float> computation = det(a, b, c); // Launch the async computation.
	const float d = co_await computation; // Wait for the computation to complete.
	co_return (-b + d) / (2 * a); // Once complete, process the results.
}
```

In `asyncpp`, tasks are lazy, meaning that they don't start executing until you `co_await` them. You can also force a task to start executing using `launch`:

```c++
task<void> t1 = work(); // Lazy, does not start.
task<void> t2 = launch(work()); // Eager, does start.
```

Tasks come in two flavours:
1. `task`:
	- Movable
	- Not copyable
	- Can only be `co_await`ed once
2. `shared_task`:
	- Movable
	- Copyable: does not launch multiple coroutines, just gives you multiple handles to the result
	- Can be `co_await`ed any number of times
		- Repeatedly in the same thread
		- Simultaneously from multiple threads: each thread must have its own copy!


### <a name="feature_generator"></a> Generator

Generator can generate multiple values using `co_yield`, this is what sets them apart from tasks that can generate only one result. The generator below generates an infinite number of results:

```c++
generator<int> iota() {
	int value = 0;
	while (true) {
		co_yield value++;
	}
}
```

To access the sequence of generated values, you can use iterators:

```c++
// Prints numbers 1 to 100
generator<int> g = iota();
auto it = g.begin();
for (int index = 0; index < 100; ++index, ++it) {
	std::cout << *it << std::endl;
}
```

You can also take advantage of C++20 ranges:

```c++
// Prints numbers 1 to 100
for (const auto value : iota() | std::views::take(100)) {
	std::cout << value << std::endl;
}
```

Unlike tasks, generators are always synchronous, so they run on the current thread when you advance the iterator, and you don't have to await them.


### <a name="feature_stream"></a> Stream

Streams are a mix of tasks and generators: like generators, they can yield multiple values, but like tasks, you have to await each value. You can think of a stream as a `generator<task<T>>`, but with a simpler interface and a more efficient implementation. Like tasks, streams are also asynchronous, and can be set to run on another thread.

The `iota` function looks the same when implemented as a `stream`:

```c++
stream<int> iota() {
	int value = 0;
	while (true) {
		co_yield value++;
	}
}
```

The difference is in how you access the yielded elements:

```c++
auto s = iota();
while (const auto it = co_await s) {
	std::cout << *it << std::endl;
}
```

You can `co_await` the stream object multiple times, and each time it will give you an "iterator" to the latest yielded value. Before retrieving the value from the iterator, you have to verify if it's valid. An invalid iterator signals the end of the stream.


### <a name="feature_event"></a> Event & broadcast_event

Events allow you to signal the completion of an operation in one task to another task:

```c++
task<void> producer(std::shared_ptr<event<int>> evt) {
	evt->set_value(100);
}

task<void> consumer(std::shared_ptr<event<int>> evt) {
	std::cout << (co_await *evt) << std::endl;
}

const auto event = std::make_shared<event<int>>();
launch(producer(event));
launch(consumer(event));
```

The relationship between `event` and `broadcast_event` is similar to that of `task` and `shared_task`: you can await an `event` only once, and only from one thread at the same time, but you can await a `broadcast_event` multiple times without thread-safety concerns. It's important to note that both `event` and `broadcast_event` are neither movable nor copyable.

While events can be useful on their own, they can also be used to implement higher level primitives, such as the usual `std::promise / std::future` pair. In fact, `task` and `shared_task` are implemented using `event` and `broadcast_event`, respectively.


### <a name="feature_mutex"></a> Mutex & shared_mutex

Mutexes and shared mutexes in asyncpp are virtually equivalent to their standard library counterparts, but they are tailored for coroutine contexts.

The interfaces have been slightly modified to make them suitable for coroutines:

```c++
task<void> lock_mutex(mutex& mtx) {
	co_await mtx; // Locks the mutex.
	mtx.unlock();
}

task<void> lock_shared_mutex(shared_mutex& mtx) {
	// Lock exclusively:
	co_await mtx.exclusive();
	mtx.unlock();
	// Lock shared:
	co_await mtx.shared();
	mtx.unlock_shared();
}
```

`asyncpp` also comes with its own `unique_lock` and `shared_lock` that help you manage mutexes in an RAII fashion:

```c++
task<void> lock_mutex(mutex& mtx) {
	unique_lock lk(co_await mtx); // Locks the mutex.
}

task<void> lock_shared_mutex(shared_mutex& mtx) {
	{
		unique_lock lk(co_await mtx.exclusive());
	}
	{
		shared_lock lk(co_await mtx.shared());
	}
}
```

### <a name="feature_join"></a> Join

To retrieve the result of a coroutine, we must `co_await` it, however, only a coroutine can `co_await` another one. Then how is it possible to wait for a coroutine's completion from a plain old function? For this purpose, `asnyncpp` provides `join`:

```c++
#include <asyncpp/join.hpp>

using namespace asyncpp;

task<int> coroutine() {
	co_return 0;
}

int main() {
	return join(coroutine());
}
```

Join uses OS thread-synchronization primitives to block the current thread until the coroutine is finished. Join can be used for anything that can be `co_await`ed: tasks, streams, events, and even mutexes.


### <a name="feature_sleep"></a> Sleeping

Just like with mutexes, you shouldn't use threading primitives like `std::this_thread::sleep_for` inside coroutines. As a replacement, `asyncpp` provides a coroutine-friendly `sleep_for` and `sleep_until` method:

```c++
task<void> sleepy() {
	co_await sleep_for(20ms);
}
```

Sleeping is implemented by a background thread that manages a priority queue of coroutines that have been put to sleep. The background thread then uses the operating system's sleep functions to wait until the next coroutine has to be awoken. It then awakes that coroutine, and goes back to sleep until the next one. There is not busy loop that wastes CPU power.

### <a name="feature_scheduler"></a> Schedulers

Think about the following code:

```c++
task<measurement> measure() {
	co_return heavy_computation();
}

task<report> analyze() {
	auto t = measurement(); // This line doesn't do any heavy computation because tasks are lazy.
	const auto m = co_await t; // This line does the heavy computation.
	co_return make_report(m);
}
```

This code may be asynchronous, but it does **not** use multiple threads. To achieve that, we are going to need schedulers, for example a `thread_pool`:

```c++
task<measurement> measure() {
	co_return heavy_computation();
}

task<report> analyze(thread_pool& sched) {
	auto t = launch(measurement(), sched); // Execution of the heavy computation will start on the thread pool ASAP.
	const auto m = co_await t; // We suspend until the thread pool finished the heavy computation.
	co_return make_report(m); // We countinue as soon as the results are available.
}
```

`launch` can optionally take a second argument, which is the scheduler that the coroutine will run on. In this case, it is the thread pool, and the coroutine will be assigned to any if its threads.

Let's assume that we did not specify a scheduler for `analyze`, even though `measure` is running on the thread pool. In this case, when `measure` is finished, as the thread pool's thread returns control to the caller of `measure`, that is, `analyze`, it will cause `analyze`'s second part to also run on the thread pool.

However, if you specify a scheduler for both `analyze` and `measure`, they will at all times respect their target schedulers. As such, `analyze`s second part is guaranteed to run on `analyze`'s scheduler, even if `measure` runs on the thread pool.

#### <a name="feature_thread_pool"></a> Thread_pool

The thread pool is currently the only scheduler in `asyncpp`. It's a traditional thread pool with multiple threads that wait and execute coroutines when they become ready for execution. The thread pool uses work stealing to dynamically distribute the workload. When saturated with tasks, the thread pool incurs no synchronization overhead and is extremely fast.


### <a name="feature_allocator"></a> Allocator awareness

`asyncpp`'s coroutines are allocator aware in the same fashion as described in the [proposal for generators](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2502r0.pdf).

This means that you can specify the allocator used for dynamically allocating the coroutine's promise using a template parameter:

```c++
namespace asyncpp::pmr {
template <class T>
using task<T> = asyncpp::task<T, std::pmr::polymorphic_allocator<>>;
}
```

Since there is no other way of passing data to the coroutine's creation than the parameter list, you have to include the allocator there. To differentiate the allocator argument from other, generic arguments, you have to pass it first, prepended by `std::allocator_arg`, like so:

```c++
pmr::task<int> coroutine(std::allocator_arg_t, std::pmr::polymorphic_allocator<>, int a, int b) {
	co_return a + b;
}
```

Just like in the generator proposal, type-erased allocators are supported via setting the allocator template parameter to `void`. The code below will function the exact same way as the previous snippet:

```c++
task<int, /* Alloc = */ void> coroutine(std::allocator_arg_t, std::pmr::polymorphic_allocator<>, int a, int b) {
	co_return a + b;
}
```

The need for specifying allocators come from the fact that coroutines have to do dynamic allocation on creation, as their body's state cannot be placed on the stack, it must be placed on the heap to survive suspension and possible moves to another thread. Allocators help you control how exactly the coroutine's state will be allocated. (Note: compilers may do a Heap Allocation eLimination Optimization (HALO) to avoid allocations altogether, but `asyncpp`'s coroutines use a design for parallelism that is difficult to optimize for the compilers.)


### <a name="feature_integration"></a> Integration with other coroutine libraries

If `asyncpp` does not provide all that you need, but neither does another coroutine library that you considered using, it might seem like a good idea to combine the two.

Combining coroutine libraries is surprisingly easy to do: since they all have to conform to a strict interface provided by the compiler, they are largely interoperable and you can just mix and match primitives. While mixing is not that likely to render your code completely broken, it can introduce some unexpected side effects.

#### Example: using coro::mutex (libcoro) in an asyncpp::task

```c++
asyncpp::task<void> coroutine(coro::mutex& mutex) {
	coro::scoped_lock lk = co_await mutex;
	// Do work inside critical section...
}
```

This code will execute correctly, but with a caveat: if you have bound `coroutine` to an `asyncpp::scheduler`, `libcoro` will not respect that, and `coroutine` will **not** be continued on the bound scheduler, but rather on whichever thread unlocked the mutex.


#### Example: awaiting an `asyncpp::event` from a `coro::task`

```c++
coro::task<void> coroutine(asyncpp::event<void>& evt) {
	co_await evt;
}
```

This code will not actually compile, because `asyncpp`'s primitives can only be awaited by coroutines that implement `asyncpp`'s `resumable_promise` interface, which `libcoro`'s `task` obviously does not. While this limitation, or feature, whichever you think it is, could be relaxed, there are no current plans for it.

If this code actually compiled, it would likely work exactly as expected.


### <a name="feature_extension"></a> Extending asyncpp

As `asyncpp` has its own little ecosystem that consist of a few interfaces, extending asyncpp is as easy as implementing those interfaces, and they will cooperate seemlessly with the rest of `asyncpp`.

#### Adding a new coroutine

To implement a new coroutine like `task`, `generator`, or `stream`, your coroutine's promise has to satisfy the following interfaces:

```c++
#include <asyncpp/promise.hpp>

class my_coroutine {
	struct my_promise 
		: asyncpp::resumable_promise,
		  asyncpp::schedulable_promise,
		  asyncpp::allocator_aware_promise<Alloc>,
	{
		void resume() override; // resumable_promise
		void resume_now() override; // schedulable_promise
	};
	// ...
}
```

While all these interfaces are optional to implement, you may miss out on specific functionality if you don't implement them:
- `resumable_promise` gives you control over how your coroutine is resumed from a suspended state. Typically, you can either resume the coroutine immediately, or enqueue it on a scheduler. This interface is required for your coroutine to be able to `co_await` `asyncpp` primitives.
- `schedulable_promise` enables schedulers to resume a coroutine immediately on the current thread. You have to implement this interface in order to bind your coroutine to an asyncpp scheduler.
- `allocator_aware_promise` makes your promise support allocators. It's more of a mixin than an interface, you don't have to implement anything. It's also completely optional.

#### Adding a new scheduler

```c++
#include <asyncpp/scheduler.hpp>

class my_scheduler : public asyncpp::scheduler {
public:
	void schedule(asyncpp::schedulable_promise& promise) override {
		promise.resume_now();
	}
};
```

This scheduler will just resume the scheduled promise on the current thread immediately.

#### Adding a new synchronization primitive

```c++
#include <asyncpp/promise.hpp>

struct my_primitive {
	asyncpp::resumable_promise* m_enclosing = nullptr;

	bool await_ready(); // Implement as you wish.

	// Make sure you give special treatment to resumable_promises.
	template <std::convertible_to<const asyncpp::resumable_promise&> Promise>
	bool await_suspend(std::coroutine_handle<Promise> enclosing) {
		// Store the promise that's awaiting my_primitive.
		// Later, you should resume it using resumable_promise::resume().
		m_enclosing = &enclosing.promise();
	}

	void await_resume(); // Implement as you wish.

}
```

When you have await this synchronization primitive, you have the opportunity to store the awaiting coroutine's handle. If the awaiting coroutine is an `asyncpp` coroutine, it will have the `resumable_promise` interface implemented, and you should give it special treatment: instead of simply resuming the coroutine handle, you should use the `resumable_promise`'s `resume` method, which will take care of using the proper scheduler.


## Contributing

### Adding features

Check out the section of the documentation about extending `asyncpp`. This should provide make adding a new feature fairly straightforward.

Multi-threaded code is notoriously error-prone, therefore proper tests are expected for every new feature. `asyncpp` comes with a mini framework to exhaustively test multi-threaded code by running all possible orderings. Interleaved tests should be present when applicable. Look at existing tests for reference.

Strive to make code as short and simple as possible, and break it down to pieces as small as possible. This helps both with testing and makes it easier to reason about the code, which is of utmost important with multi-threaded code.

### Reporting bugs

You can always open an issue if you find a bug. Please provide as much detail as you can, including source code to reproduce, if possible. Multi-threaded bugs are difficult to reproduce so all information available will likely be necessary.

## Acknowledgment

I've used [libcoro](https://github.com/jbaldwin/libcoro) as well as [cppcoro](https://github.com/lewissbaker/cppcoro) for inspiration.

## License

`asyncpp` is distributed under the MIT license, therefore can be used in commercial and non-commercial projects alike with very few restrictions.




