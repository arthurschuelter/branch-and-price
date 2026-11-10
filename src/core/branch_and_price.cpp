#include "branch_and_price.hpp"
#include "pricing.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <functional>
// #include "instance.hpp"
// #include "solver_rmp.hpp"

BranchAndPrice::BranchAndPrice(const Instance& inst, const Config& cfg)
    : inst_(inst), 
      cfg_(cfg), 
      rmp_(std::make_shared<RMPSolver>(inst)),
      pricer_(std::make_shared<Pricer>(inst)) {
        this->pool_ = std::vector<Column>();
        this->open_ = std::deque<std::unique_ptr<Node>>();
}

BranchAndPrice::~BranchAndPrice() = default;

Solution BranchAndPrice::buildInitialSolution() const { 
    Solution solution;
    solution.cost = 0.0;

    int day = 1;
    for (int v = 0; v < inst_.vertexCount(); ++v) {
        if (v == inst_.depot) { continue; }

        const Column* selected = nullptr;

        for (const Column& column : pool_) {
            if (column.day == day &&
                column.route.size() == 1 &&
                column.route[0] == v) {
                selected = &column;
                break;
            }
        }
        if (selected == nullptr) {
            throw std::runtime_error("Coluna unitária inicial não encontrada");
        }

        const auto evaluation = inst_.evaluateRoute(selected->route, selected->day);
        if (!evaluation.valid) {
            throw std::runtime_error("Rota inicial inválida");
        }
        if (!std::isfinite(selected->cost) ||
            std::abs(selected->cost - evaluation.total_cost) > 1e-6) {
            throw std::runtime_error("Custo da coluna inicial inconsistente");
        }

        solution.columns.push_back(*selected);
        solution.cost += evaluation.total_cost;
        
        ++day;
    }

    return solution;
}

Solution BranchAndPrice::solve() {
    t0_ = std::chrono::steady_clock::now();
    stats_ = Stats{};
    incumbent_ = Solution{};
    global_lb_ = 0.0;
    initRoot();
    
    Solution initial = buildInitialSolution();
    updateIncumbent(initial);

    // this->printOpen();
    // this->printPool();

    while (!open_.empty() && !timeIsUp()) {
        auto node = selectNode();

        if (node->lb >= incumbent_.cost) {
            updateGlobalLB();
            continue;
        }

        stats_.nodes++;
        std::vector<std::size_t> columnIndices;
        const NodeResult result = processNode(*node, columnIndices);   // resolve o nó (geração de colunas)
        logNode(*node);

        if (result == NodeResult::TimeLimit || result == NodeResult::RestrictedMasterInfeasible ||
            result == NodeResult::PricingInterrupted || result == NodeResult::PricingInconclusive ||
            result == NodeResult::NoCertification || result == NodeResult::DuplicateColumns) {
            const char* reason = result == NodeResult::TimeLimit ? "tempo esgotado" :
                result == NodeResult::RestrictedMasterInfeasible ? "PMR inviável" :
                result == NodeResult::PricingInterrupted ? "pricing interrompido" :
                result == NodeResult::PricingInconclusive ? "pricing inconclusivo" :
                result == NodeResult::DuplicateColumns ? "colunas duplicadas" : "sem certificação";
            std::cerr << "Nó não resolvido: " << reason << '\n';
            pushNode(std::move(node));
            updateGlobalLB();
            break;
        }

        if (result == NodeResult::Infeasible) {
            updateGlobalLB();
            continue;
        }

        if (result == NodeResult::Error) {
            throw std::runtime_error("Erro ao processar nó");
        }
        if (result != NodeResult::Converged) {
            throw std::runtime_error("Resultado inesperado ao processar nó");
        }

        if (node->lb >= incumbent_.cost) {
            updateGlobalLB();
            continue;
        }

        if (isIntegral()) {
            // Sem igualdade entre UB e LB certificado não é seguro fechar o nó.
            pushNode(std::move(node));
            updateGlobalLB();
            break;
        }

        int i, j;
        if (selectBranchingPair(columnIndices, i, j)) {
            branch(*node, i, j);
        } else {
            const auto solution = assignBlocks(columnIndices);
            if (solution) updateIncumbent(*solution);
            if (node->lb >= incumbent_.cost) {
                updateGlobalLB();
                continue;
            }
            // Uma incumbente isolada não certifica o fechamento do nó.
            pushNode(std::move(node));
            updateGlobalLB();
            break;
        }

        updateGlobalLB();
    }

    // Tempo acabou (ou árvore esgotada)
    updateGlobalLB();
    if (!open_.empty() && cfg_.heuristic_time > 0.0) roundingHeuristic();
    updateGlobalLB();
    stats_.time_total = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0_).count();
    return incumbent_;
}

// Diagnóstico provisório do PMR usando o mapeamento de colunas inseridas.
std::optional<Solution> BranchAndPrice::validateInitialRmp(
    const RMPSolver& rmp, 
    const std::vector<std::size_t>& columnIndices
) const {
    constexpr double eps = 1e-6;
    const int n = inst_.vertexCount();
    const double lpObjective = rmp.objective();
    const auto x = rmp.values();
    const auto u = rmp.customerDuals();
    const auto lambda = rmp.dayDuals();

    if (!std::isfinite(lpObjective)) {
        throw std::runtime_error("Objetivo do PMR não finito");
    }
    if (x.size() != columnIndices.size()) {
        throw std::runtime_error(
            "Quantidade de valores diferente da quantidade de colunas"
        );
    }

    if (u.size() != static_cast<std::size_t>(n) ||
        lambda.size() != static_cast<std::size_t>(n)) {
        throw std::runtime_error("Tamanho incorreto dos vetores de duais");
    }
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(u[i]) || !std::isfinite(lambda[i])) {
            throw std::runtime_error("Dual não finito");
        }
    }
    if (std::abs(u[inst_.depot]) > eps || std::abs(lambda[0]) > eps) {
        throw std::runtime_error("Dual não utilizado deve ser zero");
    }

    double dualObjective = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i == inst_.depot) continue;
        std::cerr << "u[" << i << "] = " << u[i] << '\n';
        dualObjective += u[i];
    }
    for (int d = 1; d < n; ++d) {
        std::cerr << "lambda[" << d << "] = " << lambda[d] << '\n';
        if (lambda[d] > eps) {
            throw std::runtime_error("Dual de dia positivo");
        }
        dualObjective += lambda[d];
    }
    if (!std::isfinite(dualObjective) ||
        std::abs(dualObjective - lpObjective) > eps) {
        throw std::runtime_error("Objetivos primal e dual do PMR diferem");
    }

    bool integral = true;
    for (std::size_t k = 0; k < x.size(); ++k) {
        if (!std::isfinite(x[k]) || x[k] < -eps || x[k] > 1.0 + eps) {
            throw std::runtime_error("Valor de variável do PMR inválido");
        }
        if (std::abs(x[k]) > eps && std::abs(x[k] - 1.0) > eps) {
            integral = false;
        }

        const auto& column = pool_[columnIndices[k]];

        const double rc = reducedCost(column, u, lambda);
        if (!std::isfinite(rc) || rc < -eps) {
            throw std::runtime_error("Custo reduzido negativo no PMR ótimo");
        }
        if (x[k] > eps && std::abs(rc) > eps) {
            throw std::runtime_error("Complementaridade violada no PMR");
        }
    }
    std::cerr << "PMR: objetivo primal = " << lpObjective
              << ", dual = " << dualObjective
              << "; custos reduzidos conferidos em " << x.size()
              << " colunas (sem certificação do mestre completo).\n";

    for (int v = 0; v < this->inst_.vertexCount(); ++v) {
        if (v == this->inst_.depot) { continue; }

        double sum = 0.0;
        for (std::size_t k = 0; k < x.size(); ++k) {
            if (this->pool_[columnIndices[k]].has[v]) {
                sum += x[k];
            }
        }

        if (std::abs(sum - 1.0) > 1e-6) {
            throw std::runtime_error("Cobertura incorreta de cliente");
        }
    }

    for (int d = 1; d < this->inst_.vertexCount(); ++d) {
        double sum = 0.0;
        for (std::size_t k = 0; k < x.size(); ++k) {
            if (this->pool_[columnIndices[k]].day == d) {
                sum += x[k];
            }
        }

        if (sum > 1.0 + 1e-6) {
            throw std::runtime_error("Limite de rotas por dia violado");
        }
    }

    double objective = 0.0;
    for (std::size_t k = 0; k < x.size(); ++k) {
        objective += this->pool_[columnIndices[k]].cost * x[k];
    }

    if (!std::isfinite(objective) || std::abs(objective - lpObjective) > eps) {
        throw std::runtime_error("Objetivo do PMR inconsistente");
    }

    if (!integral) {
        return std::nullopt;
    }

    Solution solution;
    solution.cost = 0.0;
    std::vector<int> coverage(n, 0);
    std::vector<bool> usedDays(n, false);
    for (std::size_t k = 0; k < x.size(); ++k) {
        if (x[k] > 0.5) {
            const auto& column = pool_[columnIndices[k]];
            const auto evaluation = inst_.evaluateRoute(column.route, column.day);
            if (!evaluation.valid) {
                throw std::runtime_error("Rota inválida na solução inteira");
            }
            if (!std::isfinite(column.cost) ||
                std::abs(column.cost - evaluation.total_cost) > eps) {
                throw std::runtime_error("Custo de coluna difere do custo da rota");
            }
            if (usedDays[column.day]) {
                throw std::runtime_error("Dia repetido na solução inteira");
            }
            usedDays[column.day] = true;
            for (int customer : column.route) ++coverage[customer];
            solution.cost += evaluation.total_cost;


            solution.columns.push_back(column);
        }
    }

    for (int i = 0; i < n; ++i) {
        if (coverage[i] != (i == inst_.depot ? 0 : 1)) {
            throw std::runtime_error("Cobertura incorreta na solução inteira");
        }
    }
    if (std::abs(solution.cost - lpObjective) > eps) {
        throw std::runtime_error("Custo da solução inteira difere do objetivo LP");
    }
    return solution;
}

double BranchAndPrice::initialRoutingLowerBound() const {
    // Saturar em inteiro exatamente representável evita arredondar o LB para cima.
    constexpr double cap = 4503599627370496.0; // 2^52
    double bound = 0.0;
    const auto add = [&](int value) {
        bound = (bound >= cap - static_cast<double>(value))
            ? cap : bound + static_cast<double>(value);
    };
    const int n = inst_.vertexCount();
    if (n <= 1) return 0.0;
    int depotMin = std::numeric_limits<int>::max();
    for (int i = 0; i < n; ++i) {
        if (i == inst_.depot) continue;
        depotMin = std::min(depotMin, inst_.travel_costs[inst_.depot][i]);
        int minOutgoing = std::numeric_limits<int>::max();
        for (int j = 0; j < n; ++j) {
            if (i != j) minOutgoing = std::min(minOutgoing, inst_.travel_costs[i][j]);
        }
        add(inst_.vertices[i].priorities[0]);
        add(minOutgoing);
    }
    add(depotMin);
    return bound;
}

void BranchAndPrice::initRoot() {
    this->pool_.clear();
    this->open_.clear();
    next_node_id_ = 1;

    for (int customer = 0; customer < inst_.vertexCount(); ++customer) {
        if (inst_.depot == customer) continue;

        for (int day = 1; day < inst_.vertexCount(); ++day) {
            Column column{};

            column.route.push_back(customer);

            column.has = std::vector<bool>(inst_.vertexCount(), false);
            column.has[customer] = true;

            column.day = day;

            RouteEvaluation route = inst_.evaluateRoute(column.route, column.day);
            if (!route.valid) continue;

            column.cost = route.total_cost;

            this->pool_.push_back(column);
        }
    }

    if (this->cfg_.use_preprocessing) {
        // TODO: Implementar pré-processamento.
    }

    auto root = std::make_unique<Node>();
    root->id = 0;
    root->depth = 0;
    root->lb = initialRoutingLowerBound();
    global_lb_ = root->lb;
    stats_.root_lb = root->lb;

    this->pushNode(std::move(root));
}

NodeResult BranchAndPrice::processNode(Node& node, std::vector<std::size_t>& columnIndices) {
    // O PMR inviável não prova inviabilidade do nó; não atualizar node.lb sem certificação.
    columnIndices.clear();

    std::vector<int> initialGroup(inst_.vertexCount());
    for (int i = 0; i < inst_.vertexCount(); ++i) {
        initialGroup[i] = i;
    }

    for (const auto& decision : node.decisions) {
        if (!decision.same) { continue; }

        int groupI = initialGroup[decision.i];
        int groupJ = initialGroup[decision.j];

        for (int& g : initialGroup) {
            if (g == groupJ) { g = groupI; }
        }
    }

    for (const auto& decision : node.decisions) {
        if (decision.same) { continue; }

        if (initialGroup[decision.i] == initialGroup[decision.j]) {
            std::cerr << "Nó inviável: DIFFERENT dentro de grupo SAME\n";
            return NodeResult::Infeasible;
        }
    }

    std::vector<std::vector<int>> groups(inst_.vertexCount());
    for (int i = 0; i < inst_.vertexCount(); ++i) {
        if (i == inst_.depot) { continue; }

        groups[initialGroup[i]].push_back(i);
    }

    for (const auto& group : groups) {
        if (group.empty()) continue;

        const auto evaluation = inst_.evaluateRoute(group, 1);
        if (!evaluation.valid) {
            std::cerr << "Grupo SAME inválido: capacidade ou conflito\n";
            return NodeResult::Infeasible;
        }

        for (int d = 1; d < inst_.vertexCount(); ++d){
            Column col;
            
            col.route = group;
            col.day = d;
            col.has = std::vector<bool>(inst_.vertexCount(), false);
            for (int i: group) {
                col.has[i] = true;
            }
            col.cost = inst_.evaluateRoute(col.route, d).total_cost;

            if (!findPoolColumn(col)) {
                pool_.push_back(col);
            }
        }
    }

    try {
        columnIndices.reserve(pool_.size());
        rmp_->reset();
        for (std::size_t i = 0; i < pool_.size(); ++i) {
            if (!columnAllowed(pool_[i], node)) { continue; }

            rmp_->addColumn(pool_[i]);
            columnIndices.push_back(i);
        }
    } catch (const GRBException& e) {
        std::cerr << "Erro Gurobi na montagem do PMR: " << e.getMessage() << '\n';
        rmp_->discard();
        columnIndices.clear();
        return NodeResult::Error;
    } catch (const std::exception& e) {
        std::cerr << "Erro na montagem do PMR: " << e.what() << '\n';
        rmp_->discard();
        columnIndices.clear();
        return NodeResult::Error;
    }

    return columnGeneration(node, columnIndices);
}

NodeResult BranchAndPrice::columnGeneration(Node& node, std::vector<std::size_t>& columnIndices) {
    constexpr double eps = 1e-6;
    auto remaining = [&]() {
        return cfg_.time_limit - std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0_).count();
    };
    try {
        while (remaining() > 0.0) {
            const int status = rmp_->solveLP(remaining());
            if (status == GRB_TIME_LIMIT) return NodeResult::TimeLimit;
            if (status == GRB_INFEASIBLE) return NodeResult::RestrictedMasterInfeasible;

            if (status != GRB_OPTIMAL) {
                return NodeResult::Error;
            }

            const auto solution = validateInitialRmp(*rmp_, columnIndices);
            if (solution) {
                updateIncumbent(*solution);
            }

            const auto u = rmp_->customerDuals();
            const auto lambda = rmp_->dayDuals();

            auto insert = [&](const PricingResult& result) {
                int added = 0;
                for (const auto& c : result.columns) {

                    const auto evaluation = inst_.evaluateRoute(c.route, c.day);
                    if (!evaluation.valid) {
                        throw std::runtime_error("Rota de pricing inválida");
                    }
                    std::vector<bool> has(inst_.vertexCount(), false);
                    for (int customer : c.route) {
                        has[customer] = true;
                    }
                    
                    if (has != c.has || !columnAllowed(c, node) || !std::isfinite(c.cost) ||
                        std::abs(c.cost - evaluation.total_cost) > eps) {
                        throw std::runtime_error("Coluna de pricing inválida");
                    }
                    const double rc = reducedCost(c, u, lambda);
                    if (!std::isfinite(rc) || rc >= -eps) {
                        throw std::runtime_error("Coluna de pricing inválida");
                    }
                    const auto existing = findPoolColumn(c);
                    if (existing) {
                        bool inRmp = false;
                        for (std::size_t index : columnIndices) {
                            if (index == *existing) { 
                                inRmp = true; 
                                break; 
                            }
                        }

                        if (inRmp) continue;
                    }

                    columnIndices.reserve(columnIndices.size() + 1);
                    const bool newColumn = !existing;
                    if (newColumn) pool_.push_back(c);
                    const std::size_t index = existing ? *existing : pool_.size() - 1;
                    try {
                        rmp_->addColumn(pool_[index]);
                    } catch (...) {
                        if (newColumn) { 
                            pool_.pop_back();
                        }
                        throw;
                    }
                    columnIndices.push_back(index);
                    ++added;
                    ++stats_.columns_generated;
                }
                return added;
            };

            if (cfg_.use_heuristic_pricing && remaining() > 0.0) {
                ++stats_.heuristic_pricing_calls;
                auto result = pricer_->heuristic(u, lambda, node.decisions, remaining());
                if (result.status == PricingStatus::Error) {
                    return NodeResult::Error;
                }
                
                const int added = insert(result);
                
                if (result.status != PricingStatus::Completed) {
                    return result.status == PricingStatus::Interrupted
                        ? NodeResult::PricingInterrupted : NodeResult::PricingInconclusive;
                }

                if (added) {
                    continue;
                }
            }
            if (remaining() <= 0.0) {
                return NodeResult::TimeLimit;
            }

            ++stats_.exact_pricing_calls;
            auto result = pricer_->exact(u, lambda, node.decisions, remaining());
            if (result.status == PricingStatus::Error) {
                return NodeResult::Error;
            }

            const int added = insert(result);
            if (result.status != PricingStatus::Completed) {
                return result.status == PricingStatus::Interrupted
                    ? NodeResult::PricingInterrupted : NodeResult::PricingInconclusive;
            }

            if (added) {
                continue;
            }
            
            if (!result.columns.empty()) {
                std::cerr << "Pricing exato retornou apenas colunas já presentes.\n";
                return NodeResult::DuplicateColumns;
            }

            const auto candidate = diagnosticLowerBound(result, u, lambda);
            if (candidate) {
                std::ostringstream value;
                value << std::setprecision(std::numeric_limits<double>::max_digits10)
                      << *candidate;
                std::cerr << "Candidata a LB ajustado (não certificada): "
                          << value.str() << '\n';
            } else {
                std::cerr << "Limites por dia incompletos; LB ajustado indisponível.\n";
            }

            // A certificação e o fechamento ainda dependem da validação independente
            // do pricing e das tolerâncias numéricas. Manter o nó aberto.
            std::cerr << "Pricing concluído sem novas colunas; nó permanece aberto enquanto a certificação é validada.\n";
            return NodeResult::NoCertification;
        }
    } catch (const GRBException& e) {
        std::cerr << "Erro Gurobi na geração de colunas: " << e.getMessage() << '\n';
        rmp_->discard();
        columnIndices.clear();
        return NodeResult::Error;
    } catch (const std::exception& e) {
        std::cerr << "Erro na geração de colunas: " << e.what() << '\n';
        rmp_->discard();
        columnIndices.clear();
        return NodeResult::Error;
    }
    return NodeResult::TimeLimit;
}

std::optional<double> BranchAndPrice::diagnosticLowerBound(
    const PricingResult& result,
    const std::vector<double>& customerDuals,
    const std::vector<double>& dayDuals
) const {
    constexpr double eps = 1e-6;
    const auto n = static_cast<std::size_t>(inst_.vertexCount());
    if (result.status != PricingStatus::Completed || !result.columns.empty() ||
        result.dayLowerBounds.size() != n || customerDuals.size() != n ||
        dayDuals.size() != n) return std::nullopt;

    double candidate = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (i == static_cast<std::size_t>(inst_.depot)) continue;
        if (!std::isfinite(customerDuals[i])) return std::nullopt;
        candidate += customerDuals[i];
    }
    for (std::size_t d = 1; d < n; ++d) {
        const auto& bound = result.dayLowerBounds[d];
        if (!bound || !std::isfinite(*bound) || !std::isfinite(dayDuals[d])) {
            return std::nullopt;
        }
        candidate += std::min(dayDuals[d], 0.0) + std::min(0.0, *bound - eps);
    }
    if (!std::isfinite(candidate)) return std::nullopt;
    return candidate;
}

std::optional<std::size_t> BranchAndPrice::findPoolColumn(const Column& column) const {
    for (std::size_t i = 0; i < pool_.size(); ++i) {
        if (pool_[i].day == column.day && pool_[i].route == column.route) return i;
    }
    return std::nullopt;
}

bool BranchAndPrice::columnAllowed(const Column& c, const Node& node) const {
    for (const auto& d : node.decisions) {
        bool hasI = c.has[d.i];
        bool hasJ = c.has[d.j];

        if (d.same && hasI != hasJ){ return false; }
        if (!d.same && hasI && hasJ){ return false; }
    }
    return true;
}

// ---------------------------------------------------------------
// Ramificação
// ---------------------------------------------------------------
bool BranchAndPrice::isIntegral(double eps) const {
    const auto values = rmp_->values();
    
    for (double v: values) {
        if (!std::isfinite(v)) { throw std::runtime_error("Valor infinito no PMR"); }

        const bool nearZero = std::abs(v) <= eps;
        const bool nearOne = std::abs(v - 1.0) <= eps;

        if (!nearZero && !nearOne) { 
            return false; 
        }
    }

    return true;
}

bool BranchAndPrice::selectBranchingPair(
    const std::vector<std::size_t>& columnIndices, int& i, int& j
) const {
    constexpr double eps = 1e-6;
    const auto values = rmp_->values();
    if (values.size() != columnIndices.size()) {
        throw std::runtime_error("Mapeamento de colunas incompatível com o PMR");
    }
    const int n = inst_.vertexCount();
    std::vector<std::vector<double>> fractions(n, std::vector<double>(n, 0.0));
    for (std::size_t k = 0; k < values.size(); ++k) {
        const double x = values[k];
        if (!std::isfinite(x) || x < -eps || x > 1.0 + eps || columnIndices[k] >= pool_.size()) {
            throw std::runtime_error("Valor ou índice de coluna inválido na ramificação");
        }
        const auto& column = pool_[columnIndices[k]];
        if (column.has.size() != static_cast<std::size_t>(n)) {
            throw std::runtime_error("Coluna inválida na ramificação");
        }
        for (int a = 0; a < n; ++a) {
            if (a == inst_.depot || !column.has[a]) continue;
            for (int b = a + 1; b < n; ++b) {
                if (b != inst_.depot && column.has[b]) fractions[a][b] += x;
            }
        }
    }
    bool found = false;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (int a = 0; a < n; ++a) {
        if (a == inst_.depot) continue;
        for (int b = a + 1; b < n; ++b) {
            if (b == inst_.depot) continue;
            const double fraction = fractions[a][b];
            if (!std::isfinite(fraction) || fraction < -eps || fraction > 1.0 + eps) {
                throw std::runtime_error("Fração de par inválida no PMR");
            }
            if (fraction <= eps || fraction >= 1.0 - eps) continue;
            const double distance = std::abs(fraction - 0.5);
            if (!found || distance < bestDistance) {
                found = true;
                bestDistance = distance;
                i = a;
                j = b;
            }
        }
    }
    return found;
}

std::optional<Solution> BranchAndPrice::assignBlocks(
    const std::vector<std::size_t>& columnIndices
) const {
    constexpr double eps = 1e-6;
    const auto values = rmp_->values();
    if (values.size() != columnIndices.size()) return std::nullopt;
    const int n = inst_.vertexCount();
    std::vector<std::vector<int>> blocks;
    for (std::size_t k = 0; k < values.size(); ++k) {
        if (!std::isfinite(values[k]) || values[k] < -eps || columnIndices[k] >= pool_.size())
            return std::nullopt;
        if (values[k] <= eps) continue;
        auto block = pool_[columnIndices[k]].route;
        std::sort(block.begin(), block.end());
        if (block.empty() || std::adjacent_find(block.begin(), block.end()) != block.end())
            return std::nullopt;
        if (std::find(blocks.begin(), blocks.end(), block) == blocks.end())
            blocks.push_back(std::move(block));
    }
    std::vector<int> coverage(n, 0);
    for (const auto& block : blocks) {
        for (int customer : block) {
            if (customer < 0 || customer >= n || customer == inst_.depot) return std::nullopt;
            if (++coverage[customer] != 1) return std::nullopt;
        }
    }
    for (int i = 0; i < n; ++i)
        if (coverage[i] != (i == inst_.depot ? 0 : 1)) return std::nullopt;

    // Para cada bloco e dia, guardar a rota de menor custo disponível no PMR.
    std::vector<std::vector<const Column*>> choices(blocks.size(), std::vector<const Column*>(n, nullptr));
    for (std::size_t index : columnIndices) {
        if (index >= pool_.size()) return std::nullopt;
        const auto& column = pool_[index];
        if (column.day < 1 || column.day >= n) return std::nullopt;
        auto key = column.route;
        std::sort(key.begin(), key.end());
        const auto it = std::find(blocks.begin(), blocks.end(), key);
        if (it == blocks.end()) continue;
        auto& choice = choices[static_cast<std::size_t>(it - blocks.begin())][column.day];
        if (!choice || column.cost < choice->cost) choice = &column;
    }
    Solution best;
    std::vector<bool> usedDays(n, false);
    std::vector<Column> selected;
    std::function<void(std::size_t, double)> visit = [&](std::size_t k, double cost) {
        if (timeIsUp() || cost >= best.cost) return;
        if (k == blocks.size()) {
            best.cost = cost;
            best.columns = selected;
            return;
        }
        for (int day = 1; day < n; ++day) {
            const auto* column = choices[k][day];
            if (!column || usedDays[day]) continue;
            const auto evaluation = inst_.evaluateRoute(column->route, day);
            if (!evaluation.valid || !std::isfinite(column->cost) ||
                std::abs(column->cost - evaluation.total_cost) > eps) continue;
            usedDays[day] = true;
            selected.push_back(*column);
            visit(k + 1, cost + column->cost);
            selected.pop_back();
            usedDays[day] = false;
        }
    };
    visit(0, 0.0);
    if (best.cost >= 1e18) return std::nullopt;
    return best;
}

void BranchAndPrice::branch(const Node& node, int i, int j) {
    const int n = inst_.vertexCount();
    if (i < 0 || i >= n || j < 0 || j >= n || i == j ||
        i == inst_.depot || j == inst_.depot) {
        throw std::invalid_argument("Par de ramificação inválido");
    }
    if (next_node_id_ > std::numeric_limits<int>::max() - 2 ||
        node.depth == std::numeric_limits<int>::max()) {
        throw std::overflow_error("Limite de IDs ou profundidade da árvore atingido");
    }

    auto same = std::make_unique<Node>(node);
    auto different = std::make_unique<Node>(node);
    same->id = next_node_id_;
    different->id = next_node_id_ + 1;
    same->depth = different->depth = node.depth + 1;
    same->decisions.push_back({i, j, true});
    different->decisions.push_back({i, j, false});

    // Inserir DIFFERENT primeiro para visitar SAME primeiro em DFS.
    pushNode(std::move(different));
    try {
        pushNode(std::move(same));
    } catch (...) {
        open_.pop_back();
        throw;
    }
    next_node_id_ += 2;
}

// ---------------------------------------------------------------
// Seleção de nós
// ---------------------------------------------------------------
std::unique_ptr<Node> BranchAndPrice::selectNode() {
    std::unique_ptr<Node> node;
    
    if (cfg_.search == SearchStrategy::DFS) {
        node = std::move(this->open_.back());
        this->open_.pop_back();
    }

    else if (cfg_.search == SearchStrategy::BEST_BOUND) {
        auto best = std::min_element(open_.begin(), open_.end(),
            [](const auto& a, const auto& b) { return a->lb < b->lb; });
        node = std::move(*best);
        open_.erase(best);
    }
    return node;
}

void BranchAndPrice::pushNode(std::unique_ptr<Node> n) {
    this->open_.push_back(std::move(n));
}

void BranchAndPrice::updateGlobalLB() {
    if (open_.empty()) {
        global_lb_ = incumbent_.cost;
        return;
    }
    double bound = incumbent_.cost;
    for (const auto& node : open_) bound = std::min(bound, node->lb);
    global_lb_ = std::max(0.0, bound);
}

// ---------------------------------------------------------------
// Heurística e utilidades
// ---------------------------------------------------------------
void BranchAndPrice::roundingHeuristic() {
    const auto start = std::chrono::steady_clock::now();
    const auto remaining = [&]() {
        const auto now = std::chrono::steady_clock::now();
        const double heuristicLeft = cfg_.heuristic_time - std::chrono::duration<double>(now - start).count();
        const double totalLeft = cfg_.time_limit + cfg_.heuristic_time -
            std::chrono::duration<double>(now - t0_).count();
        return std::min(heuristicLeft, totalLeft);
    };
    if (!std::isfinite(cfg_.heuristic_time) || cfg_.heuristic_time <= 0.0) return;
    try {
        GRBEnv env;
        GRBModel model(env);
        model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
        const int n = inst_.vertexCount();
        std::vector<GRBLinExpr> coverage(n), days(n);
        std::vector<GRBVar> variables;
        std::vector<std::size_t> indices;
        variables.reserve(pool_.size());
        indices.reserve(pool_.size());
        for (std::size_t k = 0; k < pool_.size(); ++k) {
            if (remaining() <= 0.0) return;
            const auto& c = pool_[k];
            const auto evaluation = inst_.evaluateRoute(c.route, c.day);
            if (!evaluation.valid || !std::isfinite(c.cost) ||
                std::abs(c.cost - evaluation.total_cost) > 1e-6) continue;
            const auto var = model.addVar(0.0, 1.0, c.cost, GRB_BINARY);
            variables.push_back(var);
            indices.push_back(k);
            for (int customer : c.route) coverage[customer] += var;
            days[c.day] += var;
        }
        for (int i = 0; i < n; ++i) {
            if (remaining() <= 0.0) return;
            if (i != inst_.depot) model.addConstr(coverage[i] == 1);
        }
        for (int day = 1; day < n; ++day) {
            if (remaining() <= 0.0) return;
            model.addConstr(days[day] <= 1);
        }
        if (remaining() <= 0.0) return;
        model.set(GRB_DoubleParam_TimeLimit, remaining());
        model.optimize();
        if (remaining() <= 0.0 || model.get(GRB_IntAttr_SolCount) == 0) return;

        Solution solution;
        solution.cost = 0.0;
        std::vector<int> seen(n, 0);
        std::vector<bool> usedDays(n, false);
        for (std::size_t k = 0; k < variables.size(); ++k) {
            if (remaining() <= 0.0) return;
            const double value = variables[k].get(GRB_DoubleAttr_X);
            if (!std::isfinite(value) || (std::abs(value) > 1e-6 &&
                std::abs(value - 1.0) > 1e-6)) return;
            if (value <= 0.5) continue;
            const auto& c = pool_[indices[k]];
            const auto evaluation = inst_.evaluateRoute(c.route, c.day);
            if (!evaluation.valid || usedDays[c.day] ||
                std::abs(c.cost - evaluation.total_cost) > 1e-6) return;
            usedDays[c.day] = true;
            for (int customer : c.route) ++seen[customer];
            solution.cost += evaluation.total_cost;
            solution.columns.push_back(c);
        }
        for (int i = 0; i < n; ++i)
            if (seen[i] != (i == inst_.depot ? 0 : 1)) return;
        if (remaining() <= 0.0) return;
        updateIncumbent(solution);
    } catch (const GRBException& e) {
        std::cerr << "Erro Gurobi no arredondamento: " << e.getMessage() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Erro no arredondamento: " << e.what() << '\n';
    }
}

bool BranchAndPrice::timeIsUp() const {
    std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - this->t0_;
        
    if (elapsed.count() >= this->cfg_.time_limit) { return true; }
    
    return false;
}

void BranchAndPrice::updateIncumbent(const Solution& s) {
    if (std::isfinite(s.cost) && s.cost < incumbent_.cost - 1e-6) {
        incumbent_ = s;
    }
}

double BranchAndPrice::reducedCost(
    const Column& column,
    const std::vector<double>& customerDuals,
    const std::vector<double>& dayDuals
) const {
    double cost = column.cost - dayDuals[column.day];

    for (int customer : column.route) {
        cost -= customerDuals[customer];
    }

    return cost;
}

void BranchAndPrice::logNode(const Node& node) const {
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0_
    ).count();

    std::cerr << "no=" << node.id << " profundidade=" << node.depth
              << " lb_no=" << node.lb << " lb_global=" << global_lb_
              << " ub=";
    if (incumbent_.cost < 1e18) {
        std::cerr << incumbent_.cost;
    } else {
        std::cerr << "indisponivel";
    }
    std::cerr << " colunas_pool=" << pool_.size()
              << " colunas_geradas=" << stats_.columns_generated
              << " pricing_heuristico=" << stats_.heuristic_pricing_calls
              << " pricing_exato=" << stats_.exact_pricing_calls
              << " tempo_s=" << elapsed << '\n';
}

void BranchAndPrice::printPool() {
    int i = 0;
    for (const auto& col : this->pool_) {
        std::cout << i++ << ": ";
        for (const auto& r : col.route) { 
            std::cout << r << "-";
        }
        std::cout << std::endl;
    }
}

void BranchAndPrice::printOpen() {
    std::cout << "Open:\n";
    for (const auto& node : this->open_) {
        std::cout << "  " << node->id << std::endl;
    }
}
