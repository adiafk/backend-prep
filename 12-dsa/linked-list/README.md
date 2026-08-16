# Linked Lists

## Core Patterns

Most linked list problems use one of: fast/slow pointers, reversal, or dummy head.

---

## Fast / Slow Pointers

```cpp
// Detect cycle
bool hasCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;
    }
    return false;
}

// Find middle (slow is at middle when fast reaches end)
ListNode* findMiddle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}
```

---

## Reversal

```cpp
ListNode* reverseList(ListNode* head) {
    ListNode* prev = nullptr;
    ListNode* curr = head;
    while (curr) {
        ListNode* next = curr->next;  // save next
        curr->next = prev;            // reverse pointer
        prev = curr;                  // advance prev
        curr = next;                  // advance curr
    }
    return prev;  // prev is new head
}
```

---

## Dummy Head

Use when the head node might change or to simplify edge cases at the start of the list.

```cpp
// Merge two sorted lists
ListNode* mergeTwoLists(ListNode* l1, ListNode* l2) {
    ListNode dummy(0);
    ListNode* tail = &dummy;
    while (l1 && l2) {
        if (l1->val <= l2->val) { tail->next = l1; l1 = l1->next; }
        else                    { tail->next = l2; l2 = l2->next; }
        tail = tail->next;
    }
    tail->next = l1 ? l1 : l2;
    return dummy.next;
}
```

---

## Problems

### Remove Nth Node From End
**Trick**: advance fast pointer n steps first, then move both until fast reaches the end.
```cpp
ListNode* removeNthFromEnd(ListNode* head, int n) {
    ListNode dummy(0, head);
    ListNode* fast = &dummy;
    ListNode* slow = &dummy;
    for (int i = 0; i <= n; i++) fast = fast->next;
    while (fast) { slow = slow->next; fast = fast->next; }
    slow->next = slow->next->next;
    return dummy.next;
}
```

### Reorder List (L0→L1→...→Ln → L0→Ln→L1→Ln-1)
```cpp
void reorderList(ListNode* head) {
    // 1. Find middle
    ListNode* slow = head, *fast = head;
    while (fast->next && fast->next->next) {
        slow = slow->next; fast = fast->next->next;
    }
    // 2. Reverse second half
    ListNode* second = slow->next;
    slow->next = nullptr;
    second = reverseList(second);
    // 3. Merge
    ListNode* first = head;
    while (second) {
        ListNode* tmp1 = first->next, *tmp2 = second->next;
        first->next = second;
        second->next = tmp1;
        first = tmp1; second = tmp2;
    }
}
```

---

## Complexity

| Operation | Singly Linked | Array |
|-----------|-------------|-------|
| Access by index | O(n) | O(1) |
| Insert at head | O(1) | O(n) |
| Insert at tail | O(n) (O(1) with tail ptr) | O(1) amortized |
| Delete (given ptr) | O(1) | O(n) |
| Search | O(n) | O(n) |
