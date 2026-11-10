#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

struct Vertex {
    int x = 0;
    int y = 0;
    int weight = 0;
    std::vector<int> priorities;
};

struct RouteEvaluation {
    bool valid = false;
    std::int64_t load = 0;
    std::int64_t logistics_cost = 0;
    std::int64_t priority_cost = 0;
    std::int64_t total_cost = 0;

    void printResult() const;
};

class Instance {
public:
    static Instance read(const std::string& path, int depot, int capacity);

    int vertexCount() const { return static_cast<int>(vertices.size()); }
    int conflictsCount() const { return static_cast<int>(conflicts.size()); }
    bool areInConflict(int i, int j) const;
    void printInstance();

    RouteEvaluation evaluateRoute(const std::vector<int>& route, int day) const;

    int depot = 0;
    int capacity = 0;
    std::vector<Vertex> vertices;
    
    std::vector<std::vector<int>> travel_costs;
    std::vector<std::pair<int, int>> conflicts;
};
