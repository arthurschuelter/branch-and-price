#include "pricing.hpp"

#include <chrono>
#include <utility>


Pricer::Pricer(const Instance& inst) 
    : inst_(inst) {}

PricingResult Pricer::heuristic(
    const std::vector<double>& customerDuals,
    const std::vector<double>& dayDuals,
    const std::vector<BranchDecision>& decisions,
    double remainingSeconds
) const {
    if (remainingSeconds <= 0.0) {
        return {PricingStatus::Interrupted, {}};
    }

    const auto start = std::chrono::steady_clock::now();

    int n = inst_.vertexCount();

    if (customerDuals.size() != static_cast<std::size_t>(n) ||
        dayDuals.size() != static_cast<std::size_t>(n)) {
            return {PricingStatus::Error, {}};
    }

    for (int i = 0; i < n; ++i) {
       if (!std::isfinite(customerDuals[i]) ||
            !std::isfinite(dayDuals[i])) {
                return {PricingStatus::Error, {}};
        }
    }
 
    for (const auto& decision : decisions) {
        if (decision.i < 0 || decision.i >= n ||
            decision.j < 0 || decision.j >= n ||
            decision.i == inst_.depot || decision.j == inst_.depot ||
            decision.i == decision.j) {
            return {PricingStatus::Error, {}};
        }
    }

    std::vector<int> initialGroup(n);
    for (int i = 0; i < n; ++i) {
        initialGroup[i] = i;
    }

    for (const auto& decision : decisions) {
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= remainingSeconds) {
            return {PricingStatus::Interrupted, {}};
        }

        if (!decision.same) {
            continue;
        }

        const int groupI = initialGroup[decision.i];
        const int groupJ = initialGroup[decision.j];

        for (int& group : initialGroup) {
            if (group == groupJ) {
                group = groupI;
            }
        }
    }

    std::vector<std::vector<int>> groups(n);
    for (int customer = 0; customer < n; ++customer) {
        if (customer == inst_.depot) {
            continue;
        }

        groups[initialGroup[customer]].push_back(customer);
    }
 
    for (const auto& decision : decisions) {
        if (!decision.same &&
            initialGroup[decision.i] == initialGroup[decision.j]) {
            return {PricingStatus::Error, {}};
        }
    }
    
    constexpr double eps = 1e-6;
    std::vector<Column> improvingColumns;
    
    for (int d = 1; d < n; ++d) {
        for (const auto& group : groups) {
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed >= remainingSeconds) {
                return {PricingStatus::Interrupted, std::move(improvingColumns)};
            }

            if (group.empty()) {
                continue;
            }

            const auto evaluation = inst_.evaluateRoute(group, d);
            if (!evaluation.valid) {
                continue;
            }

            Column column;
            column.route = group;
            column.day = d;
            column.cost = evaluation.total_cost;
            
            column.has = std::vector<bool>(n, false);
            for (int customer: group) {
                column.has[customer] = true;
            }
            
            for (const auto& otherGroup : groups) {
                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - start
                ).count();

                if (elapsed >= remainingSeconds) {
                    return {PricingStatus::Interrupted, std::move(improvingColumns)};
                }

                if (otherGroup.empty() || &otherGroup == &group) {
                    continue;
                }

                std::vector<int> candidateRoute = column.route;
                candidateRoute.insert(
                    candidateRoute.end(),
                    otherGroup.begin(),
                    otherGroup.end()
                );

                const auto candidateEvaluation =
                    inst_.evaluateRoute(candidateRoute, d);

                if (!candidateEvaluation.valid) {
                    continue;
                }
                std::vector<bool> candidateHas(n, false);
                for (int customer : candidateRoute) {
                    candidateHas[customer] = true;
                }

                bool allowed = true;
                for (const auto& decision : decisions) {
                    if (!decision.same &&
                        candidateHas[decision.i] &&
                        candidateHas[decision.j]) {
                        allowed = false;
                        break;
                    }
                }

                if (!allowed) {
                    continue;
                }

                double rc = candidateEvaluation.total_cost - dayDuals[d];
                for (int customer : candidateRoute) {
                    rc -= customerDuals[customer];
                }

                if (rc < -eps) {
                    Column candidate;
                    candidate.route = candidateRoute;
                    candidate.has = candidateHas;
                    candidate.day = d;
                    candidate.cost = candidateEvaluation.total_cost;

                    improvingColumns.push_back(candidate);
                }

            }
        }
    }

    return {
       PricingStatus::Completed, 
       std::move(improvingColumns)
    };
}

PricingResult Pricer::exact(
    const std::vector<double>& customerDuals,
    const std::vector<double>& dayDuals,
    const std::vector<BranchDecision>& decisions,
    double remainingSeconds
) const {
    // O modelo exato ainda será implementado.
    return {PricingStatus::Interrupted, {}};
}
