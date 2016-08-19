thenable
========

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
    promise<int> p;

    ThenableFuture<int> f = p.get_future();

    f.then( []( int i ) {
        assert( i == 10 );

        cout << "Hello, World!" << endl;
    }, then_launch::detached );

    thread( [&p] {
        this_thread::sleep_for( 1s );

        p.set_value( 10 );
    } ).join();

    return 0;
}

```

The above example waits for `f` to resolve in a separate thread, while p is set in another thread after one second.

## Dependencies

This project relies on files from my `function_traits` project located here: [function_traits](https://github.com/novacrazy/function_traits).

Clone the repo or download a ZIP of it and add the `'include'` directory to your build system.

For example, to add it to `CMakeLists.txt` you would do this:

```
include_directories(SYSTEM "/path/to/function_traits/include")
```

## API

Coming soon.
