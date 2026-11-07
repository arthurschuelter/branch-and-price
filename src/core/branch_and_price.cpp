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
    // TODO [PMR 2]: criar rmp_ com std::make_shared<RMPSolver>(inst_).
    // Usar este membro em todos os nós; reset() reconstrói o modelo de cada nó.
    // TODO [CG 1]: criar pricer_ quando a interface e a implementação de Pricer
    // existirem. A integração do PMR pode ser feita antes dessa etapa.
}

BranchAndPrice::~BranchAndPrice() = default;

// ---------------------------------------------------------------
// Laço principal
// ---------------------------------------------------------------


Solution BranchAndPrice::solve() {
    t0_ = std::chrono::steady_clock::now();
    initRoot();

    // ESQUELETO DA PRÓXIMA ETAPA — seguir PMR 1 a 10, nesta ordem.
    // A inspeção dos duais do teste local está implementada no bloco GRB_OPTIMAL.
    // TODO [PMR 3]: construir uma incumbente inicial: escolher uma coluna
    // unitária por cliente, atribuindo dias distintos 1..n-1. Validar as rotas,
    // cobertura, dias e custo antes de guardá-la; manter global_lb_ = 0.
    // Assim, timeout antes do primeiro LP ainda permite devolver solução viável.
    // TODO [PMR 4]: depois da inspeção, mover montagem/solução do mestre local
    // para processNode(), usando rmp_. Remover este bloco de diagnóstico inteiro,
    // incluindo seu retorno antecipado, para alcançar o laço da árvore.
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
        constexpr double eps = 1e-6;
        const int n = inst_.vertexCount();
        const double lpObjective = rmp.objective();
        const auto x = rmp.values();
        const auto u = rmp.customerDuals();
        const auto lambda = rmp.dayDuals();

        if (!std::isfinite(lpObjective)) {
            throw std::runtime_error("Objetivo do PMR não finito");
        }
        if (x.size() != this->pool_.size()) {
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

            const auto& column = pool_[k];
            double reducedCost = column.cost - lambda[column.day];
            for (int i = 0; i < n; ++i) {
                if (i != inst_.depot && column.has[i]) reducedCost -= u[i];
            }
            if (!std::isfinite(reducedCost) || reducedCost < -eps) {
                throw std::runtime_error("Custo reduzido negativo no PMR ótimo");
            }
            if (x[k] > eps && std::abs(reducedCost) > eps) {
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

        if (!std::isfinite(objective) || std::abs(objective - lpObjective) > eps) {
            throw std::runtime_error("Objetivo do PMR inconsistente");
        }

        // O teste local ainda não tem pricing/branching integrado. Impedir que
        // um LP fracionário seja convertido ou enviado para a árvore provisória.
        if (!integral) {
            throw std::runtime_error(
                "PMR fracionário: integração com pricing/branching ainda pendente"
            );
        }

        Solution solution;
        solution.cost = 0.0;
        std::vector<int> coverage(n, 0);
        std::vector<bool> usedDays(n, false);
        for (std::size_t k = 0; k < x.size(); ++k) {
            if (x[k] > 0.5) {
                const auto& column = pool_[k];
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
        updateIncumbent(solution);
        return incumbent_;
    }

    // this->printOpen();
    // this->printPool();

    while (!open_.empty() && !timeIsUp()) {
        auto node = selectNode();

        // TODO [PMR 9]: definir um resultado de processamento que diferencie
        // interrupção, inviabilidade provada e convergência certificada.
        // O bool atual não expressa todos esses casos. Em timeout, preservar
        // este nó na fronteira antes de sair; não descartá-lo no continue.

        // Poda por limitante: lb herdado do pai já não melhora o UB?
        // TODO: if (node->lb >= incumbent_.cost - eps) continue;

        stats_.nodes++;
        bool ok = processNode(*node);   // resolve o nó (geração de colunas)
        if (!ok) continue;              // inviável ou podado

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
    // TODO [PMR 5a]: chamar rmp_->reset(); percorrer pool_ e inserir somente
    // colunas para as quais columnAllowed(c, node) seja verdadeiro.
    // Manter um vetor com os índices dessas colunas no pool_ na ordem de inserção:
    // depois da filtragem, o índice de rmp_->values() NÃO é o índice do pool_.
    // TODO [PMR 5b]: na raiz, conferir 36 colunas e 12 restrições do exemplo.
    // Nos filhos SAME, garantir colunas viáveis com grupos inteiros e dia;
    // unitárias filtradas podem não bastar. Construir e avaliar a ordem da rota.
    // Se optar por artificiais, implementar uma fase de viabilidade certificada.
    // TODO [PMR 6a]: calcular restante = cfg_.time_limit - segundos desde t0_
    // imediatamente antes de solveLP(restante), sem reiniciar o relógio por nó.
    // TODO [PMR 6b]: tratar status explicitamente. Ler primais/duais apenas após
    // GRB_OPTIMAL nesta etapa. TIME_LIMIT preserva incumbente e LB herdado.
    // GRB_INFEASIBLE do PMR filtrado sozinho não prova inviabilidade do nó.
    // TODO [PMR 6c]: reutilizar as conferências de cobertura, dias e objetivo
    // do teste local de solve(), usando o mapeamento de índices acima.
    // Não atribuir objective() a node.lb enquanto não houver certificação.
    return columnGeneration(node);
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
        // TODO: SAME      -> coluna tem ambos ou nenhum dos dois
        // TODO: DIFFERENT -> coluna não tem os dois juntos
    }
    return true;
}

// ---------------------------------------------------------------
// Ramificação
// ---------------------------------------------------------------
bool BranchAndPrice::isIntegral(double eps) const {
    // TODO [PMR 7a]: consultar os valores da última solução LP ótima do mestre.
    // Para cada x, verificar proximidade de 0 ou 1 com eps; rejeitar valores
    // não finitos ou fora do domínio. Não usar apenas x > 0.5 como teste.
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
    // O chamador valida a solução antes de propor uma nova incumbente.
    if (std::isfinite(s.cost) && s.cost < incumbent_.cost - 1e-6) {
        incumbent_ = s;
    }
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
