# Strings

## Key Operations

```cpp
string s = "hello world";

// Length, access
s.size();         // 11
s[0];             // 'h'
s.front();        // 'h'
s.back();         // 'd'

// Substring
s.substr(6, 5);   // "world" (start, length)

// Find
s.find("world");  // 6 — position of first match
s.find("xyz");    // string::npos if not found

// Modification
s += "!";
reverse(s.begin(), s.end());

// Conversion
stoi("42");            // string → int
to_string(42);         // int → string
tolower('A');          // 'a' — single char
```

---

## Two Pointer on Strings

### Valid Palindrome
```cpp
bool isPalindrome(string s) {
    int left = 0, right = s.size() - 1;
    while (left < right) {
        while (left < right && !isalnum(s[left]))  left++;
        while (left < right && !isalnum(s[right])) right--;
        if (tolower(s[left]) != tolower(s[right])) return false;
        left++; right--;
    }
    return true;
}
```

---

## Sliding Window on Strings

### Longest Substring Without Repeating Characters
```cpp
int lengthOfLongestSubstring(string s) {
    unordered_map<char, int> last; // char → last seen index
    int maxLen = 0, left = 0;
    
    for (int right = 0; right < s.size(); right++) {
        if (last.count(s[right]) && last[s[right]] >= left) {
            left = last[s[right]] + 1;  // shrink window
        }
        last[s[right]] = right;
        maxLen = max(maxLen, right - left + 1);
    }
    return maxLen;
}
// Time: O(n) | Space: O(min(n, charset))
```

### Minimum Window Substring
Find smallest window in s containing all characters of t.

```cpp
string minWindow(string s, string t) {
    unordered_map<char, int> need, have;
    for (char c : t) need[c]++;
    
    int required = need.size(), formed = 0;
    int left = 0, minLen = INT_MAX, minLeft = 0;
    
    for (int right = 0; right < s.size(); right++) {
        have[s[right]]++;
        if (need.count(s[right]) && have[s[right]] == need[s[right]]) {
            formed++;
        }
        while (formed == required) {
            if (right - left + 1 < minLen) {
                minLen = right - left + 1;
                minLeft = left;
            }
            have[s[left]]--;
            if (need.count(s[left]) && have[s[left]] < need[s[left]]) {
                formed--;
            }
            left++;
        }
    }
    return minLen == INT_MAX ? "" : s.substr(minLeft, minLen);
}
// Time: O(|s| + |t|) | Space: O(|s| + |t|)
```

---

## String Matching

### KMP (Knuth-Morris-Pratt) — O(n + m)
Avoids re-scanning characters after a mismatch by using a failure function.

```cpp
vector<int> buildLPS(const string& pattern) {
    int m = pattern.size();
    vector<int> lps(m, 0);
    int len = 0, i = 1;
    while (i < m) {
        if (pattern[i] == pattern[len]) {
            lps[i++] = ++len;
        } else if (len) {
            len = lps[len - 1];  // fall back
        } else {
            lps[i++] = 0;
        }
    }
    return lps;
}

vector<int> kmpSearch(const string& text, const string& pattern) {
    vector<int> lps = buildLPS(pattern);
    vector<int> matches;
    int i = 0, j = 0;
    while (i < (int)text.size()) {
        if (text[i] == pattern[j]) { i++; j++; }
        if (j == (int)pattern.size()) {
            matches.push_back(i - j);
            j = lps[j - 1];
        } else if (i < (int)text.size() && text[i] != pattern[j]) {
            if (j) j = lps[j - 1];
            else i++;
        }
    }
    return matches;
}
```

---

## Common Problems

### Encode / Decode Strings
Serialize `["lint","code","love","you"]` to a single string and back.

```cpp
string encode(vector<string>& strs) {
    string result;
    for (const string& s : strs) {
        result += to_string(s.size()) + "#" + s;
    }
    return result;
}

vector<string> decode(string s) {
    vector<string> result;
    int i = 0;
    while (i < (int)s.size()) {
        int j = s.find('#', i);
        int len = stoi(s.substr(i, j - i));
        result.push_back(s.substr(j + 1, len));
        i = j + 1 + len;
    }
    return result;
}
```

### Longest Palindromic Substring (Expand Around Center)
```cpp
string longestPalindrome(string s) {
    int start = 0, maxLen = 1;
    
    auto expand = [&](int l, int r) {
        while (l >= 0 && r < (int)s.size() && s[l] == s[r]) {
            if (r - l + 1 > maxLen) { maxLen = r - l + 1; start = l; }
            l--; r++;
        }
    };
    
    for (int i = 0; i < (int)s.size(); i++) {
        expand(i, i);    // odd length
        expand(i, i+1);  // even length
    }
    return s.substr(start, maxLen);
}
// Time: O(n²) | Space: O(1)
// Manacher's algorithm achieves O(n) but is complex — expand-around-center is usually acceptable
```

---

## Interview Notes

- Always clarify: ASCII or Unicode? Case-sensitive? What counts as alphanumeric?
- `string::npos` is returned by `find()` when not found — compare with `!= string::npos`
- String operations like `substr()` and `+` are O(n) — avoid in inner loops
- For frequency maps of lowercase letters: `int freq[26] = {}` beats `unordered_map<char,int>` for constant-size alphabets
