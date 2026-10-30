# Prompt 2:
Modelo: Claude - Sonnet 5.5
Data: 30/10/2026

## Prompt 1
Anexado: `roadmap.md`.
```
De acordo com esse roadmap, me ajude a montar o esboço do main para um algoritmo do branch and price.
Estou meio perdido em como estruturar esse código, quero que faça apenas a parte do branch and price. NÃO QUERO O CODIGO PRONTO, APENAS UMA ESTRUTURA INICIAL PARA QUE EU POSSA ITERAR.
Ainda, me explique como o branch and price funciona
```

## Resposta 1
```
Vou criar os arquivos do esqueleto (só estrutura, com TODOs) e depois explico o algoritmo.

Montei três arquivos com a estrutura e os TODOs: main.cpp, branch_and_price.hpp e branch_and_price.cpp. Não há lógica implementada, só o fluxo e os pontos em que você precisa decidir algo.

Como o Branch-and-Price funciona

1. O problema do mestre tem colunas demais. No seu problema, cada coluna é uma rota válida R num dia d. Existem exponencialmente muitas, então não dá para listar todas. O PMR começa com poucas colunas (as unitárias) e vai ganhando outras.

2. Geração de colunas resolve a relaxação linear.

Você resolve o PMR (LP) com as colunas atuais. O Gurobi devolve os duais: u_i (um por cliente) e λ_d (um por dia).
Os duais dizem quanto cada cliente e cada dia “valem” no ótimo atual.
O pricing procura uma coluna com custo reduzido negativo, c̄ = ρ(R) + Σ(π(i,d) − u_i) − λ_d < −ε. Isso significa que, se essa rota existisse no PMR, ela melhoraria o objetivo.
Se achar, você adiciona a coluna e resolve de novo. Se o pricing exato não achar nenhuma, o LP restrito é ótimo para o LP completo, e o valor é um limitante inferior válido.
Por isso o pricing heurístico vem primeiro (barato) e o exato só entra quando o heurístico falha. Só o exato prova que a convergência é real.

3. A solução do LP pode ser fracionária, então ramifica-se. Ramificar em x_{R,d} seria péssimo: o ramo x=0 não impede o pricing de gerar a mesma rota de novo. Por isso o Ryan-Foster ramifica em pares de clientes:

f_ij = Σ_{R∋i,j} Σ_d x_{R,d} indica o quanto i e j estão juntos. Se alguma f_ij é fracionária, escolhe-se o par mais próximo de 0,5.
SAME: i e j precisam estar na mesma rota. Colunas com só um dos dois saem do PMR, e o pricing precisa respeitar isso.
DIFFERENT: i e j nunca na mesma rota. Colunas com os dois saem, e no pricing vira uma aresta de conflito extra.
Essas decisões ficam no nó e valem para toda a subárvore.

4. A árvore funciona como um branch-and-bound comum. Cada nó roda a geração de colunas e produz um LB. A poda acontece quando o LB é maior ou igual ao UB, quando o nó é inviável ou quando a solução já é inteira (atualiza o UB). A heurística -h pega o pool de colunas e resolve um MIP para tentar melhorar o UB.

Como o esqueleto se organiza
solve() é o laço da árvore: seleciona o nó, processa, poda, testa integralidade, ramifica.
processNode monta o PMR com as colunas do pool que respeitam as decisões do nó e chama columnGeneration.
columnAllowed filtra o pool por nó, para você manter um pool global em vez de uma lista por nó.
selectNode é onde alternam DFS e best-bound (EXP4).
selectBranchingPair concentra a regra de ramificação (EXP3).
Config já tem as flags das ablações, para você não precisar refatorar depois.
Pontos onde costuma dar problema
Viabilidade em nós profundos. Use variáveis artificiais com Big-M no PMR. Se alguma ficar positiva depois da convergência, o nó é inviável. Isso afeta os duais, então o custo reduzido do pricing precisa ser consistente com eles.
LB inválido por timeout. Se a geração de colunas for interrompida, o objetivo do RMP não é limitante inferior. Deixei um comentário no código sobre isso.
Pricing sob SAME. Tratar o par como “super-cliente” é a abordagem mais simples. Pense em como isso afeta capacidade e custo de TSP.
Colunas duplicadas. Sem checagem, o pricing pode devolver a mesma rota repetidamente. Considere um hash por (conjunto de clientes, dia).
Custo reduzido do dia. Perceba que o pricing depende do dia d, porque π(i,d) e λ_d mudam. Isso pode virar um pricing por dia.
Sugestão de ordem para iterar
Instance e colunas unitárias.
RMP e geração de colunas só no nó raiz, que é o ponto de controle da Fase 2.
Árvore com DFS e Ryan-Foster.
Best-bound, -h e as ablações.

Se quiser, o próximo passo pode ser o esqueleto do Pricer ou do RMPSolver, que são as duas interfaces que este arquivo assume.
```


``` C++
// main.cpp -- ESQUELETO
#include <iostream>
#include <unistd.h>      // getopt
#include "branch_and_price.hpp"
// #include "instance.hpp"

int main(int argc, char** argv) {
    std::string file, out;
    int depot = 0, capacity = 0;
    Config cfg{};
    cfg.time_limit = 60;
    cfg.heuristic_time = 0;

    // TODO: parsing: -f -s -C -t -h -o   (cuidado: -h conflita com ajuda no getopt)
    // TODO: flags extras p/ ablações (--no-heur-pricing, --dfs, --bestbound, ...)

    // TODO: Instance inst = Instance::read(file, depot, capacity);

    // TODO: BranchAndPrice bp(inst, cfg);
    // TODO: Solution sol = bp.solve();

    // TODO: escrever sol + bp.lowerBound() em 'out' no formato do enunciado
    // TODO: stats em stderr / CSV para o script de experimentos
    return 0;
}

```

``` C++
// branch_and_price.hpp -- ESQUELETO. Tudo marcado com TODO é com você.
#pragma once
#include <vector>
#include <deque>
#include <memory>
#include <chrono>
#include <string>

// ---------------------------------------------------------------
// Estruturas de dados
// ---------------------------------------------------------------

// Uma coluna do mestre: rota R no dia d
struct Column {
    std::vector<int> route;   // ordem de visita (sem o depósito) -> útil p/ saída
    std::vector<bool> has;    // has[i] = true se cliente i está na rota (consulta O(1))
    int day;                  // dia d
    double cost;              // c_{R,d} = rho(R) + pi(R,d)
    // TODO: considere guardar um hash/assinatura para detectar colunas duplicadas
};

// Decisão de ramificação de Ryan-Foster
struct BranchDecision {
    int i, j;       // par de clientes (i < j)
    bool same;      // true = SAME (mesma rota), false = DIFFERENT
};

// Nó da árvore
struct Node {
    int id;
    int depth;
    double lb;                              // limitante inferior do nó (valor do PMR)
    std::vector<BranchDecision> decisions;  // caminho da raiz até aqui (todas as decisões)
    // TODO: guardar a solução fracionária? duais para warm start? (decida o que precisa)
};

struct Solution {
    double cost = 1e18;                 // UB
    std::vector<Column> columns;        // rotas escolhidas (cada uma com seu dia)
};

struct Stats {
    long nodes = 0;
    long columns_generated = 0;
    double time_pricing = 0;
    double time_total = 0;
    double root_lb = 0;
    // TODO: o que mais o rodar_experimentos.py precisa?
};

enum class SearchStrategy { DFS, BEST_BOUND };

struct Config {
    double time_limit;              // -t
    double heuristic_time;          // -h
    SearchStrategy search = SearchStrategy::DFS;
    bool use_heuristic_pricing = true;   // EXP1
    bool use_preprocessing = true;       // EXP2
    int branching_rule = 0;              // EXP3
    // TODO: epsilon, big-M, etc.
};

// ---------------------------------------------------------------
// Classe principal
// ---------------------------------------------------------------
class Instance;     // instance.hpp
class RMPSolver;    // solver_rmp.hpp  (wrapper do Gurobi: LP do mestre)
class Pricer;       // pricing.hpp     (heurístico + exato)

class BranchAndPrice {
public:
    BranchAndPrice(const Instance& inst, const Config& cfg);
    Solution solve();                       // laço principal da árvore
    double lowerBound() const { return global_lb_; }
    const Stats& stats() const { return stats_; }

private:
    // --- árvore ---
    void initRoot();                                  // cria nó raiz + colunas iniciais
    std::unique_ptr<Node> selectNode();               // DFS ou best-bound
    void pushNode(std::unique_ptr<Node> n);
    void updateGlobalLB();                            // LB global = min lb dos nós abertos

    // --- processamento de um nó ---
    // Retorna false se nó inviável / podado
    bool processNode(Node& node);
    bool columnGeneration(Node& node);                // loop: RMP -> pricing -> add cols
    bool columnAllowed(const Column& c, const Node& node) const;  // respeita SAME/DIFF?

    // --- ramificação ---
    bool isIntegral(double eps = 1e-6) const;         // x_{R,d} todo inteiro?
    bool selectBranchingPair(int& i, int& j) const;   // f_ij mais perto de 0.5 (EXP3)
    void branch(const Node& node, int i, int j);      // cria filhos SAME e DIFFERENT

    // --- heurística (-h) ---
    void roundingHeuristic();                         // MIP sobre o pool de colunas

    // --- utilidades ---
    bool timeIsUp() const;
    void updateIncumbent(const Solution& s);
    void logNode(const Node& node) const;             // stderr: ajuda no exemplo à mão

    const Instance& inst_;
    Config cfg_;
    Stats stats_;

    std::vector<Column> pool_;          // pool GLOBAL de colunas (filtra-se por nó)
    std::deque<std::unique_ptr<Node>> open_;   // nós abertos (TODO: trocar por priority_queue p/ best-bound)

    Solution incumbent_;                // UB
    double global_lb_ = 0;

    std::unique_ptr<RMPSolver> rmp_;
    std::unique_ptr<Pricer> pricer_;
    std::chrono::steady_clock::time_point t0_;
};

```

``` C++
// branch_and_price.cpp -- ESQUELETO: só o fluxo; preencha os TODOs.
#include "branch_and_price.hpp"
// #include "instance.hpp"
// #include "solver_rmp.hpp"
// #include "pricing.hpp"

BranchAndPrice::BranchAndPrice(const Instance& inst, const Config& cfg)
    : inst_(inst), cfg_(cfg) {
    // TODO: criar rmp_ e pricer_
}

// ---------------------------------------------------------------
// Laço principal
// ---------------------------------------------------------------
Solution BranchAndPrice::solve() {
    t0_ = std::chrono::steady_clock::now();
    initRoot();

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

// ---------------------------------------------------------------
// Raiz
// ---------------------------------------------------------------
void BranchAndPrice::initRoot() {
    // TODO: colunas unitárias (i, d) para todo i e todo d  -> pool_
    // TODO: (EXP2) pré-processamento, se ativado
    // TODO: criar Node raiz (decisions vazio, lb = -inf) e pushNode
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
    // TODO: DFS  -> pop do fim (pilha)
    // TODO: BEST_BOUND -> nó com menor lb
    return nullptr;
}

void BranchAndPrice::pushNode(std::unique_ptr<Node> n) {
    // TODO
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
    // TODO: comparar tempo decorrido com cfg_.time_limit
    return false;
}

void BranchAndPrice::updateIncumbent(const Solution& s) {
    // TODO: if (s.cost < incumbent_.cost) incumbent_ = s;
}

void BranchAndPrice::logNode(const Node& node) const {
    // TODO: imprimir no stderr nó, lb, ub, duais (útil p/ conferir o exemplo à mão)
}

```

***

## Prompt 2
```
Me explique como funciona geração de colunas
```
## Resposta 2
```
A ideia

Alguns LPs têm um número enorme de variáveis (colunas), então não dá para escrevê-los por inteiro. A geração de colunas resolve o LP com um subconjunto das colunas e só cria as que realmente ajudam.

O ciclo
Mestre restrito (PMR): resolve o LP só com as colunas atuais. O ponto de partida precisa ser viável (por isso as colunas unitárias).
Duais: o solver devolve um preço dual para cada restrição (u_i por cliente, λ_d por dia).
Pricing: procura uma coluna fora do PMR com custo reduzido negativo.
Adiciona as colunas encontradas e volta ao passo 1.
Para quando o pricing exato prova que nenhuma coluna tem custo reduzido negativo.
Por que isso funciona

Num LP de minimização, a solução é ótima quando nenhuma variável tem custo reduzido negativo. O PMR só garante isso para as colunas que ele conhece. As outras, que ficaram de fora, são “não verificadas”.

O pricing faz essa verificação sem enumerar nada:

text
c̄(R,d) = c(R,d) − Σ_{i∈R} u_i − λ_d

Intuitivamente, o custo da rota menos o “valor” que ela entrega, medido pelos preços duais. Se for negativo, a rota é vantajosa pelos preços atuais e deve entrar. Se o mínimo de c̄ sobre todas as rotas for ≥ 0, o PMR já é ótimo para o problema completo.

O pricing é um problema de otimização

Em vez de checar rota por rota, o pricing minimiza c̄ sobre todas as rotas válidas. No seu caso, isso é um problema do tipo TSP com capacidade e conflitos, por dia. Daí a divisão:

Heurístico: acha rotas boas rápido. Se achar c̄ < 0, já serve para continuar o ciclo.
Exato: só é chamado quando o heurístico falha, e é ele que prova a convergência.
Dois pontos importantes
O valor do PMR é um limitante superior do LP completo durante o processo, porque ele tem menos colunas. Ele só vira o valor exato do LP (e, portanto, limitante inferior do problema inteiro) quando o pricing exato confirma que não há colunas negativas.
A convergência pode ser lenta (tailing-off): as últimas iterações melhoram pouco. Em geral ajuda adicionar várias colunas por iteração, não só uma.
Resumo em uma frase

O mestre diz “quanto vale cada restrição”, o pricing responde “existe alguma coluna que, a esse preço, compensa?”, e o ciclo se repete até a resposta ser “não”.
```
***

## Prompt 3
```
pode gerar uma instância simples para começar a testar e desenvolver
```
## Resposta 3
Gerou `instancia_teste.txt`
```
Criei instancia_teste.txt. Atenção: o roadmap não traz o formato de entrada do enunciado, então inventei um. Se o formato real for diferente, é só ajustar o parser.

Formato adotado:

text
n m                  # nº de vértices (incluindo depósito) e nº de arestas de conflito
x y w a              # uma linha por vértice 0..n-1: coordenadas, peso, prioridade a_i
...
u v                  # m linhas: arestas do grafo de conflitos

Conteúdo: depósito 0, 6 clientes, C = 10, distância de Manhattan e π(i,d) = a_i · d.

Cliente	(x,y)	peso	a_i
1	(2,1)	4	3
2	(3,3)	3	1
3	(-2,2)	5	2
4	(-3,-1)	2	5
5	(1,-3)	6	1
6	(4,-2)	3	4

Conflitos: (1,2) e (3,5).

Por que serve para testar:

O peso total é 23, então com C = 10 precisa de pelo menos 3 rotas, e o problema não é trivial.
Nenhum cliente sozinho excede C, então as colunas unitárias são válidas.
Os conflitos já exercitam o pricing e o grafo de conflitos.
Existe partição viável, por exemplo {1,3}, {2,4,6}, {5}.
Tem 6 dias e poucas rotas possíveis, então dá para conferir à mão e comparar com enum_exata (n < 8).

Quando o -h e os experimentos estiverem prontos, o gerador_instancias.py pode produzir instâncias maiores nesse mesmo formato.ver
```