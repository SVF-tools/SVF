//===- ExternalPAG.h -- Program assignment graph------------------------------//

/*
 * ExternalPAG.h
 *
 *  Created on: Aug 20, 2018
 *      Author: Mohamad Barbar
 */

#ifndef EXTERNALPAG_H_
#define EXTERNALPAG_H_

#include "Graphs/PAGNode.h"

extern llvm::cl::list<std::string> DumpPAGFunctions;

namespace SVF
{

/// Represents the PAG of a function loaded externally (i.e. from file).
/// It's purpose is to be attached to the main PAG (almost) seamlessly.
class ExternalPAG
{
private:
    /// Maps function names to the entry nodes of the extpag which implements
    /// it. This is to connect arguments and callsites.
    static Map<const SVFFunction*, Map<int, PAGNode *>>
            functionToExternalPAGEntries;
    static Map<const SVFFunction*, PAGNode *> functionToExternalPAGReturns;

    /// Name of the function this external PAG represents.
    std::string functionName;

    /// Value nodes in this external PAG, represented by NodeIDs
    /// because we will rebuild these nodes in the main PAG.
    NodeSet valueNodes;
    /// Object nodes in this external PAG, represented by NodeIDs
    /// because we will rebuild these nodes in the main PAG.
    NodeSet objectNodes;
    /// Edges in this external PAG, represented by the parts of an Edge because
    /// we will rebuild these edges in the main PAG.
    OrderedSet<std::tuple<NodeID, NodeID, std::string, int>> edges;

    // Special nodes.

    /// Nodes in the ExternalPAG which call edges should connect to.
    /// argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    Map<int, NodeID> argNodes;
    /// Node from which return edges connect.
    NodeID returnNode;

    /// Whether this function has a return or not.
    bool hasReturn;

    /// For passing that passed to -extpags option, splitting fname@path into
    /// a pair.
    static std::vector<std::pair<std::string, std::string>>
            parseExternalPAGs(llvm::cl::list<std::string> &extpagsArgs);

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

public:
    ExternalPAG(std::string functionName) : functionName(functionName),
        returnNode(-1), hasReturn(false) {}

    ~ExternalPAG() {}

    /// Parses command line arguments and attaches external PAGs to main
    /// PAG.
    static void initialise(SVFModule* svfModule);

    /// Connects callsite if a external PAG implementing the relevant function
    /// has been added.
    /// Returns true on success, false otherwise.
    static bool connectCallsiteToExternalPAG(CallSite *cs);

    /// Whether an external PAG implementing function exists.
    static bool hasExternalPAG(const SVFFunction* function);

    /// Dump individual PAGs of specified functions. Currently to outs().
    static void dumpFunctions(std::vector<std::string> functions);

    std::string getFunctionName() const
    {
        return functionName;
    }

    NodeSet &getValueNodes()
    {
        return valueNodes;
    }
    NodeSet &getObjectNodes()
    {
        return objectNodes;
    }
    OrderedSet<std::tuple<NodeID, NodeID, std::string, int>> &getEdges()
    {
        return edges;
    }

    Map<int, NodeID> &getArgNodes()
    {
        return argNodes;
    }

    NodeID getReturnNode() const
    {
        return returnNode;
    }
    void setReturnNode(NodeID returnNode)
    {
        this->returnNode = returnNode;
        hasReturn = true;
    }

    /// Does this function have a return node?
    bool hasReturnNode() const
    {
        return hasReturn;
    }

    /// Adds (creates new equivalents) all the nodes and edges of this extpag to
    /// the main PAG. function is used as a key for future lookups.
    /// Returns true on success, false otherwise (incl. if it already exists).
    bool addExternalPAG(const SVFFunction* function);

};

} // End namespace SVF

#endif  /* EXTERNALPAG_H_ */
