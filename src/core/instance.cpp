#include "instance.hpp"
#include <algorithm>
#include <utility>
#include <vector>

Instance Instance::read(const std::string& path, int depot, int capacity) {
    std::ifstream input (path);
    if (!input) {
        throw std::runtime_error("Não foi possível abrir o arquivo da instância...");
    }

    Instance instance;
    instance.depot = depot;
    instance.capacity = capacity;

    int n_vertices, n_conflicts;
    std::string text;
    
    std::getline(input, text);
    std::istringstream row(text);
    row >> n_vertices >> n_conflicts;

    for (int i = 0; i < n_vertices; ++i) {
        int x, y, w, p;
        
        std::getline(input, text);
        std::istringstream row(text);
        row >> x >> y >> w >> p;

        Vertex v = {x, y, w, p};
        instance.vertices.push_back(v);
    }

    for (int i = 0; i < n_conflicts; ++i) {
        int u, v;
        
        std::getline(input, text);
        std::istringstream row(text);
        row >> u >> v;

        if (u > v) std::swap(u, v);
        instance.conflicts.push_back(std::pair<int, int>(u, v));
    }
    std::sort(instance.conflicts.begin(), instance.conflicts.end());

    instance.printInstance();

    return instance;
}

bool Instance::areInConflict(int i, int j) const {
    if (i > j) std::swap(i, j);
    return std::binary_search(this->conflicts.begin(), this->conflicts.end(), std::make_pair(i, j));
}

void Instance::printInstance() {
    std::cout << "Depot: "      << this->depot << std::endl;
    std::cout << "Capacity: "   << this->capacity << std::endl;
    
    std::cout << "N Vertices: " << this->vertices.size() << std::endl;
    for (int i = 0; i < vertexCount(); ++i) {
        Vertex v = this->vertices[i];
        printf("(%3d, %3d) | w: %d | p: %d\n", v.x, v.y, v.weight, v.priority);
    }

    std::cout << "\nN Conflicts: " << this->conflicts.size() << std::endl;
    for (int i = 0; i < conflictsCount(); ++i) {
        std::pair<int, int> p = this->conflicts[i];
        printf("(%3d, %3d)\n", p.first, p.second);
    }
}