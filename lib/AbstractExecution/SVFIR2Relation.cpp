//
// Created by Xiao on 2022/8/18.
//

#include "AbstractExecution/SVFIR2ItvExeState.h"

using namespace SVF;
using namespace SVFUtil;

void SVFIR2ItvExeState::translateBinaryRel(const BinaryOPStmt *binary)
{
    NodeID op0 = binary->getOpVarID(0);
    NodeID op1 = binary->getOpVarID(1);
    NodeID res = binary->getResID();
    Z3Expr &rel_res = _relEs[res];
    const Z3Expr &rel_value0 = _relEs.toZ3Expr(op0);
    const Z3Expr &rel_value1 = _relEs.toZ3Expr(op1);
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    {
        rel_res = rel_value0 + rel_value1;
        break;
    }
    case BinaryOPStmt::Sub:
    {
        rel_res = rel_value0 - rel_value1;
        break;
    }
    case BinaryOPStmt::Mul:
    {
        rel_res = rel_value0 * rel_value1;
        break;
    }
    case BinaryOPStmt::SDiv:
    {
        rel_res = rel_value0 / rel_value1;
        break;
    }
    case BinaryOPStmt::SRem:
        break;
    case BinaryOPStmt::Xor:
        break;
    case BinaryOPStmt::And:
        break;
    case BinaryOPStmt::Or:
        break;
    case BinaryOPStmt::AShr:
        break;
    case BinaryOPStmt::Shl:
        break;
    default:
    {
        assert(false && "implement this part");
        abort();
    }
    }
}

void SVFIR2ItvExeState::translateCmpRel(const CmpStmt *cmp)
{
    NodeID op0 = cmp->getOpVarID(0);
    NodeID op1 = cmp->getOpVarID(1);
    NodeID res_id = cmp->getResID();
    u32_t predicate = cmp->getPredicate();
    Z3Expr &rel_res = _relEs[res_id];
    const Z3Expr &rel_value0 = _relEs.toZ3Expr(op0);
    const Z3Expr &rel_value1 = _relEs.toZ3Expr(op1);
    switch (predicate)
    {
    case CmpStmt::ICMP_EQ:
    {
        rel_res = ite(rel_value0 == rel_value1, 1, 0);
        break;
    }
    case CmpStmt::ICMP_NE:
    {
        rel_res = ite(rel_value0 != rel_value1, 1, 0);
        break;
    }
    case CmpStmt::ICMP_UGT:
    case CmpStmt::ICMP_SGT:
    {
        rel_res = ite(rel_value0 > rel_value1, 1, 0);
        break;
    }
    case CmpStmt::ICMP_UGE:
    case CmpStmt::ICMP_SGE:
    {
        rel_res = ite(rel_value0 >= rel_value1, 1, 0);
        break;
    }
    case CmpStmt::ICMP_ULT:
    case CmpStmt::ICMP_SLT:
    {
        rel_res = ite(rel_value0 < rel_value1, 1, 0);
        break;
    }
    case CmpStmt::ICMP_ULE:
    case CmpStmt::ICMP_SLE:
    {
        rel_res = ite(rel_value0 <= rel_value1, 1, 0);
        break;
    }
    default:
        assert(false && "implement this part");
        abort();
    }
}

void SVFIR2ItvExeState::translateLoadRel(const LoadStmt *load)
{
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    _relEs[lhs] = _relEs.load(Z3Expr((int) _es[rhs].lb().getNumeral()));
}

void SVFIR2ItvExeState::translateStoreRel(const StoreStmt *store)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    assert(_es[lhs].is_numeral() && "loc not numeral?");
    _relEs.store(Z3Expr((int) _es[lhs].lb().getNumeral()), _relEs.toZ3Expr(rhs));

}

void SVFIR2ItvExeState::translateCopyRel(const CopyStmt *copy)
{
    u32_t rhs = copy->getRHSVarID();
    u32_t lhs = copy->getLHSVarID();
    _relEs[lhs] = _relEs.toZ3Expr(rhs);
}

void SVFIR2ItvExeState::translateSelectRel(const SelectStmt *select)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    if (_es[cond].is_numeral())
    {
        _relEs[res] = _es[cond].is_zero() ? _relEs.toZ3Expr(fval) : _relEs.toZ3Expr(tval);
    }
    else
    {

    }
}

void SVFIR2ItvExeState::translatePhiRel(const PhiStmt *phi, const ICFGNode *srcNode,
                                        const std::vector<const ICFGEdge *> &path)
{
    u32_t res = phi->getResID();
    std::unordered_map<const ICFGNode *, NodeID> candidate_ids;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        candidate_ids[phi->getOpICFGNode(i)] = phi->getOpVarID(i);
    }
    for (auto it = path.end() - 1; it != path.begin(); --it)
    {
        auto cur_node = (*it)->getDstNode();
        if (candidate_ids.find(cur_node) != candidate_ids.end())
        {
            if (_es.getVarToVal().find(res) == _es.getVarToVal().end())
            {
                _relEs[res] = _relEs.toZ3Expr(candidate_ids[cur_node]);
            }
            else
            {

            }
            return;
        }
    }
    assert(false && "predecessor ICFGNode of this PhiStmt not found?");
}

void SVFIR2ItvExeState::translateCallRel(const CallPE *callPE)
{
    u32_t rhs = callPE->getRHSVarID();
    u32_t lhs = callPE->getLHSVarID();
    _relEs[lhs] = _relEs.toZ3Expr(rhs);
}

void SVFIR2ItvExeState::translateRetRel(const RetPE *retPE)
{
    u32_t lhs = retPE->getLHSVarID();
    u32_t rhs = retPE->getRHSVarID();
    _relEs[lhs] = _relEs.toZ3Expr(rhs);
}


