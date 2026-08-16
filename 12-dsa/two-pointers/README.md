# Two Pointers Pattern

## When to Use Two Pointers

Two pointers is a technique where you maintain two index variables that traverse a data structure — typically an array or string — to reduce O(n^2) brute-force solutions to O(n).

### Indicators that two pointers applies:
- **Sorted array + pair condition**: find two elements summing to a target, or satisfying some inequality
- **Palindrome / symmetry check**: comparing characters from both ends toward the center
- **Partitioning**: rearranging elements in-place (e.g., Dutch national flag, remove duplicates)
- **Subarray / window problems**: when sliding window would also work, two pointers on sorted data is often cleaner
- **Merging two sorted arrays**: classic two-pointer merge

---

## The Three Variants

### 1. Opposite Ends (Converging)

Both pointers start at opposite ends and move toward each other.

**Use when:** sorted array, palindrome check, maximize/minimize some expression involving two elements.

**C++ Template:**
```cpp
int left = 0, right = n - 1;
while (left < right) {
    int val = arr[left] + arr[right]; // or some condition
    if (val == target) {
        // found answer
        left++; right--;
    } else if (val < target) {
        left++;
    } else {
        right--;
    }
}
```

**TypeScript Template:**
```typescript
let left = 0, right = arr.length - 1;
while (left < right) {
    const val = arr[left] + arr[right];
    if (val === target) {
        // process answer
        left++; right--;
    } else if (val < target) {
        left++;
    } else {
        right--;
    }
}
```

---

### 2. Same Direction — Fast / Slow (Runner)

Both pointers start at the beginning; one advances faster or under a different condition.

**Use when:** detect cycle in linked list, remove duplicates in-place, find middle of list, partition array.

**C++ Template:**
```cpp
int slow = 0;
for (int fast = 0; fast < n; fast++) {
    if (condition(arr[fast])) {
        arr[slow++] = arr[fast]; // keep valid elements
    }
}
// slow is the new length
```

**TypeScript Template:**
```typescript
let slow = 0;
for (let fast = 0; fast < arr.length; fast++) {
    if (condition(arr[fast])) {
        arr[slow++] = arr[fast];
    }
}
// slow is the new length
```

---

### 3. Partition (Three-Way)

Three pointers (low, mid, high) to sort or classify elements into three categories.

**Use when:** Dutch national flag problem, sort colors, partition around a pivot.

**C++ Template:**
```cpp
int low = 0, mid = 0, high = n - 1;
while (mid <= high) {
    if (arr[mid] == 0) {
        swap(arr[low++], arr[mid++]);
    } else if (arr[mid] == 1) {
        mid++;
    } else {
        swap(arr[mid], arr[high--]);
    }
}
```

**TypeScript Template:**
```typescript
let low = 0, mid = 0, high = arr.length - 1;
while (mid <= high) {
    if (arr[mid] === 0) {
        [arr[low], arr[mid]] = [arr[mid], arr[low]];
        low++; mid++;
    } else if (arr[mid] === 1) {
        mid++;
    } else {
        [arr[mid], arr[high]] = [arr[high], arr[mid]];
        high--;
    }
}
```

---

## Solved Problems

---

### 1. Valid Palindrome
**LeetCode 125**

**Problem:** Given a string, determine if it is a palindrome considering only alphanumeric characters (case-insensitive).

**Intuition:**
Place one pointer at the start, one at the end. Skip non-alphanumeric characters. Compare characters after lowercasing. If any mismatch is found, return false. If pointers cross, it is a palindrome.

This is the canonical "opposite ends" two-pointer pattern — no extra space needed, single pass.

**C++ Solution:**
```cpp
bool isPalindrome(string s) {
    int left = 0, right = (int)s.size() - 1;
    while (left < right) {
        while (left < right && !isalnum(s[left]))  left++;
        while (left < right && !isalnum(s[right])) right--;
        if (tolower(s[left]) != tolower(s[right])) return false;
        left++; right--;
    }
    return true;
}
```

**Complexity:**
- Time: O(n) — single pass through the string
- Space: O(1) — no extra data structures

---

### 2. Two Sum II (Input Array Is Sorted)
**LeetCode 167**

**Problem:** Given a sorted array, find two numbers that add up to a target. Return their 1-indexed positions.

**Intuition:**
Because the array is sorted, start with the widest possible window: left = 0, right = n-1. If the sum is too small, increment left to increase it. If too large, decrement right to decrease it. Exactly one solution is guaranteed.

The sorted property is the key — it gives us a monotonic relationship that lets us confidently move either pointer.

**C++ Solution:**
```cpp
vector<int> twoSum(vector<int>& numbers, int target) {
    int left = 0, right = (int)numbers.size() - 1;
    while (left < right) {
        int sum = numbers[left] + numbers[right];
        if (sum == target)   return {left + 1, right + 1};
        else if (sum < target) left++;
        else                   right--;
    }
    return {}; // guaranteed to find answer
}
```

**Complexity:**
- Time: O(n) — each pointer moves at most n steps total
- Space: O(1)

---

### 3. 3Sum
**LeetCode 15**

**Problem:** Find all unique triplets in an array that sum to zero.

**Intuition:**
Sort the array first. Iterate with index i as the "anchor" element. For each i, run the Two Sum II pattern on the subarray to the right of i with target = -nums[i]. After finding a valid pair, skip duplicate values for both pointers to avoid duplicate triplets.

Sorting is the prerequisite — it enables the two-pointer inner loop and makes deduplication straightforward.

**C++ Solution:**
```cpp
vector<vector<int>> threeSum(vector<int>& nums) {
    sort(nums.begin(), nums.end());
    vector<vector<int>> result;
    int n = nums.size();

    for (int i = 0; i < n - 2; i++) {
        if (i > 0 && nums[i] == nums[i - 1]) continue; // skip duplicates for i

        int left = i + 1, right = n - 1;
        while (left < right) {
            int sum = nums[i] + nums[left] + nums[right];
            if (sum == 0) {
                result.push_back({nums[i], nums[left], nums[right]});
                while (left < right && nums[left]  == nums[left + 1])  left++;
                while (left < right && nums[right] == nums[right - 1]) right--;
                left++; right--;
            } else if (sum < 0) {
                left++;
            } else {
                right--;
            }
        }
    }
    return result;
}
```

**Complexity:**
- Time: O(n^2) — outer loop O(n), inner two-pointer O(n)
- Space: O(1) auxiliary (excluding output)

---

### 4. Container With Most Water
**LeetCode 11**

**Problem:** Given n vertical lines at positions 0..n-1 with heights height[i], find two lines that together with the x-axis form a container holding the most water.

**Intuition:**
The water held between lines i and j is min(height[i], height[j]) * (j - i). Start with the widest container (i=0, j=n-1). The width can only decrease as we move pointers inward, so the only way to potentially increase water is to find a taller line. Always move the pointer pointing to the shorter line inward — moving the taller one can only hurt or maintain the current minimum height while reducing width.

**C++ Solution:**
```cpp
int maxArea(vector<int>& height) {
    int left = 0, right = (int)height.size() - 1;
    int maxWater = 0;
    while (left < right) {
        int water = min(height[left], height[right]) * (right - left);
        maxWater = max(maxWater, water);
        if (height[left] < height[right]) left++;
        else                              right--;
    }
    return maxWater;
}
```

**Complexity:**
- Time: O(n) — single pass
- Space: O(1)

---

### 5. Trapping Rain Water
**LeetCode 42**

**Problem:** Given an elevation map, compute how much water it can trap after raining.

**Intuition:**
Water at position i is bounded by min(maxLeft[i], maxRight[i]) - height[i]. A naive approach precomputes these arrays in O(n) space. The two-pointer approach avoids extra space by maintaining running maxima from both ends.

Key insight: if maxLeft < maxRight, then the water at the left pointer is fully determined by maxLeft (the right side is guaranteed taller). Process the left pointer and advance it inward. Symmetrically for the right pointer. This handles each element exactly once.

**C++ Solution:**
```cpp
int trap(vector<int>& height) {
    int left = 0, right = (int)height.size() - 1;
    int maxLeft = 0, maxRight = 0;
    int water = 0;

    while (left < right) {
        if (height[left] < height[right]) {
            if (height[left] >= maxLeft) maxLeft = height[left];
            else                         water += maxLeft - height[left];
            left++;
        } else {
            if (height[right] >= maxRight) maxRight = height[right];
            else                           water += maxRight - height[right];
            right--;
        }
    }
    return water;
}
```

**Complexity:**
- Time: O(n) — each element visited exactly once
- Space: O(1) — no prefix arrays needed

---

## Quick Reference

| Problem | Variant | Key Insight |
|---|---|---|
| Valid Palindrome | Opposite ends | Skip non-alphanumeric, compare inward |
| Two Sum II | Opposite ends | Sorted => adjust sum by moving either pointer |
| 3Sum | Opposite ends + sort | Fix one element, two-pointer for the rest |
| Container With Most Water | Opposite ends | Move shorter line pointer inward |
| Trapping Rain Water | Opposite ends | Running max from each side; process smaller side |
