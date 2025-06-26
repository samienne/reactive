#include "bqui/annotation.h"

#include <algorithm>
#include <sstream>
#include <cassert>

namespace bqui
{

Annotation::Node const& Annotation::addNode(std::string name)
{
    nodes_.push_back(Node{nodes_.size(), std::move(name)});
    return nodes_.back();
}

Annotation::Edge const& Annotation::addEdge(Node const& from, Node const& to)
{
    edges_.push_back(Edge({from.index, to.index}));
    return edges_.back();
}

void Annotation::addEdge(Node const& from, void const* to)
{
    auto i = shared_.find(to);
    sharedEdges_.push_back(SharedEdge{from.index, i->first});
}

void Annotation::addTree(Node const& node, Annotation const& tree)
{
    /*if (tree.nodes_.empty())
        return;*/

    auto index = nodes_.size();
    auto offset = index;
    auto nodeIndex = node.index;

    for (auto const& n : tree.nodes_)
    {
        nodes_.push_back(n);
        nodes_.back().index += offset;
    }

    for (auto const& e : tree.edges_)
    {
        edges_.push_back(e);
        edges_.back().from += offset;
        edges_.back().to += offset;
    }

    edges_.push_back(Edge{nodeIndex, index});

    for (auto const& s : tree.shared_)
    {
        if (shared_.find(s.first) == shared_.end())
            shared_.insert(s);
    }

    for (auto const& e : tree.sharedEdges_)
    {
        sharedEdges_.push_back({e.from + offset, e.to});
    }
}

void Annotation::addShared(void const* ptr, Node const& node,
        Annotation const& tree)
{
    auto i = shared_.find(ptr);
    if (i != shared_.end())
        return;

    auto j = shared_.insert(std::make_pair(ptr, tree));
    j.first->second.shared_.clear();

    sharedEdges_.push_back({node.index, ptr});

    for (auto const& n : tree.shared_)
    {
        if (shared_.find(n.first) == shared_.end())
            shared_.insert(n);
    }
}

std::vector<Annotation::Node> const& Annotation::getNodes() const
{
    return nodes_;
}

std::vector<Annotation::Edge> const& Annotation::getEdges() const
{
    return edges_;
}

bool Annotation::hasShared(void const* ptr) const
{
    return shared_.find(ptr) != shared_.end();
}

std::string Annotation::getDot() const
{
    std::ostringstream ss;

    ss << "digraph signals {\n";

    for (auto const& node : getNodeNames())
        ss << node;

    for (auto const& edge : getEdgeNames())
        ss << edge;

    ss << "}";

    return ss.str();
}

std::vector<std::string> Annotation::getNodeNames() const
{
    auto toString = [](std::string const& prefix, Node const& node)
        -> std::string
    {
        return "\tn" + prefix + std::to_string(node.index) + "[label=\""
            + node.name + "\"];\n";
    };

    std::vector<std::string> result;
    for (auto const& node : nodes_)
    {
        result.push_back(toString("", node));
    }

    for (auto&& shared : shared_)
    {
        std::string sharedPrefix = std::to_string((size_t)shared.first) + "_";
        for (auto const& node : shared.second.nodes_)
        {
            result.push_back(toString(sharedPrefix, node));
        }
    }

    return result;
}

std::vector<std::string> Annotation::getEdgeNames() const
{
    std::vector<std::string> result;
    for (auto&& edge: edges_)
    {
        result.push_back("\tn" + std::to_string(edge.to) + " -> n"
                + std::to_string(edge.from) + ";\n");
    }

    auto edgeToString = [](SharedEdge const& edge) -> std::string
    {
        return "\tn" + std::to_string((size_t)edge.to) + "_0 -> n"
            + std::to_string(edge.from) + ";\n";
    };

    for (auto const& shared : shared_)
    {
        std::string prefix = std::to_string((size_t)shared.first) + "_";
        for (auto const& edge : shared.second.edges_)
        {
            result.push_back("\tn" + prefix + std::to_string(edge.to)
                    + " -> n" + prefix + std::to_string(edge.from) + ";\n");
        }

        for (auto const& edge : shared.second.sharedEdges_)
        {
            result.push_back("\tn" + std::to_string((size_t)edge.to)
                    + "_0 -> n" + prefix + std::to_string(edge.from) + ";\n");
        }
    }

    for (auto&& edge : sharedEdges_)
    {
        result.push_back(edgeToString(edge));
    }

    return result;
}

} // reactive

