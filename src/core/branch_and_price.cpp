#include "branch_and_price.hpp"
#include <chrono>
#include <cmath>
#include <iterator>
#include <stdexcept>
// #include "instance.hpp"
// #include "solver_rmp.hpp"
// #include "pricing.hpp"

BranchAndPrice::BranchAndPrice(const Instance& inst, const Config& cfg)
    : inst_(inst), cfg_(cfg) {
        this->pool_ = std::vector<Column>();
        this->open_ = std::deque<std::unique_ptr<Node>>();
    // TODO: criar rmp_ e pricer_
}

BranchAndPrice::~BranchAndPrice() = default;

// ---------------------------------------------------------------
// Laço principal
// ---------------------------------------------------------------


Solution BranchAndPrice::solve() {
    t0_ = std::chrono::steady_clock::now();
    initRoot();

    RMPSolver rmp(this->inst_);
 
    for (const auto& column : this->pool_) {
        rmp.addColumn(column);
    }

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - this->t0_
    ).count();

    double remainingSeconds = this->cfg_.time_limit - elapsed;
    int grbStatus = rmp.solveLP(remainingSeconds);
    if (grbStatus == GRB_OPTIMAL) {
        Solution solution;
        solution.cost = rmp.objective();

        auto x = rmp.values();
        if (x.size() != this->pool_.size()) {
            throw std::runtime_error(
                "Quantidade de valores diferente da quantidade de colunas"
            );
        }
        
        for (int v = 0; v < this->inst_.vertexCount(); ++v) {
            if (v == this->inst_.depot) { continue; }

            double sum = 0.0;
            for (std::size_t k = 0; k < x.size(); ++k) {
                if (this->pool_[k].has[v]) {
                    sum += x[k];
                }
            }

            if (std::abs(sum - 1.0) > 1e-6) {
                throw std::runtime_error("Cobertura incorreta de cliente");
            }
        }

        // Conferir o limite de uma rota por dia, com tolerância 1e-6.
        for (int d = 1; d < this->inst_.vertexCount(); ++d) {
            double sum = 0.0;
            for (std::size_t k = 0; k < x.size(); ++k) {
                if (this->pool_[k].day == d) {
                    sum += x[k];
                }
            }

            if (sum > 1.0 + 1e-6) {
                throw std::runtime_error("Limite de rotas por dia violado");
            }
        }

        double objective = 0.0;
        for (std::size_t k = 0; k < x.size(); ++k) {
            objective += this->pool_[k].cost * x[k];
        }

        if (std::abs(objective - rmp.objective()) > 1e-6) {
            throw std::runtime_error("Objetivo do PMR inconsistente");
        }

        for (std::size_t k = 0; k < x.size(); ++k) {
            if (x[k] > 0.5) {
                solution.columns.push_back(this->pool_[k]);
            }
        } 

        return solution;
    }

    // this->printOpen();
    // this->printPool();

    while (!open_.empty() && !timeIsUp()) {
        auto node = selectNode();

        // Poda por limitante: lb herdado do pai já não melhora o UB?
        // TODO: if (node->lb >= incumbent_.cost - eps) continue;

        stats_.nodes++;
        bool ok = processNode(*node);   // resolve o nó (geração de colunas)
        if (!ok) continue;              // inviável ou podado

        // TODO: depois de processNode, node->lb está atualizado
        // TODO: poda por limitante de novo (lb >= UB)

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
bool BranchAndPrice::processNode(Node& node) {
    // 1. Montar o PMR do nó: colunas do pool que respeitam node.decisions
    //    TODO: rmp_->reset(); for (c : pool_) if (columnAllowed(c, node)) rmp_->addColumn(c);
    // 2. Garantir viabilidade (variáveis artificiais Big-M)  TODO
    // 3. Geração de colunas
    return columnGeneration(node);
}

bool BranchAndPrice::columnGeneration(Node& node) {
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
        // TODO: SAME      -> coluna tem ambos ou nenhum dos dois
        // TODO: DIFFERENT -> coluna não tem os dois juntos
    }
    return true;
}

// ---------------------------------------------------------------
// Ramificação
// ---------------------------------------------------------------
bool BranchAndPrice::isIntegral(double eps) const {
    // TODO: todos os x_{R,d} estão a eps de 0 ou 1?
    return false;
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
    // TODO: if (s.cost < incumbent_.cost) incumbent_ = s;
}

void BranchAndPrice::logNode(const Node& node) const {
    // TODO: imprimir no stderr nó, lb, ub, duais (útil p/ conferir o exemplo à mão)
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
