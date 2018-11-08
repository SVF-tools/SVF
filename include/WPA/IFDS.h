/*
 * IFDS.h
 *
 */

#ifndef INCLUDE_WPA_IFDS_H_
#define INCLUDE_WPA_IFDS_H_


#include "Util/ICFG.h"
#include "WPA/Andersen.h"
#include <iostream>

class IFDS {

private:
	ICFG* icfg;
	PointerAnalysis* pta;

public:
    class PathNode;
    class PathEdge;
    typedef std::string ValueName;          //(stmtNode->getPAGDstNode()->getValueName())
    typedef std::set<const PAGNode*> Datafact;    //set of uninitialized variables at ICFGNode
    typedef std::set<Datafact> Facts;       //different datafacts from different path
    typedef std::set<const ICFGNode*> ICFGNodeSet;
    typedef std::list<PathEdge*> PathEdgeSet;
    typedef std::map<const ICFGNode*, Facts> ICFGNodeToDataFactsMap;

protected:
    PathEdgeSet WorkList;         //worklist used during the tabulation algorithm
    PathEdgeSet PathEdgeList;     //used to restore all PathEdges (result)
    PathEdgeSet SummaryEdgeList;  //used to restore all SummaryEdges
    ICFGNodeSet ICFGDstNodeSet;
    ICFGNodeSet SummaryICFGDstNodeSet;
    ICFGNodeToDataFactsMap ICFGNodeToFacts;
    ICFGNodeToDataFactsMap SummaryICFGNodeToFacts;
    Facts facts, facts2;    // Datafacts for a given ICFGNode
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

    //add new PathEdge components into PathEdgeList and WorkList
    void propagate(PathNode* srcPN, ICFGNode* succ, Datafact d);

    bool isNotInSummaryEdgeList(ICFGNode* n1, Datafact d1, ICFGNode* n2, Datafact d2);

    //get all ICFGNode in all EndPathNode of PathEdgeList
    ICFGNodeSet& getDstICFGNodeSet();

    ICFGNodeSet& getSummaryDstICFGNodeSet();

    //transfer function of given ICFGNode
    Datafact transferFun(PathNode* pathNode);

    //whether the variable is initialized
    bool isInitialized(const PAGNode* pagNode, Datafact datafact);

    //print ICFGNodes and theirs datafacts
    void printRes();

    //Get points-to set of given PAGNode
    inline PointsTo& getPts(NodeID id) {
        return pta->getPts(id);
    }

    ICFGNode* getRetNode(ICFGNode* call);

    // in order to denote : <node, d> , d here is datafact before the execution of node
    class PathNode{
        const ICFGNode* icfgNode;
        Datafact datafact;

        //Constructor
    public:
        PathNode();

        PathNode(const ICFGNode* node, Datafact fact){
            icfgNode = node;
            datafact = fact;
        }

        void setICFGNode(ICFGNode* node){
            icfgNode = node;
        }

        void setDataFact (Datafact fact){
            datafact = fact;
        }

        const ICFGNode* getICFGNode() const{
            return icfgNode;
        }

        Datafact& getDataFact() {
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

        PathNode* getDstPathNode() const{
            return dstNode;
        }

        PathNode* getSrcPathNode() const{
            return srcNode;
        }
    };
};

#endif /* INCLUDE_WPA_IFDS_H_ */