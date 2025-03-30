//===- SVFIR.cpp -- Program assignment graph------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFIR.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/IRGraph.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

void IRGraph::destorySymTable()
{

    for (auto &pair: objTypeInfoMap)
    {
        if (ObjTypeInfo* ti = pair.second)
            delete ti;
    }

    for (const SVFType* type : svfTypes)
        delete type;
    svfTypes.clear();

    for (const StInfo* st : stInfos)
        delete st;
    stInfos.clear();
}

IRGraph::~IRGraph()
{
    destorySymTable();
}


NodeID IRGraph::getReturnNode(const FunObjVar *func) const
{
    FunObjVarToIDMapTy::const_iterator iter =  returnFunObjSymMap.find(func);
    assert(iter!=returnFunObjSymMap.end() && "ret sym not found");
    return iter->second;
}

NodeID IRGraph::getVarargNode(const FunObjVar *func) const
{
    FunObjVarToIDMapTy::const_iterator iter =  varargFunObjSymMap.find(func);
    assert(iter!=varargFunObjSymMap.end() && "vararg sym not found");
    return iter->second;
}
void IRGraph::printFlattenFields(const SVFType *type)
{
    if (const SVFArrayType* at = SVFUtil::dyn_cast<SVFArrayType>(type))
    {
        outs() << "  {Type: " << *at << "}\n"
               << "\tarray type "
               << "\t [element size = " << getNumOfFlattenElements(at) << "]\n"
               << "\n";
    }
    else if (const SVFStructType *st = SVFUtil::dyn_cast<SVFStructType>(type))
    {
        outs() <<"  {Type: " << *st << "}\n";
        const std::vector<const SVFType*>& finfo = getTypeInfo(st)->getFlattenFieldTypes();
        int field_idx = 0;
        for(const SVFType* type : finfo)
        {
            outs() << " \tField_idx = " << ++field_idx
                   << ", field type: " << *type << "\n";
        }
        outs() << "\n";
    }
    else if (const SVFPointerType* pt= SVFUtil::dyn_cast<SVFPointerType>(type))
    {
        outs() << *pt << "\n";
    }
    else if (const SVFFunctionType* fu =
                 SVFUtil::dyn_cast<SVFFunctionType>(type))
    {
        outs() << "  {Type: " << *fu << "}\n\n";
    }
    else if (const SVFOtherType* ot = SVFUtil::dyn_cast<SVFOtherType>(type))
    {
        outs() << "  {Type: "<< *ot << "(SVFOtherType)}\n\n";
    }
    else
    {
        assert(type->isSingleValueType() && "not a single value type, then what else!!");
        /// All rest types are scalar type?
        u32_t eSize = getNumOfFlattenElements(type);
        outs() << "  {Type: " << *type << "}\n"
               << "\t [object size = " << eSize << "]\n"
               << "\n";
    }
}

const std::vector<const SVFType *> &IRGraph::getFlattenFieldTypes(const SVFStructType *T)
{
    return getTypeInfo(T)->getFlattenFieldTypes();
}

const SVFType *IRGraph::getFlatternedElemType(const SVFType *baseType, u32_t flatten_idx)
{
    if(Options::ModelArrays())
    {
        const std::vector<const SVFType*>& so = getTypeInfo(baseType)->getFlattenElementTypes();
        assert (flatten_idx < so.size() && !so.empty() && "element index out of bounds or struct opaque type, can't get element type!");
        return so[flatten_idx];
    }
    else
    {
        const std::vector<const SVFType*>& so = getTypeInfo(baseType)->getFlattenFieldTypes();
        assert (flatten_idx < so.size() && !so.empty() && "element index out of bounds or struct opaque type, can't get element type!");
        return so[flatten_idx];
    }
}

const SVFType *IRGraph::getOriginalElemType(const SVFType *baseType, u32_t origId) const
{
    return getTypeInfo(baseType)->getOriginalElemType(origId);
}

u32_t IRGraph::getFlattenedElemIdx(const SVFType *T, u32_t origId)
{
    if(Options::ModelArrays())
    {
        const std::vector<u32_t>& so = getTypeInfo(T)->getFlattenedElemIdxVec();
        assert ((unsigned)origId < so.size() && !so.empty() && "element index out of bounds, can't get flattened index!");
        return so[origId];
    }
    else
    {
        if(SVFUtil::isa<SVFStructType>(T))
        {
            const std::vector<u32_t>& so = getTypeInfo(T)->getFlattenedFieldIdxVec();
            assert ((unsigned)origId < so.size() && !so.empty() && "Struct index out of bounds, can't get flattened index!");
            return so[origId];
        }
        else
        {
            /// When Options::ModelArrays is disabled, any element index Array is modeled as the base
            assert(SVFUtil::isa<SVFArrayType>(T) && "Only accept struct or array type if Options::ModelArrays is disabled!");
            return 0;
        }
    }
}

u32_t IRGraph::getNumOfFlattenElements(const SVFType *T)
{
    if(Options::ModelArrays())
        return getTypeInfo(T)->getNumOfFlattenElements();
    else
        return getTypeInfo(T)->getNumOfFlattenFields();
}

const ObjTypeInfo *IRGraph::createDummyObjTypeInfo(NodeID symId, const SVFType *type)
{
    if (objTypeInfoMap.find(symId)==objTypeInfoMap.end())
    {
        ObjTypeInfo* ti = createObjTypeInfo(type);
        objTypeInfoMap[symId] = ti;
    }
    ObjTypeInfo* ti = objTypeInfoMap[symId];
    return ti;
}

APOffset IRGraph::getModulusOffset(const BaseObjVar *baseObj, const APOffset &apOffset)
{
    /// if the offset is negative, it's possible that we're looking for an obj node out of range
    /// of current struct. Make the offset positive so we can still get a node within current
    /// struct to represent this obj.

    APOffset offset = apOffset;
    if(offset < 0)
    {
        writeWrnMsg("try to create a gep node with negative offset.");
        offset = std::abs(offset);
    }
    u32_t maxOffset = baseObj->getMaxFieldOffsetLimit();

    /*!
     * @offset: the index allocated to the newly generated field node;
     * @Options::MaxFieldLimit(): preset upper bound of field number;
     * @maxOffset: the max field number of the base object;
     */
    if (maxOffset == 0)
        offset = 0;
    else if (Options::MaxFieldLimit() < maxOffset)
        /*!
         * E.g., offset == 260, maxOffset == 270, Options::MaxFieldLimit() == 256 ==> offset = 4
         */
        offset = offset % maxOffset;
    else if ((u32_t)offset > maxOffset - 1)
    {
        if (Options::CyclicFldIdx())
            /*!
             * E.g., offset == 100, maxOffset == 98, Options::MaxFieldLimit() == 256 ==> offset = 2
             */
            offset = offset % maxOffset;
        else
            /*!
             * E.g., offset == 100, maxOffset == 98, Options::MaxFieldLimit() == 256 ==> offset = 97
             */
            offset = maxOffset - 1;
    }

    return offset;
}

ObjTypeInfo *IRGraph::createObjTypeInfo(const SVFType *type)
{

    ObjTypeInfo* typeInfo = new ObjTypeInfo(type, Options::MaxFieldLimit());
    if(type && type->isPointerTy())
    {
        typeInfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    }
    return typeInfo;
}

const StInfo *IRGraph::getTypeInfo(const SVFType *T) const
{
    assert(T);
    SVFTypeSet::const_iterator it = svfTypes.find(T);
    assert(it != svfTypes.end() && "type info not found? collect them first during SVFIR Building");
    return (*it)->getTypeInfo();
}

/*!
 * Add a SVFIR edge into edge map
 */
bool IRGraph::addEdge(SVFVar* src, SVFVar* dst, SVFStmt* edge)
{

    DBOUT(DPAGBuild,
          outs() << "add edge from " << src->getId() << " kind :"
          << src->getNodeKind() << " to " << dst->getId()
          << " kind :" << dst->getNodeKind() << "\n");
    src->addOutEdge(edge);
    dst->addInEdge(edge);
    return true;
}

/*!
 * Return true if it is an intra-procedural edge
 */
SVFStmt* IRGraph::hasNonlabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind)
{
    SVFStmt edge(src,dst,kind, false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Return an MultiOpndStmt if found
 */
SVFStmt* IRGraph::hasLabeledEdge(SVFVar* src, SVFVar* op1, SVFStmt::PEDGEK kind, const SVFVar* op2)
{
    SVFStmt edge(src,op1,SVFStmt::makeEdgeFlagWithAddionalOpnd(kind,op2), false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Return an inter-procedural edge if found
 */
SVFStmt* IRGraph::hasLabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind, const ICFGNode* callInst)
{
    SVFStmt edge(src,dst,SVFStmt::makeEdgeFlagWithCallInst(kind,callInst), false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

SVFStmt* IRGraph::hasEdge(SVFStmt* edge, SVFStmt::PEDGEK kind)
{
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}


/*!
 * Dump this IRGraph
 */
void IRGraph::dump(std::string name)
{
    GraphPrinter::WriteGraphToFile(outs(), name, this);
}

/*!
 * View IRGraph
 */
void IRGraph::view()
{
    SVF::ViewGraph(this, "ProgramAssignmentGraph");
}


u32_t IRGraph::getValueNodeNum()
{
    if (valVarNum != 0) return valVarNum;
    u32_t num = 0;
    for (const auto& item: *this)
    {
        if (SVFUtil::isa<ValVar>(item.second))
            num++;
    }
    return valVarNum = num;
}


u32_t IRGraph::getObjectNodeNum()
{
    if (objVarNum != 0) return objVarNum;
    u32_t num = 0;
    for (const auto& item: *this)
    {
        if (SVFUtil::isa<ObjVar>(item.second))
            num++;
    }
    return objVarNum = num;
}



namespace SVF
{
/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<IRGraph*> : public DefaultDOTGraphTraits
{

    typedef SVFVar NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(IRGraph *graph)
    {
        return graph->getGraphName();
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
    static bool isNodeHidden(SVFVar *node, IRGraph *)
    {
        if (Options::ShowHiddenNode()) return false;
        else return node->isIsolatedNode();
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(SVFVar *node, IRGraph*)
    {
        std::string str;
        std::stringstream rawstr(str);
        // print function info
        if (node->getFunction())
            rawstr << "[" << node->getFunction()->getName() << "] ";

        rawstr << node->toString();

        return rawstr.str();

    }

    static std::string getNodeAttributes(SVFVar *node, IRGraph*)
    {
        if (SVFUtil::isa<ValVar>(node))
        {
            if(SVFUtil::isa<GepValVar>(node))
                return "shape=hexagon";
            else if (SVFUtil::isa<DummyValVar>(node))
                return "shape=diamond";
            else
                return "shape=box";
        }
        else if (SVFUtil::isa<ObjVar>(node))
        {
            if(SVFUtil::isa<GepObjVar>(node))
                return "shape=doubleoctagon";
            else if(SVFUtil::isa<BaseObjVar>(node))
                return "shape=box3d";
            else if (SVFUtil::isa<DummyObjVar>(node))
                return "shape=tab";
            else
                return "shape=component";
        }
        else if (SVFUtil::isa<RetValPN>(node))
        {
            return "shape=Mrecord";
        }
        else if (SVFUtil::isa<VarArgValPN>(node))
        {
            return "shape=octagon";
        }
        else
        {
            assert(0 && "no such kind!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(SVFVar*, EdgeIter EI, IRGraph*)
    {
        const SVFStmt* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<AddrStmt>(edge))
        {
            return "color=green";
        }
        else if (SVFUtil::isa<CopyStmt>(edge))
        {
            return "color=black";
        }
        else if (SVFUtil::isa<GepStmt>(edge))
        {
            return "color=purple";
        }
        else if (SVFUtil::isa<StoreStmt>(edge))
        {
            return "color=blue";
        }
        else if (SVFUtil::isa<LoadStmt>(edge))
        {
            return "color=red";
        }
        else if (SVFUtil::isa<PhiStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<SelectStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<CmpStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<BinaryOPStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<UnaryOPStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<BranchStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<TDForkPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<TDJoinPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<CallPE>(edge))
        {
            return "color=black,style=dashed";
        }
        else if (SVFUtil::isa<RetPE>(edge))
        {
            return "color=black,style=dotted";
        }

        assert(false && "No such kind edge!!");
        exit(1);
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(SVFVar*, EdgeIter EI)
    {
        const SVFStmt* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if(const CallPE* calledge = SVFUtil::dyn_cast<CallPE>(edge))
        {
            return calledge->getCallSite()->getSourceLoc();
        }
        else if(const RetPE* retedge = SVFUtil::dyn_cast<RetPE>(edge))
        {
            return retedge->getCallSite()->getSourceLoc();
        }
        return "";
    }
};
} // End namespace llvm
