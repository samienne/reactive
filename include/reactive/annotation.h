#pragma once

#include "reactivevisibility.h"

#include <string>
#include <vector>
#include <map>

namespace reactive
{
    class REACTIVE_EXPORT Annotation
    {
    public:
        struct Node
        {
            size_t index;
            std::string name;
        };

        struct Edge
        {
            size_t from;
            size_t to;
        };

        struct SharedEdge
        {
            size_t from;
            void const* to;
        };

        Node const& addNode(std::string name);
        Edge const& addEdge(Node const& from, Node const& to);
        void addEdge(Node const& from, void const* to);
        void addTree(Node const& node, Annotation const& tree);
        void addShared(void const* ptr, Node const& node,
                Annotation const& tree);

        std::vector<Node> const& getNodes() const;
        std::vector<Edge> const& getEdges() const;

        bool hasShared(void const* ptr) const;

        std::string getDot() const;

    private:
        std::vector<std::string> getNodeNames() const;
        std::vector<std::string> getEdgeNames() const;

    private:
        std::vector<Node> nodes_;
        std::vector<Edge> edges_;
        std::map<void const*, Annotation> shared_;
        std::vector<SharedEdge> sharedEdges_;
    };
}

