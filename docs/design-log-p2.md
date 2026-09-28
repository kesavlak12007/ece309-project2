# Design Log — Project 2



## Growth factor and amortized cost

For `Conversation`, I implemented a growable array of `Message` objects using three pieces of state: `data_`, `size_`, and `capacity_`. `data_` points to the dynamically allocated array in the heap, `size_` keeps track of how many actual messages are currently stored, and `capacity_` keeps track of how many message slots are available before another allocation is required.

I chose a doubling growth strategy. An empty `Conversation` starts with `capacity_ == 0`. On the first append, the capacity becomes 1. Whenever `size_ == capacity_`, a new array is allocated with twice the old capacity. The existing `Message` objects are moved into the new array, the old array is deleted, and `data_` is redirected to the new allocation. Therefore the capacities grow as

`1 -> 2 -> 4 -> 8 -> 16 -> ...`

The important point is that a reallocation does not occur on every append. Most appends simply place the new message into the next available slot, which is constant-time work.

If there are `n` total appends, the total number of elements moved during all reallocations is bounded by the geometric series

`1 + 2 + 4 + ... < 2n`.

Therefore, even though one append may occasionally require `O(n)` work when the array grows, the total growth work across `n` appends is only `O(n)`. Dividing that total cost across the `n` append operations gives an amortized cost of `O(1)` per append.

## Rule of Five evidence

The Rule of Five is necessary because `Conversation` directly owns a raw dynamically allocated `Message` array.

The destructor releases that owned array using `delete[]`.

The copy constructor performs a deep copy. A new array is allocated, and each active `Message` from the original `Conversation` is copied into the new buffer. As a result, the copied conversation contains the same messages but has a different `data_` pointer.

Copy assignment has the extra problem that the destination may already own memory. My implementation first allocates and fills a new buffer. Only after that copy succeeds does it delete the destination's old array and replace its internal state. This avoids destroying the previous contents before the replacement is safely available.

The move constructor does not copy individual messages. Instead, it steals the source object's `data_` pointer along with its `size_` and `capacity_`. The source is then reset to `nullptr`, `0`, and `0`, leaving it valid but empty.

Move assignment works similarly, except the destination may already own an array. It first releases its own array and then steals the source's allocation. The source is again reset to an empty state.

These behaviors were tested by checking that copied conversations have different backing-array addresses, while moved conversations retain the exact original backing-array address.

Copy assignment also checks for self-assignment before performing any allocation or deletion.

## Sentinel scanner: bounded pending_ proof

`SentinelScanner` handles the case where the stop sentinel `<|end_conversation|>` may be split across arbitrary streamed chunks.

For each call to `feed()`, I combine

`pending_ + chunk`

and search that combined text for the complete sentinel.

If the sentinel is found, everything before it is returned as safe output, the sentinel itself is suppressed, and `sentinel_found` is set to `true`.

If the sentinel is not found, the scanner keeps only the final `sentinel_.size() - 1` characters in `pending_` and releases everything before them.

Let the sentinel length be `m`. A sentinel that begins in the current data but finishes in a future chunk can start at most `m - 1` characters before the chunk boundary. Therefore any character farther back than that cannot possibly participate in a future cross-boundary sentinel and is safe to emit.

Thus, after every unsuccessful `feed()` call,

`pending_.size() <= m - 1`.

The amount of withheld memory is therefore bounded independently of the total stream length. This was also stress-tested using a 4 MB stream delivered one byte at a time.

When `flush()` is called, the stream has ended. No future chunk can complete a partial sentinel, so all remaining pending text becomes safe to release.

## What I would change differently

If I redesigned one part of the project, I would separate the array-growth logic from `append()` into a private helper function.

Right now, `append()` is responsible for detecting full capacity, allocating the larger array, moving existing messages, deleting the old allocation, updating the pointer and capacity, and finally inserting the new message.

The logic is correct, but separating reallocation into a helper would make the ownership transition easier to isolate, test, and reason about. In a project where the main difficulty is careful memory ownership rather than the amount of code, making those transitions as explicit as possible would improve readability and reduce the chance of subtle memory-management mistakes.