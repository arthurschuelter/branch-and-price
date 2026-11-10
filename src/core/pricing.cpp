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
    if (remainingSeconds <= 0.0) {
        return {PricingStatus::Interrupted, {}};
    }

    const auto start = std::chrono::steady_clock::now();
    const int n = inst_.vertexCount();
    const int depot = inst_.depot;
    if (customerDuals.size() != static_cast<std::size_t>(n) ||
        dayDuals.size() != static_cast<std::size_t>(n)) {
        return {PricingStatus::Error, {}};
    }
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(customerDuals[i]) || !std::isfinite(dayDuals[i])) {
            return {PricingStatus::Error, {}};
        }
    }

    GRBEnv env;
    GRBModel model(env);
    model.set(GRB_DoubleParam_MIPGap, 0.0);
    model.set(GRB_DoubleParam_MIPGapAbs, 0.0);

    // y[i] = se o cliente i pertence à rota.
    std::vector<GRBVar> y(n);
    for (int i = 0; i < n; ++i) {
        if (i == depot) {
            continue;
        }
        y[i] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
    }

    // x[i][j] = se a rota percorre o arco i -> j.
    std::vector<std::vector<GRBVar>> x(n, std::vector<GRBVar>(n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            x[i][j] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
        }
    }

    std::vector<std::vector<GRBVar>> flow(n, std::vector<GRBVar>(n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            flow[i][j] = model.addVar(0.0, n - 1, 0.0, GRB_CONTINUOUS);
        }
    }

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            model.addConstr(flow[i][j] <= (n - 1) * x[i][j]);
        }
    }

    for (int i = 0; i < n; ++i) {
        if (i == depot) {
            continue;
        }

        GRBLinExpr incomingFlow = 0.0;
        GRBLinExpr outgoingFlow = 0.0;
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            incomingFlow += flow[j][i];
            outgoingFlow += flow[i][j];
        }
        model.addConstr(incomingFlow - outgoingFlow == y[i]);
    }

    for (int i = 0; i < n; ++i) {
        if (i == depot) {
            continue;
        }

        GRBLinExpr incoming = 0.0;
        GRBLinExpr outgoing = 0.0;
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            incoming += x[j][i];
            outgoing += x[i][j];
        }

        model.addConstr(incoming == y[i]);
        model.addConstr(outgoing == y[i]);
    }

    GRBLinExpr departure = 0.0;
    GRBLinExpr arrival = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i == depot) {
            continue;
        }
        departure += x[depot][i];
        arrival += x[i][depot];
    }

    model.addConstr(departure == 1);
    model.addConstr(arrival == 1);

    GRBLinExpr load = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i == depot) {
            continue;
        }
        load += inst_.vertices[i].weight * y[i];
    }
    model.addConstr(load <= inst_.capacity);

    for (const auto& conflict : inst_.conflicts) {
        const int i = conflict.first;
        const int j = conflict.second;
        if (i == depot || j == depot) {
            continue;
        }
        model.addConstr(y[i] + y[j] <= 1);
    }

    for (const auto& decision : decisions) {
        const int i = decision.i;
        const int j = decision.j;
        if (i < 0 || i >= n || j < 0 || j >= n ||
            i == depot || j == depot || i == j) {
            return {PricingStatus::Error, {}};
        }

        if (decision.same) {
            model.addConstr(y[i] == y[j]);
        } else {
            model.addConstr(y[i] + y[j] <= 1);
        }
    }

    constexpr double eps = 1e-6;
    std::vector<Column> columns;
    std::vector<std::optional<double>> dayLowerBounds(n);
    for (int day = 1; day < n; ++day) {
        GRBLinExpr objective = -dayDuals[day];
        for (int i = 0; i < n; ++i) {
            if (i != depot) {
                objective += (inst_.vertices[i].priorities[day - 1] -
                              customerDuals[i]) * y[i];
            }
            for (int j = 0; j < n; ++j) {
                if (i != j) objective += inst_.travel_costs[i][j] * x[i][j];
            }
        }
        model.setObjective(objective, GRB_MINIMIZE);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        const double remaining = remainingSeconds - elapsed;
        if (remaining <= 0.0) {
            return {PricingStatus::Interrupted, std::move(columns), std::move(dayLowerBounds)};
        }
        model.set(GRB_DoubleParam_TimeLimit, remaining);
        model.optimize();

        const int status = model.get(GRB_IntAttr_Status);
        if (status == GRB_TIME_LIMIT) {
            return {PricingStatus::Interrupted, std::move(columns), std::move(dayLowerBounds)};
        }

        if (status == GRB_INFEASIBLE) {
            // Não existe rota admissível neste dia; zero não exige correção.
            dayLowerBounds[day] = 0.0;
            continue;
        }
        
        if (status != GRB_OPTIMAL) {
            return {
                PricingStatus::Error, 
                {}
            };
        }

        const double objectiveValue = model.get(GRB_DoubleAttr_ObjVal);
        const double bound = model.get(GRB_DoubleAttr_ObjBound);
        if (std::isfinite(bound)) dayLowerBounds[day] = bound;
        if (objectiveValue >= -eps) {
            if (!std::isfinite(bound) || bound < -eps) {
                return {
                    PricingStatus::Inconclusive, 
                    std::move(columns), std::move(dayLowerBounds)
                };
            }
            continue;
        }

        Column column{};
        column.day = day;
        column.has.assign(n, false);
        int current = depot;
        for (int steps = 0; steps < n; ++steps) {
            int next = -1;
            for (int j = 0; j < n; ++j) {
                if (j != current && x[current][j].get(GRB_DoubleAttr_X) > 0.5) {
                    if (next != -1) return {PricingStatus::Error, {}};
                    next = j;
                }
            }
            if (next == -1) return {PricingStatus::Error, {}};
            if (next == depot) break;
            if (column.has[next]) return {PricingStatus::Error, {}};
            column.has[next] = true;
            column.route.push_back(next);
            current = next;
        }
        for (int i = 0; i < n; ++i) {
            if (i != depot && (y[i].get(GRB_DoubleAttr_X) > 0.5) != column.has[i]) {
                return {PricingStatus::Error, {}};
            }
        }
        const auto evaluation = inst_.evaluateRoute(column.route, day);
        if (!evaluation.valid) return {PricingStatus::Error, {}};
        for (const auto& decision : decisions) {
            if ((decision.same && column.has[decision.i] != column.has[decision.j]) ||
                (!decision.same && column.has[decision.i] && column.has[decision.j])) {
                return {PricingStatus::Error, {}};
            }
        }
        column.cost = evaluation.total_cost;
        double reducedCost = column.cost - dayDuals[day];
        for (int customer : column.route) reducedCost -= customerDuals[customer];
        if (!std::isfinite(reducedCost) || reducedCost >= -eps ||
            std::abs(reducedCost - model.get(GRB_DoubleAttr_ObjVal)) > eps) {
            return {PricingStatus::Error, {}};
        }
        columns.push_back(std::move(column));
    }
    return {PricingStatus::Completed, std::move(columns), std::move(dayLowerBounds)};
}
