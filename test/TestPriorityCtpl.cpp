#include "priority_ctpl.h"
#include <gtest/gtest.h>
#include <string>

TEST(TestPriorityQueue, TestCreate) {
    ctpl::detail::PriorityQueue<int> pq;
    ASSERT_TRUE(pq.empty());
}

TEST(TestPriorityQueue, TestPush) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(0, 1, "third");
    pq.push(0, 3, "first");
    pq.push(0, 2, "second");

    std::string out;
    pq.pop(out);
    ASSERT_EQ(out, "first");
    pq.pop(out);
    ASSERT_EQ(out, "second");
    pq.pop(out);
    ASSERT_EQ(out, "third");
}

TEST(TestPriorityQueue, TestRemoveId) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(3, 1, "third");
    pq.push(1, 3, "first");
    pq.push(2, 2, "second");

    pq.remove_id(1);

    std::string out;
    pq.pop(out);
    ASSERT_EQ(out, "second");
    pq.pop(out);
    ASSERT_EQ(out, "third");
}

TEST(TestPriorityQueue, TestRemovePriority) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(3, 1, "third");
    pq.push(1, 3, "first");
    pq.push(2, 2, "second");

    pq.remove_priority(2);

    std::string out;
    pq.pop(out);
    ASSERT_EQ(out, "first");
    pq.pop(out);
    ASSERT_EQ(out, "third");
}
