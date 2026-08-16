# Hashing

## When to Use

Hash maps/sets are the first tool to reach for when you need:
- O(1) lookup, insert, or delete
- Counting frequencies
- Detecting duplicates
- Two-sum style "have I seen the complement?"
- Grouping items by a computed key

---

## Core Patterns

### Frequency Count
```cpp
// Count character frequency
unordered_map<char, int> freq;
for (char c : s) freq[c]++;

// Most common element
int maxFreq = 0;
char result;
for (auto& [ch, cnt] : freq) {
    if (cnt > maxFreq) { maxFreq = cnt; result = ch; }
}
```

### Two Sum Pattern — "Have I seen the complement?"
```cpp
// Two Sum: return indices of two numbers that add to target
vector<int> twoSum(vector<int>& nums, int target) {
    unordered_map<int, int> seen;  // value → index
    for (int i = 0; i < nums.size(); i++) {
        int complement = target - nums[i];
        if (seen.count(complement)) {
            return {seen[complement], i};
        }
        seen[nums[i]] = i;
    }
    return {};
}
// Time: O(n) | Space: O(n)
```

### Grouping by Key
```cpp
// Group Anagrams: group strings that are anagrams of each other
vector<vector<string>> groupAnagrams(vector<string>& strs) {
    unordered_map<string, vector<string>> groups;
    for (const string& s : strs) {
        string key = s;
        sort(key.begin(), key.end());  // sorted string as key
        groups[key].push_back(s);
    }
    vector<vector<string>> result;
    for (auto& [key, group] : groups) result.push_back(group);
    return result;
}
// Time: O(n * k log k) where k = max string length | Space: O(n)
```

### Detect Duplicate
```cpp
bool containsDuplicate(vector<int>& nums) {
    unordered_set<int> seen;
    for (int n : nums) {
        if (seen.count(n)) return true;
        seen.insert(n);
    }
    return false;
}
// Time: O(n) | Space: O(n)
```

---

## Problems

### Longest Consecutive Sequence
Find the length of the longest consecutive sequence (e.g. [100,4,200,1,3,2] → 4 for [1,2,3,4]).

**Key insight**: only start counting from numbers where `n-1` is NOT in the set. This ensures each sequence is counted once.

```cpp
int longestConsecutive(vector<int>& nums) {
    unordered_set<int> numSet(nums.begin(), nums.end());
    int longest = 0;
    for (int n : numSet) {
        if (!numSet.count(n - 1)) {  // start of a sequence
            int len = 1;
            while (numSet.count(n + len)) len++;
            longest = max(longest, len);
        }
    }
    return longest;
}
// Time: O(n) | Space: O(n)
```

### Valid Anagram
```cpp
bool isAnagram(string s, string t) {
    if (s.size() != t.size()) return false;
    unordered_map<char, int> count;
    for (char c : s) count[c]++;
    for (char c : t) {
        if (--count[c] < 0) return false;
    }
    return true;
}
```

---

## Complexity

| Operation | Average | Worst |
|-----------|---------|-------|
| Insert | O(1) | O(n) hash collision |
| Lookup | O(1) | O(n) |
| Delete | O(1) | O(n) |

Worst case is rare but can be triggered by adversarial inputs with many hash collisions. In contests, use custom hash or `map` (O(log n) guaranteed) when worried.

---

## Interview Notes

- Always consider using a hash map before nested loops — reduces O(n²) to O(n)
- `unordered_map` in C++ doesn't maintain insertion order; use `map` or a separate list if order matters
- Hashing non-primitive types (pairs, vectors) requires a custom hash function in C++
- In Python, `dict` and `set` are ordered by insertion since 3.7
