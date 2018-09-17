#include "priority_ctpl.h"
#include <gtest/gtest.h>
#include <string>

void verify_pop_order(ctpl::detail::PriorityQueue<std::string> &pq,
                      std::vector<std::string> order) {
    for(std::string s : order) {
        EXPECT_FALSE(pq.empty());
        std::string out;
        pq.pop(out);
        EXPECT_EQ(out, s);
    }
    EXPECT_TRUE(pq.empty());
}

TEST(TestPriorityQueue, TestCreate) {
    ctpl::detail::PriorityQueue<int> pq;
    EXPECT_TRUE(pq.empty());
}

TEST(TestPriorityQueue, TestPush) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(0, 1, "third");
    pq.push(0, 3, "first");
    pq.push(0, 2, "second");

    verify_pop_order(pq, {"first", "second", "third"});
}

TEST(TestPriorityQueue, TestPushNoPriority) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(0, 0, "first");
    pq.push(0, 0, "second");
    pq.push(0, 0, "third");

    verify_pop_order(pq, {"first", "second", "third"});
}

TEST(TestPriorityQueue, TestRemoveId) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(3, 1, "third");
    pq.push(1, 3, "first");
    pq.push(2, 2, "second");

    pq.remove_id(1);

    verify_pop_order(pq, {"second", "third"});
}

TEST(TestPriorityQueue, TestRemovePriority) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(3, 1, "third");
    pq.push(1, 3, "first");
    pq.push(2, 2, "second");

    pq.remove_priority(2);

    verify_pop_order(pq, {"first", "third"});
}

TEST(TestPriorityQueue, TestNoRemove) {
    ctpl::detail::PriorityQueue<std::string> pq;
    pq.push(3, 1, "third");
    pq.push(1, 3, "first");
    pq.push(2, 2, "second");

    pq.remove_id(5);
    pq.remove_priority(7);

    verify_pop_order(pq, {"first", "second", "third"});
}
