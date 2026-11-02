#include "instance.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

Instance Instance::read(const std::string& path, int depot, int capacity) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Não foi possível abrir o arquivo da instância: " + path);
    }
    if (capacity <= 0) throw std::runtime_error("A capacidade -C deve ser positiva.");

    // Descartando linhas vazias e comentários (#...).
    std::vector<std::string> lines;
    std::vector<int> line_numbers;
    std::string text;
    int line_number = 0;
    while (std::getline(input, text)) {
        ++line_number;
        std::istringstream check(text);
        std::string first_token;
        if (!(check >> first_token) || first_token[0] == '#') continue;
        lines.push_back(text);
        line_numbers.push_back(line_number);
    }
    if (lines.empty()) throw std::runtime_error("O arquivo da instância está vazio.");

    std::size_t cursor = 0;
    int n_vertices = 0;
    int n_conflicts = 0;
    std::string extra;
    std::istringstream header(lines[cursor]);
    if (!(header >> n_vertices >> n_conflicts) || (header >> extra)) {
        throw std::runtime_error(path + ":" + std::to_string(line_numbers[cursor]) +
                                 ": cabeçalho esperado: <n> <m>.");
    }
    ++cursor;
    if (n_vertices < 2) throw std::runtime_error("A instância precisa ter depósito e ao menos um cliente.");
    const long long number_of_pairs = n_vertices * (n_vertices - 1) / 2;
    if (n_conflicts < 0 || n_conflicts > number_of_pairs) {
        throw std::runtime_error("Número de conflitos inválido no cabeçalho.");
    }
    if (depot < 0 || depot >= n_vertices) {
        throw std::runtime_error("O depósito -s deve estar entre 0 e " + std::to_string(n_vertices - 1) + ".");
    }

    Instance instance;
    instance.depot = depot;
    instance.capacity = capacity;

    // Leitura dos vértices: w, x, y e pi(i,d) para d=1,...,n-1.
    for (int i = 0; i < n_vertices; ++i) {
        if (cursor >= lines.size()) throw std::runtime_error("Fim inesperado nas linhas dos vértices.");
        const int current_line = line_numbers[cursor];
        std::istringstream row(lines[cursor++]);
        std::vector<int> fields;
        int value = 0;
        while (row >> value) fields.push_back(value);
        if (!row.eof() || fields.size() != static_cast<std::size_t>(n_vertices + 2)) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                     ": linha de vértice deve ter n+2 inteiros.");
        }

        Vertex vertex;
        vertex.weight = fields[0];
        vertex.x = fields[1];
        vertex.y = fields[2];
        if (vertex.weight < 0 || (i != depot && vertex.weight > capacity)) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                     ": peso inválido ou maior que a capacidade.");
        }
        vertex.priorities.assign(fields.begin() + 3, fields.end());
        for (std::size_t d = 0; d < vertex.priorities.size(); ++d) {
            if (vertex.priorities[d] < 0) {
                throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                         ": custo de prioridade negativo.");
            }
            if (i != depot && d > 0 && vertex.priorities[d] < vertex.priorities[d - 1]) {
                throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                         ": prioridades de cliente devem ser não decrescentes.");
            }
        }
        instance.vertices.push_back(std::move(vertex));
    }

    // Linha de custo para cada par i<j; a matriz é simétrica e c(i,i)=0.
    instance.travel_costs.assign(n_vertices, std::vector<int>(n_vertices, 0));
    std::vector<std::vector<bool>> cost_seen(n_vertices,
                                             std::vector<bool>(n_vertices, false));
    for (long long k = 0; k < number_of_pairs; ++k) {
        if (cursor >= lines.size()) throw std::runtime_error("Fim inesperado nas linhas de custo c(i,j).");
        const int current_line = line_numbers[cursor];
        std::istringstream row(lines[cursor++]);
        int i = 0, j = 0, cost = 0;
        if (!(row >> i >> j >> cost) || (row >> extra) || i < 0 || j >= n_vertices || i >= j || cost < 0) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                     ": custo esperado no formato <i> <j> <c(i,j)>, com 0<=i<j<n e c>=0.");
        }
        if (cost_seen[i][j]) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) + ": par de custo duplicado.");
        }
        cost_seen[i][j] = true;
        instance.travel_costs[i][j] = cost;
        instance.travel_costs[j][i] = cost;
    }

    // Arestas de conflito.
    std::vector<std::vector<bool>> conflict_seen(n_vertices, std::vector<bool>(n_vertices, false));
    for (int k = 0; k < n_conflicts; ++k) {
        if (cursor >= lines.size()) throw std::runtime_error("Fim inesperado nas linhas de conflito.");

        const int current_line = line_numbers[cursor];
        std::istringstream row(lines[cursor++]);
        int i = 0, j = 0;
        if (!(row >> i >> j) || (row >> extra) || i < 0 || j >= n_vertices || i >= j) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) +
                                     ": conflito esperado no formato <i> <j>, com 0<=i<j<n.");
        }

        if (conflict_seen[i][j]) {
            throw std::runtime_error(path + ":" + std::to_string(current_line) + ": conflito duplicado.");
        }
        
        conflict_seen[i][j] = true;
        instance.conflicts.emplace_back(i, j);
    }
    std::sort(instance.conflicts.begin(), instance.conflicts.end());

    if (cursor != lines.size()) {
        throw std::runtime_error(path + ":" + std::to_string(line_numbers[cursor]) +
                                 ": conteúdo extra após as arestas de conflito.");
    }

    instance.printInstance();
    return instance;
}

bool Instance::areInConflict(int i, int j) const {
    if (i > j) std::swap(i, j);
    return std::binary_search(this->conflicts.begin(), this->conflicts.end(), std::make_pair(i, j));
}

RouteEvaluation Instance::evaluateRoute(const std::vector<int>& route, int day) const {
    RouteEvaluation result;
    // TODO: validar dia, índices de clientes, clientes repetidos e rota vazia.

    // TODO: somar os pesos em result.load e verificar capacidade.
    for (int u = 0; u < static_cast<int>(route.size()); ++u) {
        Vertex v = this->vertices[route[u]];
        int next_node = (u+1) % route.size();
        int curr_node = u;
        result.load += v.weight;

        if (result.load > this->capacity) {
            result.valid = false;
            return result;
        }

        result.logistics_cost += this->travel_costs[curr_node][next_node];
        result.priority_cost += this->vertices[route[u]].priorities[day - 1];
    }

    // TODO: rejeitar a rota se algum par de clientes estiver em conflito.
    for (int u = 0; u < static_cast<int>(route.size()); ++u) {
        for (int v = 0; v < static_cast<int>(route.size()); ++v) {
        if (this->areInConflict(route[u], route[v])) {
                result.valid = false;
                return result;
            }
        }
    }
    // TODO: calcular a prioridade como soma(vertices[i].priority * day).
    // TODO: preencher result.valid e result.total_cost.
    result.valid = true;
    return result;
}

void Instance::printInstance() {
    std::cout << "Depot: "      << this->depot << std::endl;
    std::cout << "Capacity: "   << this->capacity << std::endl;
    
    std::cout << "N Vertices: " << this->vertices.size() << std::endl;
    for (int i = 0; i < vertexCount(); ++i) {
        const Vertex& v = this->vertices[i];
        std::printf("(%3d, %3d) | w: %d | priorities:", v.x, v.y, v.weight);
        for (int priority : v.priorities) std::printf(" %d", priority);
        std::printf("\n");
    }

    std::cout << "\nTravel costs (upper triangle):\n";
    for (int i = 0; i < vertexCount(); ++i) {
        for (int j = i + 1; j < vertexCount(); ++j) {
            std::printf("%d %d %d\n", i, j, travel_costs[i][j]);
        }
    }

    std::cout << "\nN Conflicts: " << this->conflicts.size() << std::endl;
    for (int i = 0; i < conflictsCount(); ++i) {
        std::pair<int, int> p = this->conflicts[i];
        std::printf("(%3d, %3d)\n", p.first, p.second);
    }
}
