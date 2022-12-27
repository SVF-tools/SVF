//===- ConditionalPT.h -- Conditional points-to data structure----------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * ConditionalPT.h
 *
 *  Created on: 22/03/2014
 *      Author: Yulei Sui
 */

#ifndef CONDVAR_H_
#define CONDVAR_H_

#include "Util/SVFUtil.h"
#include "MemoryModel/PointsTo.h"
#include <sstream>

namespace SVF
{

/*!
 * Conditional Variable (c,v)
 * A context/path condition
 * A variable NodeID
 */
template<class Cond>
class CondVar
{
public:
    /// Constructor
    CondVar(const Cond& cond, NodeID id) : m_cond(cond),m_id(id)
    {
    }
    /// Copy constructor
    CondVar(const CondVar& conVar) : m_cond(conVar.m_cond), m_id(conVar.m_id)
    {
    }
    /// Default constructor
    CondVar() : m_cond(), m_id(0) {}

    ~CondVar() {}

    /**
     * Comparison between two elements.
     */
    ///@{
    inline bool operator == (const CondVar & rhs) const
    {
        return (m_id == rhs.get_id() && m_cond == rhs.get_cond());
    }

    inline bool operator != (const CondVar & rhs) const
    {
        return !(*this == rhs);
    }
    ///@}


    inline CondVar& operator= (const CondVar& rhs)
    {
        if(*this!=rhs)
        {
            m_cond = rhs.get_cond();
            m_id = rhs.get_id();
        }
        return *this;
    }


    /**
     * Less than implementation.
     */
    inline bool operator < (const CondVar & rhs) const
    {
        if(m_id != rhs.get_id())
            return m_id < rhs.get_id();
        else
            return m_cond < rhs.get_cond();
    }

    inline const Cond& get_cond() const
    {
        return m_cond;
    }
    inline NodeID get_id() const
    {
        return m_id;
    }

    inline std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "<" << m_id << " " << m_cond.toString() << "> ";
        return rawstr.str();
    }

    friend OutStream& operator<< (OutStream &o, const CondVar<Cond> &cvar)
    {
        o << cvar.toString();
        return o;
    }
private:
    Cond	m_cond;
    NodeID	m_id;
};

/*!
 * Conditional variable set
 */
template<class Element>
class CondStdSet
{
    typedef OrderedSet<Element> ElementSet;

public:
    typedef typename OrderedSet<Element>::iterator iterator;
    typedef typename OrderedSet<Element>::const_iterator const_iterator;

    CondStdSet() {}
    ~CondStdSet() {}

    /// Copy constructor
    CondStdSet(const CondStdSet<Element>& cptsSet) : elements(cptsSet.getElementSet())
    {
    }

    /// Return true if the element is added
    inline bool test_and_set(const Element& var)
    {
        return elements.insert(var).second;
    }
    /// Return true if the element is in the set
    inline bool test(const Element& var) const
    {
        return (elements.find(var) != elements.end());
    }
    /// Add the element into set
    inline void set(const Element& var)
    {
        elements.insert(var);
    }
    /// Remove var from the set.
    inline void reset(const Element& var)
    {
        elements.erase(var);
    }

    /// Set size
    //@{
    inline bool empty() const
    {
        return elements.empty();
    }
    inline unsigned size() const
    {
        return elements.size();
    }
    inline unsigned count() const
    {
        return size();
    }
    //@}

    /// Clear set
    inline void clear()
    {
        elements.clear();
    }

    /// Iterators
    //@{
    inline iterator begin()
    {
        return elements.begin();
    }
    inline iterator end()
    {
        return elements.end();
    }
    inline iterator begin() const
    {
        return elements.begin();
    }
    inline iterator end() const
    {
        return elements.end();
    }
    //@}

    /// Overload operators
    //@{
    inline bool operator|=(const CondStdSet<Element>& rhs)
    {
        const ElementSet& rhsElementSet = rhs.getElementSet();
        if(rhsElementSet.empty() || (*this==rhs))
            return false;
        u32_t oldSize = elements.size();
        elements.insert(rhsElementSet.begin(),rhsElementSet.end());
        return oldSize!=elements.size();
    }
    inline bool operator&=(const CondStdSet<Element>& rhs)
    {
        if(rhs.empty() || (*this == rhs))
            return false;
        bool changed = false;
        for (const_iterator i = rhs.begin(); i != rhs.end(); ++i)
        {
            if (elements.find(*i) == elements.end())
            {
                elements.erase(*i);
                changed = true;
            }
        }
        return changed;
    }
    inline bool operator!=(const CondStdSet<Element>& rhs) const
    {
        return elements!=rhs.getElementSet();
    }
    inline bool operator==(const CondStdSet<Element>& rhs) const
    {
        return ((*this != rhs) == false);
    }
    inline CondStdSet<Element>& operator=(const CondStdSet<Element>& rhs)
    {
        if(*this!=rhs)
        {
            elements = rhs.getElementSet();
        }
        return *this;
    }
    inline bool operator<(const CondStdSet<Element>& rhs) const
    {
        return elements < rhs.getElementSet();
    }
    //@}

    /**
     * Return TRUE if this and RHS share common elements.
     */
    bool intersects(const CondStdSet<Element>& rhs) const
    {
        if (empty() && rhs.empty())
            return false;

        for (const_iterator i = rhs.begin(); i != rhs.end(); ++i)
        {
            if (elements.find(*i) != elements.end())
                return true;
        }
        return false;
    }

    inline std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "{ ";
        for (const_iterator i = elements.begin(); i != elements.end(); ++i)
        {
            rawstr << (*i).toString() << " ";
        }
        rawstr << "} ";
        return rawstr.str();
    }

    inline const ElementSet& getElementSet() const
    {
        return elements;
    }

    /// TODO: dummy to use for PointsTo in the various PTData.
    void checkAndRemap(void) const { }

private:
    ElementSet elements;
};


/*!
 * Conditional Points-to set
 */
template<class Cond>
class CondPointsToSet
{
public:
    typedef Map<Cond, PointsTo> CondPts;
    typedef typename CondPts::iterator CondPtsIter;
    typedef typename CondPts::const_iterator CondPtsConstIter;
    typedef CondVar<Cond> SingleCondVar;

    /// Constructor
    //@{
    CondPointsToSet()
    {
    }
    CondPointsToSet(const Cond& cond, const PointsTo& pts)
    {
        _condPts[cond] |= pts;
    }
    //@}

    /// Copy constructor
    CondPointsToSet(const CondPointsToSet<Cond>& cptsSet)

    {
        _condPts = cptsSet.pointsTo();
    }

    /// Get Conditional PointsTo and starndard points-to
    //@{
    inline CondPts &pointsTo(void)
    {
        return _condPts;
    }
    inline const CondPts &pointsTo(void) const
    {
        return _condPts;
    }
    inline const PointsTo &pointsTo(Cond cond) const
    {
        CondPtsConstIter it = _condPts.find(cond);
        assert(it!=_condPts.end() && "do not have pts of such condition!");
        return it->second;
    }
    inline bool hasPointsTo(Cond cond) const
    {
        return (_condPts.find(cond) != _condPts.end());
    }
    inline PointsTo &pointsTo(Cond cond)
    {
        return _condPts[cond];
    }
    //@}

    /// iterators
    //@{
    inline CondPtsIter cptsBegin()
    {
        return _condPts.begin();
    }
    inline CondPtsIter cptsEnd()
    {
        return _condPts.end();
    }
    inline CondPtsConstIter cptsBegin() const
    {
        return _condPts.begin();
    }
    inline CondPtsConstIter cptsEnd() const
    {
        return _condPts.end();
    }
    //@}

    inline void clear()
    {
        _condPts.clear();
    }

    ///Get number of points-to targets.
    inline unsigned numElement() const
    {
        if (_condPts.empty())
            return 0;
        else
        {
            unsigned num = 0;
            for (CondPtsConstIter it = cptsBegin(); it != cptsEnd(); it++)
            {
                PointsTo pts = it->second;
                num += pts.count();
            }
            return num;
        }
    }
    /// Return true if no element in the set
    inline bool empty() const
    {
        return numElement()==0;
    }


    /// Overloading operators
    //@{
    inline CondPointsToSet<Cond>& operator=(
        const CondPointsToSet<Cond>& other)
    {
        _condPts = other.pointsTo();
        return *this;
    }

    /// Overloading operator ==
    inline bool operator==(const CondPointsToSet<Cond>& rhs) const
    {
        // Always remember give the typename when define a template variable
        if (pointsTo().size() != rhs.pointsTo().size())
            return false;
        CondPtsConstIter lit = cptsBegin(), elit = cptsEnd();
        CondPtsConstIter rit = rhs.cptsBegin(), erit = rhs.cptsEnd();
        for (; lit != elit && rit != erit; ++lit, ++rit)
        {
            const Cond& lc = lit->first;
            const Cond& rc = rit->first;
            if (lc != rc || lit->second != rit->second)
                return false;
        }
        return true;
    }
    //@}

    /// Two conditional points-to set are aliased when they access the same memory
    /// location under the same condition
    inline bool aliased(const CondPointsToSet<Cond>& rhs) const
    {
        if (pointsTo().empty() || rhs.pointsTo().empty())
            return false;
        else
        {
            CondPtsConstIter lit = cptsBegin(), elit = cptsEnd();
            for (; lit != elit; ++lit)
            {
                const Cond& lc = lit->first;
                const PointsTo& pts = lit->second;
                CondPtsConstIter rit = rhs.pointsTo().find(lc);
                if(rit !=rhs.pointsTo().end())
                {
                    const PointsTo& rpts = rit->second;
                    if (pts.intersects(rpts))
                        return true;
                }
            }
            return false;
        }
    }

    /// Check whether this CondPointsToSet is a subset of RHS
    inline bool isSubset(const CondPointsToSet<Cond>& rhs) const
    {
        if (pointsTo().size() > rhs.pointsTo().size())
            return false;
        else
        {
            CondPtsConstIter lit = cptsBegin(), elit = cptsEnd();
            for (; lit != elit; ++lit)
            {
                const Cond& lc = lit->first;
                CondPtsConstIter rit = rhs.pointsTo().find(lc);
                if (rit == rhs.pointsTo().end())
                    return false;
                else
                {
                    const PointsTo& pts = lit->second;
                    const PointsTo& rpts = rit->second;
                    if (!rpts.contains(pts))
                        return false;
                }
            }
        }
        return true;
    }

    /// Return TRUE if this and RHS share any common element.
    bool intersects(const CondPointsToSet<Cond>* rhs) const
    {
        /// if either cpts is empty, just return.
        if (pointsTo().empty() && rhs->pointsTo().empty())
            return false;

        CondPtsConstIter it = rhs->cptsBegin(), eit = rhs->cptsEnd();
        for (; it != eit; ++it)
        {
            const Cond& cond = it->first;
            if (hasPointsTo(cond))
            {
                const PointsTo& rhs_pts= it->second;
                const PointsTo& pts= pointsTo(cond);
                if (pts.intersects(rhs_pts))
                    return true;
            }
        }

        return false;
    }

    /// Result of cpts1 & ~cpts2 is stored into this bitmap.
    void intersectWithComplement(const CondPointsToSet<Cond>& cpts1, const CondPointsToSet<Cond>& cpts2)
    {
        if (cpts1.pointsTo().empty())
        {
            clear();
            return;
        }
        else if (cpts2.pointsTo().empty())
        {
            (*this) = cpts1;
        }
        else
        {
            CondPtsConstIter it1 = cpts1.cptsBegin(), eit1 = cpts1.cptsEnd();
            for (; it1 != eit1; ++it1)
            {
                const Cond& cond = it1->first;
                const PointsTo& pts1 = it1->second;
                PointsTo& pts = pointsTo(cond);
                if (cpts2.hasPointsTo(cond))
                {
                    const PointsTo& pts2 = cpts2.pointsTo(cond);
                    pts.intersectWithComplement(pts1, pts2);
                }
                else
                {
                    pts = pts1;
                }
            }
        }
    }

    /// Result of cur & ~cpts1 is stored into this bitmap.
    void intersectWithComplement(const CondPointsToSet<Cond>& cpts1)
    {
        /// if either cpts is empty, just return.
        if (empty() || cpts1.pointsTo().empty())
        {
            return;
        }
        else
        {
            CondPtsIter it = cptsBegin(), eit = cptsEnd();
            for (; it != eit; ++it)
            {
                const Cond& cond = it->first;
                PointsTo& pts = it->second;
                if (cpts1.hasPointsTo(cond))
                {
                    const PointsTo& pts1 = cpts1.pointsTo(cond);
                    pts.intersectWithComplement(pts1);
                }
            }
        }
    }

    /// Overloading operator &=
    inline bool operator &= (const CondPointsToSet<Cond>& rhs)
    {
        if (empty())
        {
            return false;
        }
        else if (rhs.empty())
        {
            clear();
            return true;
        }
        else
        {
            bool changed = false;
            for (CondPtsIter it = cptsBegin(), eit = cptsEnd(); it != eit; ++it)
            {
                const Cond& cond = it->first;
                PointsTo& pts = it->second;
                if (rhs.hasPointsTo(cond))
                {
                    if (pts &= rhs.pointsTo(cond))
                        changed = true;
                }
                else
                {
                    if (!pts.empty())
                    {
                        pts.clear();
                        changed = true;
                    }
                }
            }
            return changed;
        }
    }

    /// Overloading operator !=
    inline bool operator!=(const CondPointsToSet<Cond>& rhs)
    {
        return !(*this == rhs);
    }
    //@}


    /// Overloading operator |=
    /// Merge CondPointsToSet of RHS into this one.
    inline bool operator |= (const CondPointsToSet<Cond>& rhs)
    {
        bool changed = false;
        CondPtsConstIter rhsIt = rhs.cptsBegin();
        CondPtsConstIter rhsEit = rhs.cptsEnd();
        for (; rhsIt != rhsEit; ++rhsIt)
        {
            const Cond& cond = rhsIt->first;
            const PointsTo& rhsPts = rhsIt->second;
            PointsTo& pts = pointsTo(cond);
            if ((pts |= rhsPts))
                changed = true;
        }
        return changed;
    }

    /**
     * Compare two CondPointsToSet according to their points-to set size and
     * points-to elements.
     * 1. CondPointsToSet with smaller points-to set size is smaller than the other;
     * 2. If the sizes are equal, comparing the conditions and real points-to targets in
     *    their points-to elements.
     */
    // TODO: try to use an efficient method to compare two conditional points-to set.
    inline bool operator< (const CondPointsToSet<Cond>& rhs) const
    {
        if (pointsTo().size() < rhs.pointsTo().size())
            return true;
        else if (pointsTo().size() == rhs.pointsTo().size())
        {
            CondPtsConstIter lit = cptsBegin(), elit = cptsEnd();
            CondPtsConstIter rit = rhs.cptsBegin(), erit = rhs.cptsEnd();
            for (; lit != elit && rit != erit; ++lit, ++rit)
            {
                const Cond& lc = lit->first;
                const Cond& rc = rit->first;
                if (lc < rc)
                    return true;
                else if (lc == rc)
                {
                    const PointsTo& lpts = lit->second;
                    const PointsTo& rpts = rit->second;
                    if (lpts.count() < rpts.count())
                        return true;
                    else if (lpts.count() == rpts.count())
                    {
                        PointsTo::iterator bit = lpts.begin();
                        PointsTo::iterator eit = lpts.end();
                        PointsTo::iterator rbit = rpts.begin();
                        PointsTo::iterator reit = rpts.end();
                        for (; bit != eit && rbit != reit; bit++, rbit++)
                        {
                            if (*bit < *rbit)
                                return true;
                            else if (*bit > *rbit)
                                return false;
                        }
                    }
                    else
                        return false;
                }
                else
                    return false;
            }
        }
        return false;
    }

    /// Test and set
    //@{
    inline bool test_and_set(const SingleCondVar& var)
    {
        PointsTo& pts = pointsTo(var.get_cond());
        return pts.test_and_set(var.get_id());
    }
    inline bool test(const SingleCondVar& var) const
    {
        if (hasPointsTo(var.get_cond()))
        {
            const PointsTo& pts = pointsTo(var.get_cond());
            return pts.test(var.get_id());
        }
        return false;
    }
    inline void set(const SingleCondVar& var)
    {
        PointsTo& pts = pointsTo(var.get_cond());
        pts.set(var.get_id());
    }
    inline void reset(const SingleCondVar& var)
    {
        if (hasPointsTo(var.get_cond()))
        {
            PointsTo& pts = pointsTo(var.get_cond());
            pts.reset(var.get_id());
        }
    }
    //@}

    // Print all points-to targets
    //@{
    inline void dump(OutStream & O) const
    {
        CondPtsConstIter it = cptsBegin();
        CondPtsConstIter eit = cptsEnd();
        for (; it != eit; it++)
        {
            const PointsTo& pts = it->second;
            O << "pts{";
            SVFUtil::dumpSet(pts, O);
            O << "}";
        }
    }
    inline std::string dumpStr() const
    {
        std::string str;
        CondPtsConstIter it = cptsBegin();
        CondPtsConstIter eit = cptsEnd();
        for (; it != eit; it++)
        {
            const PointsTo& pts = it->second;
            str += "pts{";
            for (PointsTo::iterator ii = pts.begin(), ie = pts.end();
                    ii != ie; ii++)
            {
                char int2str[16];
                snprintf(int2str, sizeof(int2str), "%d", *ii);
                str += int2str;
                str += " ";
            }
            str += "}";
        }
        return str;
    }
    //@}

private:

    /// Conditional Points-to Set Iterator
    class CondPtsSetIterator
    {
    public:
        CondPtsSetIterator(CondPointsToSet<Cond> &n, bool ae = false)
            : _curIter(n.cptsBegin()), _endIter(n.cptsEnd()),
              _ptIter(_curIter->second.begin()), _ptEndIter(_curIter->second.end()), atEnd(ae) {}
        ~CondPtsSetIterator() {}
        bool operator != (int val)
        {
            return _curIter != _endIter;
        }
        /// Operator overloading
        //@{
        bool operator==(const CondPtsSetIterator &RHS) const
        {
            // If they are both at the end, ignore the rest of the fields.
            if (atEnd && RHS.atEnd)
                return true;
            // Otherwise they are the same if they have the same condVar
            return atEnd == RHS.atEnd && RHS._curIter == _curIter && RHS._ptIter == _ptIter;
        }
        bool operator!=(const CondPtsSetIterator &RHS) const
        {
            return !(*this == RHS);
        }

        void operator ++(void)
        {
            if(atEnd == true)
                return;

            if(_ptIter==_ptEndIter)
            {
                if(_curIter == _endIter)
                {
                    atEnd = true;
                    return;
                }
                _curIter++;
                _ptIter = _curIter->second.begin();
                _ptIter = _curIter->second.end();
            }
            else
                _ptIter++;
        }
        SingleCondVar operator *(void)
        {
            SingleCondVar temp_var(cond(), *_ptIter);
            return temp_var;
        }
        //@}

        Cond cond(void)
        {
            return _curIter->first;
        }

    private:
        typename CondPointsToSet<Cond>::CondPtsIter _curIter;
        typename CondPointsToSet<Cond>::CondPtsIter _endIter;
        PointsTo::iterator _ptIter;
        PointsTo::iterator _ptEndIter;
        bool atEnd;
    };

public:
    typedef CondPtsSetIterator iterator;
    /// iterators
    //@{
    inline iterator begin()
    {
        return iterator(this);
    }

    inline iterator end()
    {
        return iterator(this,true);
    }

    inline iterator begin() const
    {
        return iterator(this);
    }

    inline iterator end() const
    {
        return iterator(this,true);
    }
    //@}

private:
    CondPts _condPts;
};

} // End namespace SVF

/// Specialise hash for CondVar
template <typename Cond>
struct std::hash<const SVF::CondVar<Cond>>
{
    size_t operator()(const SVF::CondVar<Cond> &cv) const
    {
        std::hash<Cond> h;
        return h(cv.get_cond());
    }
};

template <typename Cond>
struct std::hash<SVF::CondVar<Cond>>
{
    size_t operator()(const SVF::CondVar<Cond> &cv) const
    {
        std::hash<Cond> h;
        return h(cv.get_cond());
    }
};

template <typename Element>
struct std::hash<SVF::CondStdSet<Element>>
{
    size_t operator()(const SVF::CondStdSet<Element> &css) const
    {
        // TODO: this is not a very good hash, but we probably won't be
        //       using it for now. Needed for other templates to compile...
        SVF::Hash<std::pair<Element, unsigned>> h;
        return h(std::make_pair(*css.begin(), css.size()));
    }
};

#endif /* CONDVAR_H_ */
