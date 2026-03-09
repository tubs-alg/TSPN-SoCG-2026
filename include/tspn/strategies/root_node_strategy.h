//
// Created by Dominik Krupke on 14.12.22.
// The task of this file is to find the root node to start the BnB-algorithm
// with. For a tour, this can be three circles, as the order for three does
// not matter. For a path it is reasonably to start with the circle that is
// most distanced to both end points (we generally want the root node to be
// as expensive as possible and thus close to the cost of the feasible
// solutions)
//
// For Tours: Use Convex Hull Strategy
// For Paths: Use Longest Edge Plus Farthest Circle
//

#ifndef TSPN_ROOT_NODE_STRATEGY_H
#define TSPN_ROOT_NODE_STRATEGY_H
#include "doctest/doctest.h"
#include "tspn/common.h"
#include "tspn/node.h"
#include <random>
#include <vector>

namespace tspn {
    /** Root Node Strategy Base Class.
     * A root node strategy needs to be correct under all circumstances. If it
    cannot guarantee, it must error out.
     * Note that any root node of three sites is trivially true, as direction is
    irrelevant (symmetric distances).
     * In Tour Mode, even rotation is irrelevant.
     * In Path Mode, the first and last node in the root node MUST be the origin and
    target sites, and rotation is relevant.
     */
    class RootNodeStrategy {
    public:
        [[nodiscard]] virtual std::shared_ptr<Node> get_root_node(Instance &instance,
                                                                  SocSolver &soc) = 0;

        virtual ~RootNodeStrategy() = default;
    };

    /**
     * The longest edge plus furthest site strategy first selects the longest
     * edge and then the site most distanced to it. Thus, for tours the
     * root solution will have three polys. Not that the order does not matter
     * for three polys. When using the convex hull strategy, the root node
     * may be illegal because of a bad rotation.
     *
     * In path mode, it selects the site with
     * largest cumulative distance to origin and target. */
    class LongestEdgePlusFurthestSite : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * The longest triple strategy selects the three sites with the longest
     * cumulative distances between them. In path mode, it selects the site with
     * largest cumulative distance to origin and target.
     */
    class LongestTriple : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * The longest triple with point strategy selects the three sites with the
     * longest cumulative distances between them, among them at least one point.
     * In path mode, it selects the site with largest cumulative distance to origin
     * and target, ignoring point requirements.
     */
    class LongestTripleWithPoint : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * The longest point triple strategy selects the three sites with the longest
     * cumulative distances between them, among them as many (>=1) points as
     * possible. in path mode, it selects the site with largest cumulative distance
     * to origin and target, ignoring point requirements.
     */
    class LongestPointTriple : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * Selects the pair of sites with the maximal distance (tour),
     * or origin/target pair for path instances.
     */
    class LongestPair : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * Selects a random pair of distinct sites (tour),
     * or origin/target pair for path instances.
     */
    class RandomPair : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    /**
     * duh
     */
    class RandomRoot : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override {
            std::cout << "using RandomRoot" << std::endl;
            std::vector<unsigned> randseq;
            if (instance.is_path()) {
                if (size(instance) <= 1) {
                    throw std::logic_error("cannot build path root node from less than two "
                        "sites (origin+target).");
                }
                std::vector<TourElement> seq;
                seq.push_back(TourElement(instance, 0)); // origin
                if (size(instance) >= 3) {
                    std::default_random_engine generator;
                    std::uniform_int_distribution<unsigned> distribution(
                        2, instance.size() - 1);
                    int geo_idx = static_cast<unsigned>(distribution(generator));
                    seq.push_back(TourElement(instance, geo_idx));
                }
                seq.push_back(TourElement(instance, 1)); // target
                return std::make_shared<Node>(seq, &instance, &soc);
            } else {
                if (instance.size() <= 3) {
                    // trivial case
                    for (unsigned i = 0; i < instance.size(); ++i) {
                        randseq.push_back(i);
                    }
                } else {
                    randseq.resize(instance.size());
                    std::iota(randseq.begin(), randseq.end(), 0);
                    std::shuffle(randseq.begin(), randseq.end(),
                                 std::mt19937{std::random_device{}()});
                    randseq.resize(3);
                }
            }
            std::vector<TourElement> seq;
            for (auto geo_idx: randseq) {
                seq.push_back(TourElement(instance, geo_idx));
            }
            return std::make_shared<Node>(seq, &instance, &soc);
        }
    };

    /**
     * Greedily selects annotated sites in increasing order_index while skipping
     * any site whose geo_index appears in overlapping_order_geo_indices of
     * already selected sites.
     */
    class OrderRootStrategy : public RootNodeStrategy {
    public:
        std::shared_ptr<Node> get_root_node(Instance &instance,
                                            SocSolver &soc) override;
    };

    TEST_CASE("Root Node Selection") {
        // The strategy should choose the triangle and implicitly cover the
        // second poly.
        auto polygons = std::vector<SiteVariant>{
            Polygon{{{0, 0}, {0, -1}, {-1, 0}}},
            Polygon{{{3, 0}, {3, -1}, {2, -1}}},
            Polygon{{{6, 0}, {6, -1}, {5, -1}}},
            Polygon{{{3, 6}, {3, 5}, {2, 6}}},
        };
        Instance instance(polygons);
        LongestEdgePlusFurthestSite rns;
        SocSolver soc(false);
        auto root = rns.get_root_node(instance, soc);
        CHECK(root->is_feasible());
    }
} // namespace tspn
#endif // TSPN_ROOT_NODE_STRATEGY_H
