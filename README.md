thenable
========

### This project has been postponed and no longer being actively worked on. I would not recommend using it in this state, but feel free to browse the code and steal anything useful.

> "There is a point where we needed to stop and we have clearly passed it but let’s keep going and see what happens."

This is a generic implementation of `then` for C++14, allowing you to chain together futures, promises and so forth easily and efficiently.

## Example:
```C++
#include "thenable.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <assert.h>

using namespace std;
using namespace thenable;

int main() {
    ThenablePromise<int> p, k;

    cout << "Starting asynchronous tasks on thread: " << this_thread::get_id() << endl;

    p.then( [&k]( int i ) {
        assert( i == 10 );

        cout << "Hello, World! from thread: " << this_thread::get_id() << endl;

        k.set_value( 421 );

        return k.get_future();

    } ).then( []( int i ) {
        assert( i = 421 );

        cout << "Hello, again! from thread: " << this_thread::get_id() << endl;

    }, then_launch::detached );

    //Launch a new thread to resolve the original promise.
    thread( [&p] {
        cout << "Resolving promise on thread: " << this_thread::get_id() << endl;

        p.set_value( 10 );
    } ).join();

    //Just to keep the program alive long enough for the promises/futures to propagate.
    this_thread::sleep_for( 10s );

    return 0;
}

```

The output of the above will be something like:
```
Starting asynchronous tasks on thread: 1
Resolving promise on thread: 4
Hello, World! from thread: 2
Hello, again! from thread: 3
```

If `THENABLE_DEFAULT_POLICY` is set to `std::launch::deferred`, then the last two actions will be performed on the same thread.

## Dependencies

This project relies on files from my `function_traits` project located here: [function_traits](https://github.com/novacrazy/function_traits).

Clone the repo or download a ZIP of it and add the `'include'` directory to your build system.

For example, to add it to `CMakeLists.txt` you would do this:

```
include_directories(
    SYSTEM
    "/path/to/function_traits/include"
    "/path/to/thenable/include"
)
```

## API

#### [Click here for Doxygen generated documentation](https://novacrazy.github.io/thenable/html/index.html)
