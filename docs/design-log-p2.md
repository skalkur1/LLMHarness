# Design Log — Project 2

## Growth factor and amortized cost

### Growth strategy

`Conversation` doubles its capacity whenever it is full: 0 → 1 → 2 → 4 → 8 → …
(`grow()` in `src/conversation.cpp`). A reallocation allocates the new array,
moves the existing messages over, and frees the old one.

### Amortized cost of append

Every append writes one message, so n appends make n writes. The only extra
work is moving messages during a reallocation. A reallocation happens only
when the array is full, and the array is only ever full at sizes 1, 2, 4, …, 2^k,
where 2^k ≤ n. Growing at size s moves s messages, so the total number of
moves is

1 + 2 + 4 + … + 2^k = 2^(k+1) − 1 < 2 · 2^k ≤ 2n.

Total work for n appends is fewer than n + 2n = 3n steps. The average per
append is under 3, which is constant, so append is O(1).

### Trade-offs

Capacity stays below 2n, so at most half the array is unused. A factor of 1.5
would waste less memory, but 2 keeps the analysis simplest, and conversations
are short. `GrowthDoublesAndPreservesContents` checks the capacity after each
of 1000 appends.

## Rule of Five evidence

### Ownership rule

Each buffer has exactly one owner, and `data_ == nullptr` exactly when
`capacity_ == 0`. 

### The five operations

- **Destructor:** `delete[] data_`. On an empty or moved-from object that is
  `delete[] nullptr`, which does nothing, so no special case is needed.
- **Copy constructor:** allocates a *new* array and copies each message into
  it. If a string copy throws partway through, the new array is freed before
  the error is passed on, so nothing leaks.
- **Copy assignment:** copies into a temporary first, then swaps with it. If
  the copy throws, `*this` has not been touched. After the swap, the
  temporary's destructor frees our old buffer.
- **Move constructor:** takes the pointer, size and capacity, then sets the
  source to `nullptr`, 0, 0. The source's destructor then has nothing to
  free, so there is no double free. 
- **Move assignment:** frees our own buffer, takes the source's, and empties
  the source. Self-move returns early instead of freeing the buffer it is about
  to take.

### Testing

`CopyIsDeep` asserts that a copy's `begin()` differs from the original's, and
that changing one does not change the other. `MoveStealsBuffer` asserts that
the moved-to object's `begin()` is the old pointer, the source is
`nullptr`/0/0, and a moved-from object can be reused. `CopySelfAssignment`
covers `a = a`. The whole suite runs under AddressSanitizer and
UndefinedBehaviorSanitizer with no errors or leaks reported, so there are no
double frees.

## Sentinel scanner: bounded pending_

### How feed works

Let L be the sentinel length (20 for `<|end_conversation|>`). Each `feed`
searches `buf = pending_ + chunk` and ends in one of two ways:

1. **The sentinel is found.** The text before it is returned and `pending_` is
   cleared, so its size is 0.
2. **It is not found.** `pending_` is set to the longest ending of `buf` that
   is also a beginning of the sentinel, and everything before that is returned.

### Why pending_ stays under L

In the second case, the loop that finds the ending starts at a length of at
most L − 1 and only counts down, so `pending_` never holds more than L − 1
characters. Every call ends in one of these two cases.

### Why emitting the rest is safe

For a sentinel to finish in a later chunk, it must start in `buf` somewhere
that everything from there to the end of `buf` is a partial sentinel, that is,
an ending of `buf` that is a beginning of the sentinel. Shorter endings of that
kind lie inside the longest one, so holding back the longest one holds back
every possible start. Everything before it can never become part of the
sentinel.

### Memory use and testing

The scanner never holds more than one chunk plus L − 1 characters, not the
whole reply. `ScannerBoundedMemory` feeds 4 MB of near-sentinels one byte at a
time and asserts `pending_size() ≤ L − 1` after every byte.

## What I would change differently

`Conversation` uses `new Message[capacity]`, which constructs a `Message` in
every slot, including the unused half after a doubling. That is why the spec
requires a default constructor. I would allocate raw memory instead and use
placement `new` to construct messages only when they are appended. Then empty
slots cost nothing, and `Message` would not need a default constructor.
