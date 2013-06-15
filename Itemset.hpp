#ifndef ITEMSET_HPP
#define ITEMSET_HPP

#include "Item.hpp"

#include <vector>
#include <algorithm>

#include "Typedefs.hpp"

/*!
 * \brief Itemset
 */
typedef std::vector < Item > Itemset;

///*
inline std::ostream & operator << ( std::ostream & os, const Itemset & itemset )
{
    unsigned index = itemset.size();
    if ( index ) {
        os << '(';
        std::for_each( itemset.cbegin(), itemset.cend(), [&]( const Item & tid ) {
            os << tid << ( --index ? ' ' : ')' );
        } );
    }
    return os;
}
//*/


#endif // ITEMSET_HPP
