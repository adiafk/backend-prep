# Sliding Window Pattern

## Fixed vs Variable Size — When Each Applies

### Fixed-Size Window

The window always has exactly k elements. Slide it one step at a time: add the incoming element on the right, remove the outgoing element on the left.

**When to use:**
- "Maximum/minimum/average of a subarray of length k"
- "Count subarrays of exactly length k satisfying a property"
- Any problem where k (the window size) is given directly

**Template (C++):**
```cpp
// Build the first window
for (int i = 0; i < k; i++) processAdd(arr[i]);

for (int i = k; i < n; i++) {
    updateAnswer();
    processAdd(arr[i]);       // add right
    processRemove(arr[i - k]); // remove left
}
updateAnswer(); // final window
```

**Template (TypeScript):**
```typescript
for (let i = 0; i < k; i++) processAdd(arr[i]);

for (let i = k; i < arr.length; i++) {
    updateAnswer();
    processAdd(arr[i]);
    processRemove(arr[i - k]);
}
updateAnswer();
```

---

### Variable-Size Window

The window expands and contracts based on a validity condition. Right pointer always advances; left pointer advances only when the window becomes invalid.

**When to use:**
- "Longest/shortest subarray/substring satisfying a condition"
- Condition is monotonic: once a window of size w is valid, all smaller sub-windows are valid too (or vice versa)
- Frequency constraints: "at most k distinct characters", "no repeating characters"

**Core invariant:** The window [left, right] always satisfies the required property after each contraction step.

---

## Variable Window Template with Character Frequency Map

This template covers the majority of substring problems.

**C++ Template:**
```cpp
unordered_map<char, int> freq;
int left = 0;
int result = 0;

for (int right = 0; right < s.size(); right++) {
    // 1. Expand: add s[right] to the window
    freq[s[right]]++;

    // 2. Shrink: while window is invalid, move left forward
    while (windowIsInvalid(freq, left, right)) {
        freq[s[left]]--;
        if (freq[s[left]] == 0) freq.erase(s[left]);
        left++;
    }

    // 3. Update answer using current valid window
    result = max(result, right - left + 1);
}
return result;
```

**TypeScript Template:**
```typescript
const freq = new Map<string, number>();
let left = 0;
let result = 0;

for (let right = 0; right < s.length; right++) {
    // 1. Expand
    freq.set(s[right], (freq.get(s[right]) ?? 0) + 1);

    // 2. Shrink while invalid
    while (windowIsInvalid(freq, left, right)) {
        const lc = s[left];
        freq.set(lc, freq.get(lc)! - 1);
        if (freq.get(lc) === 0) freq.delete(lc);
        left++;
    }

    // 3. Update answer
    result = Math.max(result, right - left + 1);
}
return result;
```

---

## Solved Problems

---

### 1. Maximum Average Subarray I
**LeetCode 643**

**Problem:** Find the contiguous subarray of length k with the maximum average value.

**Intuition:**
Classic fixed-size window. Compute the sum of the first k elements. Then slide: add the new right element, subtract the element falling off the left. Track the maximum sum seen.

No sorting, no hash map — just addition and subtraction as the window slides.

**C++ Solution:**
```cpp
double findMaxAverage(vector<int>& nums, int k) {
    double windowSum = 0;
    for (int i = 0; i < k; i++) windowSum += nums[i];

    double maxSum = windowSum;
    for (int i = k; i < (int)nums.size(); i++) {
        windowSum += nums[i] - nums[i - k];
        maxSum = max(maxSum, windowSum);
    }
    return maxSum / k;
}
```

**Complexity:**
- Time: O(n) — one pass
- Space: O(1)

---

### 2. Longest Substring Without Repeating Characters
**LeetCode 3**

**Problem:** Find the length of the longest substring with all unique characters.

**Intuition:**
Variable window. Expand right freely. When a duplicate character enters the window (its frequency becomes 2), shrink from the left until the duplicate is gone.

The window invariant is: all characters in [left, right] are unique (frequency <= 1 for every character). The answer is the maximum window size seen while the invariant holds.

**C++ Solution:**
```cpp
int lengthOfLongestSubstring(string s) {
    unordered_map<char, int> freq;
    int left = 0, result = 0;

    for (int right = 0; right < (int)s.size(); right++) {
        freq[s[right]]++;

        while (freq[s[right]] > 1) { // duplicate found
            freq[s[left]]--;
            left++;
        }

        result = max(result, right - left + 1);
    }
    return result;
}
```

**Complexity:**
- Time: O(n) — each character enters and leaves the window at most once
- Space: O(min(n, charset)) — at most 26 entries for lowercase letters, 128 for ASCII

---

### 3. Minimum Window Substring
**LeetCode 76**

**Problem:** Given strings s and t, find the minimum window in s that contains all characters of t (including duplicates).

**Intuition:**
Variable window — but this time we want the shortest valid window rather than the longest.

Track a `need` map (required frequencies from t) and a `have` counter (how many characters in the current window meet their required frequency). The window is valid when have == number of distinct characters in t.

When valid: record the window, then shrink from left to find the minimum. When invalid: expand right.

**C++ Solution:**
```cpp
string minWindow(string s, string t) {
    unordered_map<char, int> need, window;
    for (char c : t) need[c]++;

    int have = 0, required = (int)need.size();
    int left = 0;
    int minLen = INT_MAX, minStart = 0;

    for (int right = 0; right < (int)s.size(); right++) {
        char c = s[right];
        window[c]++;
        if (need.count(c) && window[c] == need[c]) have++;

        while (have == required) {
            // Update answer
            if (right - left + 1 < minLen) {
                minLen = right - left + 1;
                minStart = left;
            }
            // Shrink from left
            char lc = s[left];
            window[lc]--;
            if (need.count(lc) && window[lc] < need[lc]) have--;
            left++;
        }
    }
    return minLen == INT_MAX ? "" : s.substr(minStart, minLen);
}
```

**Complexity:**
- Time: O(|s| + |t|) — each character in s enters and leaves the window once
- Space: O(|s| + |t|) for the frequency maps (bounded by charset size in practice)

---

### 4. Permutation in String
**LeetCode 567**

**Problem:** Given strings s1 and s2, return true if s2 contains a permutation of s1 as a substring.

**Intuition:**
A permutation of s1 is any anagram of s1. Two strings are anagrams if and only if their character frequency maps are equal.

Use a fixed-size window of length len(s1) over s2. Maintain a frequency map of the window and compare it against the frequency map of s1 at each position.

Optimization: instead of comparing two full maps each step, track a `matches` counter — the number of characters whose frequencies are currently equal between the window and s1.

**C++ Solution:**
```cpp
bool checkInclusion(string s1, string s2) {
    if (s1.size() > s2.size()) return false;

    vector<int> need(26, 0), window(26, 0);
    for (char c : s1) need[c - 'a']++;

    int k = (int)s1.size();
    int matches = 0;

    // Count how many characters already match (need[i] == 0 means no requirement)
    // Build first window
    for (int i = 0; i < k; i++) window[s2[i] - 'a']++;

    for (int i = 0; i < 26; i++)
        if (window[i] == need[i]) matches++;

    if (matches == 26) return true;

    for (int i = k; i < (int)s2.size(); i++) {
        int addIdx = s2[i] - 'a';
        int remIdx = s2[i - k] - 'a';

        // Add right character
        window[addIdx]++;
        if (window[addIdx] == need[addIdx])      matches++;
        else if (window[addIdx] - 1 == need[addIdx]) matches--;

        // Remove left character
        window[remIdx]--;
        if (window[remIdx] == need[remIdx])      matches++;
        else if (window[remIdx] + 1 == need[remIdx]) matches--;

        if (matches == 26) return true;
    }
    return false;
}
```

**Complexity:**
- Time: O(|s1| + |s2|) — O(26) = O(1) per step
- Space: O(1) — fixed 26-element arrays

---

### 5. Longest Repeating Character Replacement
**LeetCode 424**

**Problem:** You can replace at most k characters in a string. Find the length of the longest substring containing the same letter after replacements.

**Intuition:**
Variable window. The key observation: a window of size `right - left + 1` is valid if `(window size) - (count of most frequent char) <= k`. That is, the number of characters we need to replace is at most k.

Expand right freely. When the window becomes invalid (replacements needed > k), shrink left by one. Crucially, we never shrink the window below its current maximum size — we only slide it forward. This is because the answer is monotonically non-decreasing: once we find a window of size w, we only care about windows of size > w.

**C++ Solution:**
```cpp
int characterReplacement(string s, int k) {
    vector<int> freq(26, 0);
    int left = 0, maxFreq = 0, result = 0;

    for (int right = 0; right < (int)s.size(); right++) {
        freq[s[right] - 'A']++;
        maxFreq = max(maxFreq, freq[s[right] - 'A']);

        int windowSize = right - left + 1;
        if (windowSize - maxFreq > k) {
            // Window is invalid: shrink by one (slide forward)
            freq[s[left] - 'A']--;
            left++;
        }

        result = max(result, right - left + 1);
    }
    return result;
}
```

Note: `maxFreq` is never decremented when shrinking because we only care about windows at least as large as the current best. If `maxFreq` would decrease after shrinking, we simply don't update `result` (the window size stays the same), so correctness is preserved.

**Complexity:**
- Time: O(n) — one pass; each character processed once
- Space: O(1) — 26-element frequency array

---

## Decision Guide

```
Does the problem specify a fixed window size k?
    YES -> Fixed window: maintain running aggregate, slide by adding right and removing left
    NO  -> Is there a validity condition on the window contents?
               YES -> Variable window with shrink loop
                      - Longest valid window: maximize right - left + 1
                      - Shortest valid window: minimize right - left + 1 (shrink greedily)
               NO  -> Probably not a sliding window problem
```

## Quick Reference

| Problem | Window Type | Validity Condition | Answer |
|---|---|---|---|
| Maximum Average Subarray | Fixed k | N/A | max running sum |
| Longest No-Repeat Substring | Variable | all freq <= 1 | max window size |
| Minimum Window Substring | Variable | have == required | min window size |
| Permutation in String | Fixed len(s1) | matches == 26 | any match found |
| Longest Repeating Replacement | Variable | size - maxFreq <= k | max window size |
