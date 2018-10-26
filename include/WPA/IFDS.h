/*
 * IFDS.h
 *
 */

#ifndef INCLUDE_WPA_IFDS_H_
#define INCLUDE_WPA_IFDS_H_

#include "Util/ICFG.h"

class IFDS {

private:
	ICFG* icfg;

public:
    class PathNode;
    class PathEdge;
    typedef std::string ValueName;          //(stmtNode->getPAGDstNode()->getValueName())
    typedef std::set<PAGNode*> Datafact;   // set of uninitialized variables at ICFGNode
    typedef std::set<Datafact> Facts;     // different datafacts from different path
    typedef std::set<ICFGNode*> ICFGNodeSet;
    typedef std::list<PathEdge*> PathEdgeSet;
    typedef std::map<ICFGNode*, Facts> ICFGNodeToDataFactsMap;

protected:
    PathEdgeSet WorkList;         //worklist used during the tabulation algorithm
    PathEdgeSet PathEdgeList;     //used to restore all PathEdges (result)
    PathEdgeSet SummaryEdgeList;  //used to restore all SummaryEdges
    ICFGNodeSet ICFGDstNodeSet;
    ICFGNodeToDataFactsMap UniniVarSolution; // uninitialized variable solution
    ICFGNode* mainEntryNode;

public:
	inline VFG* getVFG() const{
		return icfg->getVFG();
	}

	inline ICFG* getICFG() const{
		return icfg;
	}

	inline PAG* getPAG() const{
		return icfg->getPAG();
	}

	//constructor
	IFDS(ICFG* i);

	//procedures in Tabulation Algorithm
    void initialize();
    void forwardTabulate();
    void merge();

    //add new PathEdge components into PathEdgeList and WorkList
    void propagate(PathNode* srcPN, ICFGNode* succ, Datafact d);

    //get all ICFGNode in all EndPathNode of PathEdgeList
    ICFGNodeSet& getDstICFGNodeSet();

    //get all datafacts of a ICFGNode in all EndPathNode of PathEdgeList
    Facts getDstICFGNodeFacts(ICFGNode* node);

    // transfer function of given ICFGNode
    Datafact transferFun(PathNode* pathNode);

    //whether the variable is initialized
    bool isInitialized(PAGNode* pagNode, Datafact datafact);

    // print ICFGNodes and theirs datafacts
    void printRes();

    // in order to denote : <node, d> , d here is datafact before the execution of node
    class PathNode{
        ICFGNode* icfgNode;
        Datafact datafact;

        //Constructor
    public:
        PathNode();

        PathNode(ICFGNode* node, Datafact fact){
            icfgNode = node;
            datafact = fact;
        }

        void setICFGNode(ICFGNode* node){
            icfgNode = node;
        }

        void setDataFact (Datafact fact){
            datafact = fact;
        }

        ICFGNode* getICFGNode() {
            return icfgNode;
        }

        Datafact getDataFact() {
            return datafact;
        }

    };

// in order to denote : <node1, d1> --> <node2, d2>
    class PathEdge{
        PathNode* srcNode;
        PathNode* dstNode;

    public:
        PathEdge();

        PathEdge(PathNode* src, PathNode* dst){
            srcNode = src;
            dstNode = dst;
        }

        void setStartPathNode(PathNode* node){
            srcNode = node;
        }

        void setEndPathNode(PathNode* node){
            dstNode = node;
        }

        PathNode* getDstPathNode(){
            return dstNode;
        }

        PathNode* getSrcPathNode(){
            return srcNode;
        }

    };
};

#endif /* INCLUDE_WPA_IFDS_H_ */
