#ifndef CHARM_HPP
#define CHARM_HPP

#include "CSet.hpp"

#include "Database.hpp"
#include "Node.hpp"
#include <cassert>
#include <chrono>

/*!
 * \brief ItemMap
 */
typedef std::unordered_map< Item, Tidset, item_hash > ItemMap;

namespace Talky_G
{

/*!
 * \brief is_subsumed
 * \param c_set
 * \param node
 * \return
 */
inline bool is_subsumed(const CSet &c_set, const Node & node)
{
    const Itemset & X = node.itemset();
    const Diffset & Y = node.diffset();
    bool is_subsumed = false;
    const int hashkey = diffset_hash::hash( std::make_pair( Y, node.parent()->hashkey() ) );
    const auto range = c_set.equal_range( std::make_pair( Y, hashkey ) );
    for ( auto it = range.first; it != range.second; ++ it ) {
        const Itemset & C = (*it).second.first;
        const auto sup = (*it).second.second;
        if ( node.sup() == sup ) {
            const bool includes = std::includes( X.cbegin(), X.cend(), C.cbegin(), C.cend() );
            if ( includes ) {
                is_subsumed = true;
                break;
            }
        }
    }
    return is_subsumed;
}

/*!
 * \brief itemset_union
 * \param itemset_l
 * \param itemset_r
 * \return
 */
inline Itemset itemset_union(const Itemset &itemset_l, const Itemset & itemset_r)
{
    Itemset union_itemset( itemset_l.size() + itemset_r.size() );
    auto it_union = std::set_union( itemset_l.cbegin(), itemset_l.cend(), itemset_r.cbegin(), itemset_r.cend(), union_itemset.begin() );
    union_itemset.resize( std::distance(union_itemset.begin(), it_union) );
    return std::move(union_itemset);
}

/*!
 * \brief tidset_intersection
 * \param tidset_l
 * \param tidset_r
 * \return
 */
inline Tidset tidset_intersection(const Tidset &tidset_l, const Tidset &tidset_r)
{
    Tidset resutl_tidset( tidset_l.size() + tidset_r.size() );
    const auto it = std::set_intersection( tidset_r.cbegin(), tidset_r.cend(), tidset_l.cbegin(), tidset_l.cend(), resutl_tidset.begin() );
    resutl_tidset.resize( std::distance( resutl_tidset.begin(), it ) );
    return std::move(resutl_tidset);
}

/*!
 * \brief diffset_difference
 * \param diffset_l
 * \param diffset_r
 * \return
 */
inline Diffset diffset_difference(const Diffset &diffset_l, const Diffset & diffset_r)
{
    Diffset result_diffset( diffset_l.size() + diffset_r.size() );
    auto it = std::set_difference( diffset_r.cbegin(), diffset_r.cend(), diffset_l.cbegin(), diffset_l.cend(), result_diffset.begin() );
    result_diffset.resize( std::distance(result_diffset.begin(), it) );
    return std::move(result_diffset);
}

/*!
 * \brief save
 * \param c_set
 * \param child
 */
inline void save(CSet & c_set, const Node & child)
{
    c_set.insert( CSet::value_type( cset_key_t( child.diffset(), child.parent()->hashkey() ), cset_val_t( child.itemset(), child.sup() ) ) );
}

/*!
 * \brief is_null
 * \param node
 * \return
 */
inline bool is_null(const Node & node)
{
    return ( node.diffset().empty() || node.itemset().empty() );
}

/*!
 * \brief get_next_generator
 * \param curr
 * \param other
 * \param c_set
 * \param min_sup
 * \return
 */
inline Node get_next_generator(const Node & curr, const Node & other, const CSet & c_set, const unsigned int min_sup)
{
    Diffset cand_diffset = diffset_difference( curr.diffset(), other.diffset() );
    const unsigned int cand_sup = curr.sup() - cand_diffset.size();
    // Check support
    if ( cand_sup < min_sup ) {
        return Node(); // Return null
    }

    // Check equality
    Itemset cand_itemset = itemset_union( curr.itemset(), other.itemset() );
    const bool equal_to_curr = curr.sup() == cand_sup;
    const bool equal_to_other = other.sup() == cand_sup;

    if ( equal_to_curr || equal_to_other ) {
        return Node(); // Return null
    }
    const Node cand_node( std::move(cand_itemset), std::move(cand_diffset), &curr );
    if ( is_subsumed( c_set, cand_node ) ) {
        return Node();
    }
    return cand_node;
}


template< typename node_iterator >
/*!
 * \brief talky_g_extend
 * \param curr
 * \param right_margin
 * \param c_set
 * \param min_sup
 */
inline void talky_g_extend(node_iterator & curr, const node_iterator & right_margin, CSet &c_set, const unsigned int min_sup)
{
    Node & current_child = (*(*curr));
    if ( std::distance( right_margin, curr ) >= 1 ) {
        for ( auto it = curr - 1; std::distance( right_margin, it ) >= 0; --it ) {
            const Node & other = (*(*it));
            const auto generator = get_next_generator( current_child, other, c_set, min_sup );
            if ( ! is_null( generator ) ) {
                current_child.add_child( generator );
            }
        }
        // Loop over the children of curr from Left to Right
        for ( auto it = current_child.children().crbegin(); it != current_child.children().crend(); ++ it ) {
            const Node & child = (*(*it));
            save( c_set, child );
            talky_g_extend( it, current_child.children().crbegin(), c_set, min_sup );
        }
    }
}

/*!
 * \brief talky_g
 * \param database
 * \param min_sup
 * \return
 */
inline CSet talky_g( const Database & database, const unsigned int min_sup )
{
    ItemMap item_map;
    // For each itemset
    TID transaction_counter = 1;
    std::for_each( database.cbegin(), database.cend(), [&] ( const Itemset & itemset ) {
        // For each item in the itemset
        std::for_each( itemset.cbegin(), itemset.cend(), [&]( const Item & item ) {
            // If there is such item increase it's support
            const auto got = item_map.find( item );
            if ( item_map.cend() != got ) {
                got->second.push_back( transaction_counter );
            }
            else {
                Tidset new_tidset( 1 );
                new_tidset.at( 0 ) = transaction_counter;
                item_map.insert( std::make_pair( item, new_tidset ) );
            }
        } );
        ++ transaction_counter;
    } );
    --transaction_counter;
//    std::cerr << "transaction_counter = " << transaction_counter << std::endl;

    // Translate tidset into diffset
    std::for_each( item_map.begin(), item_map.end(), [&]( ItemMap::reference key_value ) {
        const auto tidset = std::move( key_value.second );
        // Transfort to a diffset
        for ( auto index = 1; index <= transaction_counter; ++ index ) {
            if ( tidset.cend() == std::find( tidset.cbegin(), tidset.cend(), index ) ) {
                key_value.second.push_back( index );
            }
        }
    } );
    // Fill the tree
    const unsigned int sum_of_trans_id = transaction_counter * (transaction_counter - 1) / 2;
    Node root_node( Itemset(), Diffset(), transaction_counter, sum_of_trans_id );
    {
        std::for_each( item_map.cbegin(), item_map.cend(), [&]( ItemMap::const_reference key_value ) {
            if ( min_sup <= (transaction_counter - key_value.second.size()) ) {
                Itemset itemset;
                itemset.push_back( key_value.first );
                Diffset diffset = key_value.second;
                root_node.add_child( std::move( itemset ), std::move( diffset ) );
            }
        } );
    }
    auto c_set = CSet();
    // Loop over children of root Right to Left
    for ( auto it = root_node.children().crbegin(); it != root_node.children().crend(); ++ it ) {
        Node & current_child = (*(*it));
        save( c_set, current_child );
        talky_g_extend( it, root_node.children().crbegin(), c_set, min_sup );
    }
    return c_set;
}
}

#endif // CHARM_HPP
