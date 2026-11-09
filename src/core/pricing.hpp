#pragma once

#include "branch_and_price.hpp"

class Pricer {
public:
    explicit Pricer(const Instance& inst);

    PricingResult heuristic(
        const std::vector<double>& customerDuals,
        const std::vector<double>& dayDuals,
        const std::vector<BranchDecision>& decisions,
        double remainingSeconds
    ) const;

    PricingResult exact(
        const std::vector<double>& customerDuals,
        const std::vector<double>& dayDuals,
        const std::vector<BranchDecision>& decisions,
        double remainingSeconds
    ) const;

private:
    const Instance& inst_;
};
