# Bit Manipulation

## 1. Essential Bitwise Operations

All integers are represented in memory as sequences of bits. Bitwise operators work directly on those bits, making them O(1) and extremely cache-friendly — no branches, no memory indirection.

### The six operators (C++/TypeScript)

| Operator | Symbol | Truth table / behavior |
|---|---|---|
| AND | `a & b` | 1 only if both bits are 1 |
| OR | `a \| b` | 1 if at least one bit is 1 |
| XOR | `a ^ b` | 1 if exactly one bit is 1 (bits differ) |
| NOT | `~a` | Flip all bits (includes sign bit — use with care on signed ints) |
| Left shift | `a << k` | Multiply by 2^k; vacated bits become 0 |
| Right shift | `a >> k` | Divide by 2^k (arithmetic shift for signed, logical for unsigned) |

### Truth table for a single bit position

```
a  b  | a&b  a|b  a^b
0  0  |  0    0    0
0  1  |  0    1    1
1  0  |  0    1    1
1  1  |  1    1    0
```

### Shift arithmetic

```
1 << k  = 2^k           (bit k set, all others 0 — a "mask" for position k)
a << k  = a * 2^k       (left shift k positions)
a >> k  = a / 2^k       (right shift k positions, rounds toward -infinity for signed)
```

---

## 2. Common Tricks

### Check, Set, Clear, Toggle a specific bit

```cpp
int k = 3;  // target bit position (0-indexed from LSB)

// Check bit k — is it 1?
bool isSet = (n >> k) & 1;
// Equivalently:
bool isSet2 = (n & (1 << k)) != 0;

// Set bit k (force to 1)
n |= (1 << k);

// Clear bit k (force to 0)
n &= ~(1 << k);

// Toggle bit k (flip)
n ^= (1 << k);

// Get lowest set bit (isolate rightmost 1-bit)
int lsb = n & (-n);           // works because -n = ~n + 1 in two's complement

// Clear lowest set bit
n &= (n - 1);                 // Brian Kernighan's trick — used for counting set bits

// Check if n is a power of 2
bool isPow2 = n > 0 && (n & (n - 1)) == 0;

// Check if n is odd
bool isOdd = n & 1;

// Swap two numbers without temp variable
a ^= b;  b ^= a;  a ^= b;
```

### Count set bits (popcount)

**Brian Kernighan's algorithm — O(number of set bits):**
```cpp
int countBits(int n) {
    int count = 0;
    while (n) {
        n &= (n - 1);  // clear lowest set bit
        count++;
    }
    return count;
}
```

**Built-in (GCC/Clang):**
```cpp
int count = __builtin_popcount(n);   // 32-bit
int count = __builtin_popcountll(n); // 64-bit
```

**TypeScript:**
```typescript
function countBits(n: number): number {
    let count = 0;
    while (n) { n &= (n - 1); count++; }
    return count;
}
```

### Create an n-bit all-ones mask

```cpp
int mask = (1 << n) - 1;   // bits 0..n-1 all set
// Example: n=4 → 0b1111 = 15
```

---

## 3. XOR Properties

XOR is the most useful bitwise operator for algorithmic problems. Memorize these identities:

```
a ^ a = 0         (a number XORed with itself is zero)
a ^ 0 = a         (a number XORed with zero is itself)
a ^ b = b ^ a     (commutative)
(a ^ b) ^ c = a ^ (b ^ c)  (associative)
```

### Derived tricks

```cpp
// Self-inverse: if a ^ b = c, then c ^ b = a and c ^ a = b
// Useful for "undo" operations without tracking state

// Detect if two numbers have the same sign
bool sameSign = !((a ^ b) < 0);   // MSB (sign bit) is 0 only if both same sign

// Find the missing number in [0..n]: XOR all indices with all values
// Pairs cancel (a^a=0); unpaired index = missing number

// Find the single non-duplicate in an array where every other appears twice:
// XOR all elements — duplicates cancel, leaving the unique one
```

### Why XOR works for "find the odd one out"

```
2 ^ 2 ^ 3 ^ 5 ^ 3 ^ 5 ^ 4
= (2^2) ^ (3^3) ^ (5^5) ^ 4
=   0   ^   0   ^   0   ^ 4
= 4
```

The order doesn't matter because XOR is commutative and associative. Every even-frequency element cancels to 0; the odd-frequency element survives.

---

## 4. Four Solved Problems

---

### Problem 1 — Single Number (LeetCode 136)

**Problem:** Given a non-empty array of integers where every element appears twice except one, find that single element. Must run in O(n) time and O(1) space.

**Key insight:** XOR all elements. All pairs cancel (a^a=0). The lone element remains.

```cpp
class Solution {
public:
    int singleNumber(vector<int>& nums) {
        int result = 0;
        for (int n : nums) result ^= n;
        return result;
    }
};
```

```typescript
function singleNumber(nums: number[]): number {
    return nums.reduce((acc, n) => acc ^ n, 0);
}
```

**Complexity:** Time O(n). Space O(1).

**Variant — Single Number II (LeetCode 137):** Every element appears three times except one. Use bit counting: for each bit position, sum all bits modulo 3. The remainder is the surviving bit.

```cpp
int singleNumber(vector<int>& nums) {
    int result = 0;
    for (int i = 0; i < 32; i++) {
        int bitSum = 0;
        for (int n : nums) bitSum += (n >> i) & 1;
        result |= (bitSum % 3) << i;
    }
    return result;
}
```

---

### Problem 2 — Number of 1 Bits (LeetCode 191)

**Problem:** Given a 32-bit unsigned integer, return the number of set bits (Hamming weight).

**Approach — Brian Kernighan:** Each `n & (n-1)` clears the lowest set bit. Count how many times we can do this before n becomes 0.

```cpp
class Solution {
public:
    int hammingWeight(uint32_t n) {
        int count = 0;
        while (n) {
            n &= (n - 1);  // clear lowest set bit
            count++;
        }
        return count;
    }
};
```

```typescript
function hammingWeight(n: number): number {
    let count = 0;
    // Use unsigned right shift (>>>) to handle JS 32-bit signed ints
    while (n !== 0) {
        n &= (n - 1);
        count++;
    }
    return count;
}
```

**Complexity:** Time O(k) where k = number of set bits. Worst case O(32). Space O(1).

**Alternative — shift and mask:**
```cpp
int hammingWeight(uint32_t n) {
    int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}
```
This is O(32) always. Brian Kernighan's version is faster for sparse bit patterns.

---

### Problem 3 — Reverse Bits (LeetCode 190)

**Problem:** Reverse the bits of a given 32-bit unsigned integer.

**Approach:** Read bits from the LSB end, write them to the MSB end of the result. For each of the 32 bit positions: extract the current LSB of n, OR it into the result after shifting result left, then shift n right.

```cpp
class Solution {
public:
    uint32_t reverseBits(uint32_t n) {
        uint32_t result = 0;
        for (int i = 0; i < 32; i++) {
            result = (result << 1) | (n & 1);  // shift result left, add LSB of n
            n >>= 1;                            // consume LSB of n
        }
        return result;
    }
};
```

```typescript
function reverseBits(n: number): number {
    let result = 0;
    for (let i = 0; i < 32; i++) {
        result = ((result << 1) | (n & 1)) >>> 0;  // >>> 0 keeps it unsigned 32-bit
        n >>>= 1;
    }
    return result >>> 0;
}
```

**Step-by-step for n = 0b00000010100101000001111010011100:**
```
i=0:  result = 0b0 | 0 = 0b0,          n >>= 1
i=1:  result = 0b00 | 0 = 0b00,        n >>= 1
i=2:  result = 0b001,                   n >>= 1
...
```

**Complexity:** Time O(32) = O(1). Space O(1).

**Divide and conquer (for repeated calls — O(1) with lookup table):**
```cpp
uint32_t reverseBits(uint32_t n) {
    n = ((n & 0xFFFF0000) >> 16) | ((n & 0x0000FFFF) << 16); // swap 16-bit halves
    n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) <<  8); // swap 8-bit halves
    n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) <<  4); // swap nibbles
    n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) <<  2); // swap 2-bit groups
    n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) <<  1); // swap adjacent bits
    return n;
}
```

---

### Problem 4 — Missing Number (LeetCode 268)

**Problem:** Given an array containing n distinct numbers in the range [0, n], find the one missing number.

**XOR approach:** XOR all indices 0..n with all values in the array. The index with no matching value is the missing number.

```cpp
class Solution {
public:
    int missingNumber(vector<int>& nums) {
        int n = nums.size();
        int result = n;           // start with n (the extra index)
        for (int i = 0; i < n; i++) {
            result ^= i ^ nums[i];  // XOR index and value — matching ones cancel
        }
        return result;
    }
};
```

```typescript
function missingNumber(nums: number[]): number {
    let result = nums.length;
    for (let i = 0; i < nums.length; i++) {
        result ^= i ^ nums[i];
    }
    return result;
}
```

**Trace for nums = [3, 0, 1] (n=3, missing=2):**
```
result = 3
i=0: result ^= 0 ^ 3  → 3^0^3 = 0
i=1: result ^= 1 ^ 0  → 0^1^0 = 1
i=2: result ^= 2 ^ 1  → 1^2^1 = 2
result = 2  ✓
```

**Complexity:** Time O(n). Space O(1).

**Alternative — Gauss formula:** `result = n*(n+1)/2 - sum(nums)`. Also O(n) time, O(1) space. The XOR approach avoids potential integer overflow for large n.

---

## 5. Bit Manipulation Cheat Sheet

```cpp
// ---- bit inspection ----
(n >> k) & 1          // value of bit k (0 or 1)
n & (1 << k)          // non-zero if bit k is set

// ---- bit mutation ----
n |= (1 << k)         // set bit k
n &= ~(1 << k)        // clear bit k
n ^= (1 << k)         // toggle bit k

// ---- structural ----
n & (n - 1)           // clear lowest set bit
n & (-n)              // isolate lowest set bit
n | (n - 1)           // set all bits below lowest set bit
(n & (n-1)) == 0      // is power of 2?

// ---- arithmetic shortcuts ----
n >> 1                // floor(n / 2)
n << 1                // n * 2
n & 1                 // n % 2 (odd check)
n >> 31               // sign: 0 for non-negative, -1 (all 1s) for negative

// ---- XOR identities ----
a ^ a = 0
a ^ 0 = a
a ^ b ^ a = b         // isolate b if a appears twice
```
