/**
 * Order-based filtering rule for sequences with annotated geometries.
 *
 * When a new site with an order_index annotation is added to the sequence,
 * this rule checks all triple constraints (i, j, k) where j is the newly
 * added site. This requires O(n²) checks per branching operation.
 */

#ifndef TSPN_ORDER_FILTERING_H
#define TSPN_ORDER_FILTERING_H

#include "tspn/common.h"
#include "tspn/strategies/rule.h"
#include <optional>

namespace tspn {

/**
 * Rule that enforces order constraints on annotated geometries.
 *
 * This rule filters sequences based on order_index annotations in geometries.
 * When branching adds a new site j to the sequence, this rule checks all
 * triples (i, j, k) where i and k are other sites in the sequence.
 * This allows checking constraints that depend on the relationship between
 * three sites.
 */
class OrderFiltering : public SequenceRule {
public:
  OrderFiltering() = default;

  /**
   * Setup the rule with instance information.
   * Called once at the start of branch and bound.
   */
  void setup(const Instance *instance, std::shared_ptr<Node> &root,
             SolutionPool *solution_pool) override;

  /**
   * Check if a proposed sequence satisfies all order constraints.
   *
   * This is called before creating each child node. If this returns false,
   * the sequence is rejected and no child node is created.
   *
   * For each newly added site j, checks all triples (i, j, k) where i and k
   * are other sites in the sequence. Complexity: O(n²) per call.
   *
   * @param seq The proposed sequence to check
   * @param parent The parent node (for finding what's new in seq)
   * @return true if all order constraints are satisfied, false otherwise
   */
  bool is_ok(const std::vector<TourElement> &seq, const Node &parent) override;

  ~OrderFiltering() override = default;

private:
  const Instance *instance_ = nullptr;

  /**
   * Find the newly added geometry index in the sequence.
   *
   * @param seq The new sequence
   * @param parent_seq The parent sequence
   * @return The geometry index of the newly added element, or nullopt if none
   */
  std::optional<unsigned>
  find_new_element(const std::vector<TourElement> &seq,
                   const std::vector<TourElement> &parent_seq) const;
};

} // namespace tspn
#endif // TSPN_ORDER_FILTERING_H
