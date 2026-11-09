#include "branch_and_price.hpp"
#include "pricing.hpp"
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
    initRoot();
    
    Solution initial = buildInitialSolution();
    updateIncumbent(initial);

    // this->printOpen();
    // this->printPool();

    while (!open_.empty() && !timeIsUp()) {
        auto node = selectNode();

        // Poda por limitante: lb herdado do pai já não melhora o UB?
        // TODO: if (node->lb >= incumbent_.cost - eps) continue;

        stats_.nodes++;
        const NodeResult result = processNode(*node);   // resolve o nó (geração de colunas)
        logNode(*node);

        if (result == NodeResult::Unresolved) {
            pushNode(std::move(node));
            break;
        }

        if (result == NodeResult::Infeasible) {
            continue;
        }

        if (result == NodeResult::Error) {
            throw std::runtime_error("Erro ao processar nó");
        }

        // TODO: depois de processNode, node->lb está atualizado
        // TODO: poda por limitante de novo (lb >= UB)

        // TODO [PMR 10]: habilitar podas/fechamento por ótimo do nó somente após
        // pricing certificar convergência. Enquanto CG estiver incompleta,
        // um LP integral pode fornecer UB validado, mas não encerra o nó.
        if (isIntegral()) {
            // TODO: montar Solution a partir do RMP e chamar updateIncumbent()
            continue;
        }

        int i, j;
        if (selectBranchingPair(i, j)) branch(*node, i, j);
        // TODO: e se nenhum par for encontrado mas a solução é fracionária? (investigar)

        updateGlobalLB();
    }

    // Tempo acabou (ou árvore esgotada)
    if (!open_.empty()) roundingHeuristic();   // -h
    // TODO: preencher stats_.time_total
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

void BranchAndPrice::initRoot() {
    this->pool_.clear();
    this->open_.clear();

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
    root->lb = -std::numeric_limits<double>::infinity()/2;

    this->pushNode(std::move(root));
}

// ---------------------------------------------------------------
// Processamento de um nó
// ---------------------------------------------------------------
NodeResult BranchAndPrice::processNode(Node& node) {
    // O PMR inviável não prova inviabilidade do nó; não atualizar node.lb sem certificação.

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

            bool columnExists = false;
            for (const Column& p: pool_) {
                if (p.route == col.route && p.day == col.day) { 
                    columnExists = true;
                    break; 
                }
            }

            if (!columnExists) {
                pool_.push_back(col);
            }
        }
    }

    this->rmp_->reset();
 
    std::vector<std::size_t> columnIndices;
    for (std::size_t i = 0; i < this->pool_.size(); ++i) {
        if (!this->columnAllowed(this->pool_[i], node)) { continue; }

        this->rmp_->addColumn(this->pool_[i]);
        columnIndices.push_back(i);
    }

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - this->t0_
    ).count();


    double remainingSeconds = this->cfg_.time_limit - elapsed;
    int grbStatus = this->rmp_->solveLP(remainingSeconds);
    if (grbStatus == GRB_OPTIMAL) {
        const auto initialSolution = validateInitialRmp(*this->rmp_, columnIndices);
        if (initialSolution) { 
            updateIncumbent(*initialSolution); 
        }
    }
    else if (grbStatus == GRB_TIME_LIMIT) {
        // Preserva incumbente e LB.
        return NodeResult::Unresolved;
    }
    else if (grbStatus == GRB_INFEASIBLE) {
        // PMR inviável.
        return NodeResult::Unresolved;
    }
    else {
        std::cerr << "Status inesperado do PMR: " << grbStatus << '\n';
        return NodeResult::Error;
    }

    // return columnGeneration(node);
    return NodeResult::Unresolved;
}

bool BranchAndPrice::columnGeneration(Node& node) {
    // PRÓXIMA FASE — começar depois de integrar e validar PMR 1 a 10.
    // TODO [CG 2]: remover o retorno provisório abaixo ao implementar o ciclo.
    // TODO [CG 3]: resolver PMR, conferir status e extrair u_i/lambda_d.
    // TODO [CG 4]: pricing heurístico com decisões do nó; se não encontrar
    // coluna melhorante, executar pricing exato para TODOS os dias.
    // TODO [CG 5]: validar rota, dia, decisões e custo reduzido das candidatas;
    // deduplicar sequência/dia, inserir no pool e no mestre e atualizar índices.
    // TODO [CG 6]: repetir LP/pricing. Só atualizar node.lb após prova exata
    // de ausência de custo reduzido negativo; timeout mantém LB certificado anterior.
    return true;
    while (!timeIsUp()) {
        // (a) TODO: rmp_->solveLP();  se inviável -> return false
        // (b) TODO: obter duais u_i e lambda_d
        // (c) Pricing:
        //     TODO: if (cfg_.use_heuristic_pricing) cols = pricer_->heuristic(duais, node.decisions);
        //     TODO: if (cols.empty()) cols = pricer_->exact(duais, node.decisions);
        // (d) TODO: se cols.empty(): convergiu -> node.lb = obj do RMP (LB válido!)
        //           se ainda houver artificial > 0 -> nó inviável (return false)
        //           break;
        // (e) TODO: adicionar cols ao pool_ (sem duplicar) e ao rmp_
        //     stats_.columns_generated += ...
    }
    // Se parou por tempo, o obj do RMP NÃO é limitante inferior válido!
    // TODO: tratar esse caso (não atualizar lb / marcar nó como não resolvido)
    return true;
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

bool BranchAndPrice::selectBranchingPair(int& i, int& j) const {
    // TODO: calcular f_ij = sum_{R ∋ i,j} sum_d x_{R,d}
    // TODO: escolher par com f_ij mais próximo de 0.5 (cfg_.branching_rule p/ EXP3)
    // Dica: pares já fixados por decisões anteriores não precisam ser considerados.
    return false;
}

void BranchAndPrice::branch(const Node& node, int i, int j) {
    // TODO: filho SAME:      decisions = node.decisions + {i,j,true}
    // TODO: filho DIFFERENT: decisions = node.decisions + {i,j,false}
    // TODO: lb dos filhos = node.lb ; depth + 1 ; pushNode(...)
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

    // TODO: BEST_BOUND -> nó com menor lb
    else if (cfg_.search == SearchStrategy::BEST_BOUND) {
        node = nullptr;
    }
    return node;
}

void BranchAndPrice::pushNode(std::unique_ptr<Node> n) {
    this->open_.push_back(std::move(n));
}

void BranchAndPrice::updateGlobalLB() {
    // TODO: min(lb) entre os nós abertos (e o nó corrente, se aplicável)
}

// ---------------------------------------------------------------
// Heurística e utilidades
// ---------------------------------------------------------------
void BranchAndPrice::roundingHeuristic() {
    // TODO: MIP (x binário) com TODAS as colunas do pool_, limite cfg_.heuristic_time
    // TODO: se melhorar, updateIncumbent()
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
