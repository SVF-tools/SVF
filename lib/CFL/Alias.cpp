/* -------------------- Alias.cpp ------------------ */
//
// Created by kisslune on 3/4/22.
//

#include "CFL/Alias.h"
#include "Graphs/ConsG.h"

using namespace SVF;

void AliasSolver::buildGraph()
{
    SVFIR *pag = SVFIR::getPAG();
    ConstraintGraph *consg = new ConstraintGraph(pag);

    for (auto nodeit = consg->begin(); nodeit != consg->end(); ++nodeit) {
        ConstraintNode *node = nodeit->second;
        graph()->addEdge(node->getId(), node->getId(), epsilon);

        for (auto edge: node->getAddrOutEdges()) {
            graph()->addEdge(node->getId(), edge->getDstID(), dbar);
            graph()->addEdge(edge->getDstID(), node->getId(), d);
        }
        for (auto edge: node->getLoadOutEdges()) {
            graph()->addEdge(node->getId(), edge->getDstID(), d);
            graph()->addEdge(edge->getDstID(), node->getId(), dbar);
        }
        for (auto edge: node->getStoreOutEdges()) {
            graph()->addEdge(node->getId(), edge->getDstID(), dbar);
            graph()->addEdge(edge->getDstID(), node->getId(), d);
        }
        for (auto edge: node->getCopyOutEdges()) {
            graph()->addEdge(node->getId(), edge->getDstID(), a);
            graph()->addEdge(edge->getDstID(), node->getId(), abar);
        }
        for (auto edge: node->getGepOutEdges()) {
            if (SVFUtil::isa<NormalGepCGEdge>(edge)) {
                NormalGepCGEdge *gepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge);
                graph()->addEdge(node->getId(), edge->getDstID(), f,
                                 gepEdge->getLocationSet().accumulateConstantFieldIdx());
                graph()->addEdge(edge->getDstID(), node->getId(), fbar,
                                 gepEdge->getLocationSet().accumulateConstantFieldIdx());
            } else {
                graph()->addEdge(node->getId(), edge->getDstID(), a);
                graph()->addEdge(edge->getDstID(), node->getId(), abar);
            }
        }
    }
}


/*!
 *
 */
Label AliasSolver::unaryDerivation(Label lbl)
{
    if (lbl.first == epsilon)
        return std::make_pair(V, 0);
    if (lbl.first == a)
        return std::make_pair(A, 0);
    if (lbl.first == abar)
        return std::make_pair(Abar,0);
    if (lbl.first == M)
        return std::make_pair(V, 0);

    return std::make_pair(fault, 0);
}


/*!
 *
 */
Label AliasSolver::binaryDerivation(Label llbl, Label rlbl)
{
    LabelType lty = llbl.first, rty = rlbl.first;
    if (lty == dbar && rty == V)
        return std::make_pair(DV, 0);
    if (lty == DV && rty == d)
        return std::make_pair(M,0);
    if (lty == a && rty == M)
        return std::make_pair(A,0);
    if (lty == A && rty == A)
        return std::make_pair(A,0);
    if (lty == M && rty == abar)
        return std::make_pair(Abar,0);
    if (lty == Abar && rty == Abar)
        return std::make_pair(Abar,0);
    if (lty == Abar && rty == V)
        return std::make_pair(V,0);
    if (lty == V && rty == A)
        return std::make_pair(V,0);
    if (lty == fbar && rty == V)
        return std::make_pair(FV, llbl.second);
    if (lty == FV && rty == f && llbl.second == rlbl.second)
        return std::make_pair(V,0);

    return std::make_pair(fault,0);
}
