// Tests for PartialSequenceSolution from tspn/relaxed_solution.h

#include <gtest/gtest.h>

#include "tspn_core/relaxed_solution.h"

namespace tspn {

TEST(PartialSequenceSolution, BasicSolution) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{3, 0}, {2, 0}, {3, 1}}},
      Polygon{{{3, 3}, {2, 3}, {3, 2}}},
      Polygon{{{0, 3}, {1, 3}, {0, 2}}},
  };
  Instance instance(polygons);
  SocSolver soc(false);
  auto sequence = {TourElement(instance, 0), TourElement(instance, 1),
                   TourElement(instance, 2), TourElement(instance, 3)};
  PartialSequenceSolution pss(&instance, &soc, sequence,
                              std::list<StashedTourElement>(), 0.001);
  EXPECT_NEAR(pss.obj(), 8.0, 1e-6);
  pss.simplify();
  EXPECT_NEAR(pss.obj(), 8.0, 1e-6);
  EXPECT_TRUE(pss.is_feasible());
}

// --- StashedTourElement::get_reinsert_positions tests ---

class StashedReinsertTest : public ::testing::Test {
protected:
  // 5 polygons spread out so each TourElement has a distinct geo_index 0..4
  std::vector<SiteVariant> polygons{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},    Polygon{{{3, 0}, {4, 0}, {3, 1}}},
      Polygon{{{6, 0}, {7, 0}, {6, 1}}},    Polygon{{{9, 0}, {10, 0}, {9, 1}}},
      Polygon{{{12, 0}, {13, 0}, {12, 1}}},
  };
  Instance instance{polygons};

  // Build a sequence [0, 1, 2, 3] to test reinsertion of element 4.
  std::vector<TourElement> make_seq(std::initializer_list<unsigned> indices) {
    std::vector<TourElement> seq;
    for (auto idx : indices) {
      seq.push_back(TourElement(instance, idx));
    }
    return seq;
  }

  StashedTourElement make_stash(unsigned geo_idx, std::set<unsigned> earlier,
                                std::set<unsigned> later) {
    TourElement te(instance, geo_idx);
    StashedTourElement ste(te);
    ste.earlier_geo_idxs = std::move(earlier);
    ste.later_geo_idxs = std::move(later);
    return ste;
  }
};

TEST_F(StashedReinsertTest, NoConstraintsAllowsAnyPosition) {
  // No earlier/later constraints => earliest=0, latest=seq.size()
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, {}, {});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 0u);
  EXPECT_EQ(latest, 4u);
}

TEST_F(StashedReinsertTest, EarlierConstraintPushesEarliest) {
  // Element 4 must come after element 1 (geo_index 1 is at position 1)
  // => earliest = 2 (after position 1)
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{1}, /*later=*/{});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 2u);
  EXPECT_EQ(latest, 4u);
}

TEST_F(StashedReinsertTest, LaterConstraintPushesLatest) {
  // Element 4 must come before element 2 (geo_index 2 is at position 2)
  // => latest = 2
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{}, /*later=*/{2});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 0u);
  EXPECT_EQ(latest, 2u);
}

TEST_F(StashedReinsertTest, BothConstraintsNarrowRange) {
  // Must be after 1 and before 3 => earliest=2, latest=3
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{1}, /*later=*/{3});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 2u);
  EXPECT_EQ(latest, 3u);
}

TEST_F(StashedReinsertTest,
       ImpossibleConstraintsGiveEarliestGreaterThanLatest) {
  // Must be after 3 (pos 3) and before 1 (pos 1) => earliest=4 > latest=1
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{3}, /*later=*/{1});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_GT(earliest, latest);
}

TEST_F(StashedReinsertTest, MultipleEarlierTakesLast) {
  // Must be after both 0 and 2 => earliest determined by last match = pos 2+1=3
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{0, 2}, /*later=*/{});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 3u);
  EXPECT_EQ(latest, 4u);
}

TEST_F(StashedReinsertTest, MultipleLaterTakesFirst) {
  // Must be before both 2 and 3 => latest determined by first match = pos 2
  auto seq = make_seq({0, 1, 2, 3});
  auto ste = make_stash(4, /*earlier=*/{}, /*later=*/{2, 3});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 0u);
  EXPECT_EQ(latest, 2u);
}

TEST_F(StashedReinsertTest, ConstraintNotInSequenceIsIgnored) {
  // earlier={4} but 4 is not in the sequence => no constraint applied
  auto seq = make_seq({0, 1, 2});
  auto ste = make_stash(3, /*earlier=*/{4}, /*later=*/{});
  auto [earliest, latest] = ste.get_reinsert_positions(seq);
  EXPECT_EQ(earliest, 0u);
  EXPECT_EQ(latest, 3u);
}

} // namespace tspn
