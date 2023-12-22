//
// Created by Xiao and Jiawei on 2023/5/29.
//

#ifndef SVF_SINGLEABSVALUE_H
#define SVF_SINGLEABSVALUE_H

#include "AbstractExecution/BoundedZ3Expr.h"

namespace SVF
{

/*!
 * Atom Z3 expr for constant execution state
 */
class SingleAbsValue : public BoundedZ3Expr
{
public:

    SingleAbsValue() = default;

    SingleAbsValue(const Z3Expr &z3Expr) : BoundedZ3Expr(z3Expr.getExpr()) {}

    SingleAbsValue(const BoundedZ3Expr &z3Bound) : BoundedZ3Expr(z3Bound) {}

    SingleAbsValue(const z3::expr &e) : BoundedZ3Expr(e) {}

    SingleAbsValue(s32_t i) : BoundedZ3Expr(i) {}

    SingleAbsValue(const SingleAbsValue &_z3Expr) : BoundedZ3Expr(_z3Expr) {}

    inline SingleAbsValue &operator=(const SingleAbsValue &rhs)
    {
        BoundedZ3Expr::operator=(rhs);
        return *this;
    }

    SingleAbsValue(SingleAbsValue &&_z3Expr) : BoundedZ3Expr(_z3Expr)
    {
    }

    inline SingleAbsValue &operator=(SingleAbsValue &&rhs)
    {
        BoundedZ3Expr::operator=(rhs);
        return *this;
    }

    static z3::context &getContext()
    {
        return BoundedZ3Expr::getContext();
    }

    static SingleAbsValue topConstant()
    {
        return getContext().int_const("⊤");
    }

    static SingleAbsValue bottomConstant()
    {
        return getContext().int_const("⊥");
    }

    void join_with(const SingleAbsValue& other)
    {
        if (this->isBottom())
        {
            if (other.isBottom())
            {
                return;
            }
            else
            {
                *this = other;
            }
        }
        else if (other.isBottom())
        {
            return;
        }
        else
        {
            if (!eq(*this, other))
            {
                set_to_top();
            }
        }
    }

    void set_to_top()
    {
        *this = topConstant();
    }

    static bool isTopAbsValue(const SingleAbsValue &expr)
    {
        return eq(expr, topConstant());
    }

    static bool isBottomAbsValue(const SingleAbsValue &expr)
    {
        return eq(expr, bottomConstant());
    }

    inline bool isBottom() const
    {
        return isBottomAbsValue(*this);
    }

    inline bool isTop() const
    {
        return isTopAbsValue(*this);
    }

    inline bool isSym() const
    {
        return isSymbolAbsValue(*this);
    }

    static bool isSymbolAbsValue(const SingleAbsValue &expr)
    {
        return !eq(expr, topConstant()) && !eq(expr, bottomConstant()) && !expr.is_numeral();
    }

    /// Less then or equal
    bool leq(const SingleAbsValue &rhs) const
    {
        assert(is_numeral() && rhs.is_numeral());
        return (getExpr() <= rhs.getExpr()).simplify().is_true();
    }

    // Greater than or equal
    bool geq(const SingleAbsValue &rhs) const
    {
        assert(is_numeral() && rhs.is_numeral());
        return (getExpr() >= rhs.getExpr()).simplify().is_true();
    }


    /// Reload operator
    //{%
    friend SingleAbsValue operator==(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
        {
            return topConstant();
        }
        else
        {
            return static_cast<BoundedZ3Expr>(lhs) == static_cast<BoundedZ3Expr>(rhs);
        }
    }

    friend SingleAbsValue operator!=(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) != static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator>(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) > static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator<(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) < static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator<=(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) <= static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator>=(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) >= static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator+(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) + static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator-(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) - static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator*(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isZero(lhs) || isZero(rhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) * static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator/(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || isZero(rhs))
        {
            return bottomConstant();
        }
        else if (isZero(lhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) / static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator%(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || isZero(rhs))
        {
            return bottomConstant();
        }
        else if (isZero(lhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) % static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator^(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
            return bottomConstant();
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) ^ static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator&(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (isZero(lhs) || isZero(rhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) & static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator|(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if ((lhs.is_numeral() && eq(lhs, -1)) ||
                 (rhs.is_numeral() && eq(rhs, -1)))
            return -1;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) | static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue ashr(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || (rhs.is_numeral() && !rhs.geq(0)))
            return bottomConstant();
        else if (isZero(lhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return ashr(static_cast<BoundedZ3Expr>(lhs), static_cast<BoundedZ3Expr>(rhs));
    }

    friend SingleAbsValue shl(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || (rhs.is_numeral() && !rhs.geq(0)))
            return bottomConstant();
        else if (isZero(lhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return shl(static_cast<BoundedZ3Expr>(lhs), static_cast<BoundedZ3Expr>(rhs));
    }

    friend SingleAbsValue lshr(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || (rhs.is_numeral() && !rhs.geq(0)))
            return bottomConstant();
        else if (isZero(lhs))
            return 0;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return lshr(static_cast<BoundedZ3Expr>(lhs), static_cast<BoundedZ3Expr>(rhs));
    }

    friend SingleAbsValue int2bv(u32_t n, const SingleAbsValue &e)
    {
        if (isBottomAbsValue(e))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(e))
            return topConstant();
        else
            return int2bv(n, static_cast<BoundedZ3Expr>(e));
    }

    friend SingleAbsValue bv2int(const SingleAbsValue &e, bool isSigned)
    {
        if (isBottomAbsValue(e))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(e))
            return topConstant();
        else
            return bv2int(static_cast<BoundedZ3Expr>(e), isSigned);
    }

    friend SingleAbsValue operator&&(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (eq(lhs, getContext().bool_val(false)) || eq(rhs, getContext().bool_val(false)))
            return getContext().bool_val(false);
        else if (eq(lhs, getContext().bool_val(true)))
            return rhs;
        else if (eq(rhs, getContext().bool_val(true)))
            return lhs;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) && static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator||(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs))
        {
            return bottomConstant();
        }
        else if (eq(lhs, getContext().bool_val(true)) || eq(rhs, getContext().bool_val(true)))
            return getContext().bool_val(true);
        else if (eq(lhs, getContext().bool_val(false)))
            return rhs;
        else if (eq(rhs, getContext().bool_val(false)))
            return lhs;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs))
            return topConstant();
        else
            return static_cast<BoundedZ3Expr>(lhs) || static_cast<BoundedZ3Expr>(rhs);
    }

    friend SingleAbsValue operator!(const SingleAbsValue &lhs)
    {
        if (isBottomAbsValue(lhs))
        {
            return bottomConstant();
        }
        else if (isTopAbsValue(lhs))
            return topConstant();
        else
            return !static_cast<BoundedZ3Expr>(lhs);
    }

    friend SingleAbsValue ite(const SingleAbsValue &cond, const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        if (isBottomAbsValue(lhs) || isBottomAbsValue(rhs) || isBottomAbsValue(cond))
        {
            return bottomConstant();
        }
        else if (eq(cond, getContext().bool_val(true)))
            return lhs;
        else if (eq(cond, getContext().bool_val(false)))
            return rhs;
        else if (isTopAbsValue(lhs) || isTopAbsValue(rhs) || isTopAbsValue(cond))
            return topConstant();
        else
            return ite(static_cast<BoundedZ3Expr>(cond), static_cast<BoundedZ3Expr>(lhs),
                       static_cast<BoundedZ3Expr>(rhs));
    }

    friend std::ostream &operator<<(std::ostream &out, const SingleAbsValue &expr)
    {
        out << static_cast<BoundedZ3Expr>(expr);
        return out;
    }

    friend bool eq(const SingleAbsValue &lhs, const SingleAbsValue &rhs)
    {
        return eq(static_cast<BoundedZ3Expr>(lhs), static_cast<BoundedZ3Expr>(rhs));
    }

    inline SingleAbsValue simplify() const
    {
        return getExpr().simplify();
    }
    //%}
}; // end class ConZ3Expr
} // end namespace SVF

/// Specialise hash for ConZ3Expr
template<>
struct std::hash<SVF::SingleAbsValue>
{
    size_t operator()(const SVF::SingleAbsValue &z3Expr) const
    {
        return z3Expr.hash();
    }
};


#endif //SVF_SINGLEABSVALUE_H
