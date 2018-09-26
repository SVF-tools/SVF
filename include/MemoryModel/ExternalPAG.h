//===- ExternalPAG.h -- Program assignment graph------------------------------//

/*
 * ExternalPAG.h
 *
 *  Created on: Aug 20, 2018
 *      Author: Mohamad Barbar
 */

#ifndef EXTERNALPAG_H_
#define EXTERNALPAG_H_

#include "PAG.h"

extern llvm::cl::list<std::string> ExternalPAGArgs;

/// Represents the PAG of a function loaded externally (i.e. from file).
/// It's purpose is to be attached to the main PAG (almost) seamlessly.
class ExternalPAG {
private:
    /// Name of the function this external PAG represents.
    std::string functionName;

    /// Nodes in this external PAG, represented by NodeIDs (and the type,
    /// v or o) because we will rebuild these nodes in the main PAG.
    std::set<std::tuple<NodeID, std::string>> nodes;
    /// Edges in this external PAG, represented by the parts of an Edge because
    /// we will rebuild these edges in the main PAG.
    std::set<std::tuple<NodeID, NodeID, std::string, int>>edges;

    // Special nodes.

    /// Nodes in the ExternalPAG which call edges should connect to.
    /// argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    std::map<int, NodeID> argNodes;
    /// Node from which return edges connect.
    NodeID returnNode;

    /// Whether this function has a return or not.
    bool hasReturn;

public:
    ExternalPAG(std::string functionName) : functionName(functionName),
                                            hasReturn(false) {}

    ~ExternalPAG() {}

    /// For passing that passed to -extpags option, splitting fname@path into
    /// a pair.
    static std::vector<std::pair<std::string, std::string>>
        parseExternalPAGs(llvm::cl::list<std::string> &extpagsArgs);

    std::string getFunctionName() const { return functionName; }

    std::set<std::tuple<NodeID, std::string>> &getNodes() { return nodes; }
    std::set<std::tuple<NodeID, NodeID, std::string, int>> &getEdges() {
        return edges;
    }

    std::map<int, NodeID> &getArgNodes() { return argNodes; }

    NodeID getReturnNode() const { return returnNode; }
    void setReturnNode(NodeID returnNode) {
        this->returnNode = returnNode;
        hasReturn = true;
    }

    /// Does this function have a return node?
    bool hasReturnNode() const { return hasReturn; }

    /// Reads nodes and edges from file.
    ///
    /// File format:
    /// Node: nodeID Nodetype [[0|1|2|...]+|ret]
    ///  * Giving a number means that node represents such argument.
    ///  * Giving ret means that node represents the return node..
    /// Edge: nodeID edgetype NodeID Offset
    ///
    /// Example:
    /// 1 o
    /// 2 v
    /// 3 v
    /// 4 v
    /// 1 addr 2 0
    /// 1 addr 3 0
    /// 3 gep 4 4
    void readFromFile(std::string filename);
};

#endif  /* EXTERNALPAG_H_ */
