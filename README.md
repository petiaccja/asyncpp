# async++

![Language](https://img.shields.io/badge/Language-C++20-blue)
[![License](https://img.shields.io/badge/License-MIT-blue)](#license)
[![Build & test](https://github.com/petiaccja/asyncpp/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/petiaccja/asyncpp/actions/workflows/build_and_test.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=petiaccja_asyncpp&metric=alert_status)](https://sonarcloud.io/dashboard?id=petiaccja_asyncpp)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=petiaccja_asyncpp&metric=coverage)](https://sonarcloud.io/dashboard?id=petiaccja_asyncpp)

async++ is a C++20 coroutine library that provides primitives to write async and parallel code.


## Usage

In this section:
- [Coroutine primitives](#coroutine_primitives)
- [Synchronization primitives](#sync_primitives)
- [Schedulers](#usage_schedulers)
- [Schedulers](#sync_join)
- [Interaction with other coroutine libraries](#extending_asyncpp)

### <a name="coroutine_primitives"></a>Coroutine primitives

asyncpp implements the following coroutine primitives:
- coroutine primitives:
	- [task\<T\>](#usage_task)
	- [shared_task\<T\>](#usage_shared_task)
	- [generator\<T\>](#usage_generator)
	- [stream\<T\>](#usage_stream)


#### <a name="usage_task"></a>Using `task<T>`

`task<T>` works similarly to `std::future<T>`:
- It returns one value
- Cannot be copied
- Can only be awaited once

Defining a coroutine:
```c++
task<int> my_coro() {
	co_return 0;
}
```

Retrieving the result:
```c++
auto future_value = my_coro();
int value = co_await future_value;
```


#### <a name="usage_shared_task"></a>Using `shared_task<T>`


`task<T>` works similarly to `std::shared_future<T>`:
- It returns one value
- Can be copied
- Can be awaited any number of times from the same or different threads

The interface is the same as `task<T>`.


#### <a name="usage_generator"></a>Using `generator<T>`

Generators allow you to write a coroutine that generates a sequence of values:
```c++
generator<int> my_sequence(int count) {
	for (int i=0; i<count; ++i) {
		co_yield i;
	}
}
```

Retrieving the values:
```c++
auto sequence = my_sequence(5);
for (int value : sequence) {
	f(value);
}
```


#### <a name="usage_stream"></a>Using `stream<T>`

Streams work similarly to generators, but they may run asynchronously so you have to await the results.

Defining a stream:
```c++
stream<int> my_sequence(int count) {
	for (int i=0; i<count; ++i) {
		co_yield i;
	}
}
```

Retrieving the values:
```c++
auto sequence = my_sequence(5);
while (auto value = co_await sequence) {
	f(*value);
}
```


### <a name="sync_primitives"></a>Synchronization primitives

asyncpp implements the following synchronization primitives:
- synchronization primitives:
	- [mutex](#usage_mutex)
	- [shared_mutex](#usage_shared_mutex)
	- [unique_lock](#usage_unique_lock)
	- [shared_lock](#usage_shared_lock)


#### <a name="usage_mutex"></a>Using `mutex`

Mutexes have a similar interface to `std::mutex`, except they don't have a `lock` method. Instead, you have to `co_await` them:

```c++
mutex mtx;
const bool locked = mtx.try_lock();
if (!locked) {
	co_await mtx; // Same as co_await mtx.unique()
}
mtx.unlock();
```


#### <a name="usage_shared_mutex"></a>Using `shared_mutex`

Mutexes have a similar interface to `std::shared_mutex`, except they don't have a `lock` method. Instead, you have to `co_await` them:

```c++
mutex mtx;
const bool locked = mtx.try_lock_shared();
if (!locked) {
	co_await mtx.shared();
}
mtx.unlock_shared();
```


#### <a name="usage_unique_lock"></a>Using `unique_lock`

Similar to `std::unique_lock`, but adapter to dealing with coroutines.

```c++
mutex mtx;
unique_lock lk(mtx); // Does NOT lock the mutex
unique_lock lk(co_await mtx); // Does lock the mutex
lk.unlock(); // Unlocks the mutex
lk.try_lock(); // Tries to lock the mutex
co_await lk; // Lock the mutex
```

#### <a name="usage_shared_lock"></a>Using `shared_lock`

Works similarly to `unique_lock`, but locks the mutex in shared mode.


### <a name="usage_schedulers"></a>Using schedulers

Coroutines (except generator) can be made to run by a scheduler using `bind`:

```c++
thread_pool sched;
task<int> my_task = my_coro();
bind(my_task, sched);
```

When a coroutine is bound to a scheduler, it will always run on that scheduler, no matter if it's resumed from another scheduler. This allows you to create schedulers whose threads have a specific affinity or priority, and you can be sure your tasks run only on those threads. A task that is not bound will run synchronously.


Some coroutines are only started when you `co_await` them, however, other can be launched asynchronously before retrieving the results. This is done using `launch`:

```c++
task<int> my_task = my_coro();
// Launching on whatever scheduler it's bound to:
launch(my_task);
// Launching on a specific scheduler:
thread_pool sched;
launch(my_task, sched);
```

Launching coroutines before `co_await`-ing them is useful to create actual parallelism. This way, you can make callees execute on a thread pool in parallel to each other and the caller, and only then get their results. Simply using `co_await` will respect the bound scheduler of the callee, but it cannot introduce parallelism as there is only ever on task you can await.

There is a shorthand forwarding version of both functions:

```c++
task<int> my_task = bind(my_coro(), sched);
task<int> my_task = launch(my_coro(), sched);
```


### <a name="sync_join"></a>Synchronizing coroutines and functions

Normally, coroutines can only be `co_await`-ed. Since you cannot `co_await` in regular functions, you have to use `join` to retrieve the results of a coroutine:

```c++
task<int> my_task = launch(my_coro(), sched);
int result = join(my_task);
```

Note that this can also be used for any other primitives:

```c++
mutex mtx;
join(mtx); // Acquires the lock
mtx.unlock();
```

In this case, `mutex` works just like `std::mutex`, but you lose all the advantages of coroutines.


### <a name="extending_asyncpp"></a>Interfacing with other coroutine libraries

#### Existing libraries

asyncpp's coroutines can `co_await` other coroutines, however, the scheduling will be partially taken over by the other library, meaning asyncpp's coroutines won't anymore respect their bound schedulers.

Other coroutines cannot currently co_await asyncpp's coroutines. This is because asyncpp coroutines expect awaiting coroutines to have a `resumable_promise`. This restriction could be potentially lifted, however, other coroutines would be treated by asyncpp as having no bound scheduler and run synchronously.

#### Building on top of asyncpp

All asyncpp coroutines have a promise type that derives from `resumable_promise`. This way, whenever a coroutine is resumed, the exact way to resume it is delegated to the coroutine's promise rather than executed by the caller. This allows coroutines to retain their agency over when, where and how they run, and stay on the schedulers they have been bound to.

To extend asyncpp, all you need to do is make your promises implement `resumable_promise`. If you want your coroutines to be schedulable, you have to implement `schedulable_promise`. Such coroutines should seamlessly fit into asyncpp.


## License

asyncpp is distributed under the MIT license, therefore can be used in commercial and non-commercial projects alike with very few restrictions.




