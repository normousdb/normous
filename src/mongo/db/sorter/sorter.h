/*
*    Copyright (C) 2013 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <boost/smart_ptr.hpp>
#include <deque>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <mongo/base/disallow_copying.h>

/**
 * This is the public API for the Sorter (both in-memory and external)
 *
 * Many of the classes in this file are templated on Key and Value types which
 * require the following public members:
 *
 * // A type carrying extra information used by the deserializer. Contents are
 * // up to you, but  it should be cheap to copy. Use an empty struct if your
 * // deserializer doesn't need extra data.
 * struct SorterDeserializeSettings {};
 *
 * // Serialize this object to the BufBuilder
 * void serializeForSorter(BufBuilder& buf) const;
 *
 * // Deserialize and return an object from the BufReader
 * static Type deserializeForSorter(BufReader& buf, const Type::SorterDeserializeSettings&);
 *
 * // How much memory is used by your type? Include sizeof(*this) and any memory you reference.
 * int memUsageForSorter() const;
 *
 * // For types with owned and unowned states, such as BSON, return an owned version.
 * // Return *this if your type doesn't have an unowned state
 * Type getOwned() const;
 *
 * Comparators are functors that that compare pair<Key, Value> and return an
 * int less than, equal to, or greater than 0 depending on how the two pairs
 * compare with the same semantics as memcmp.
 * Example for Key=BSONObj, Value=int:
 *
 * class MyComparator {
 * public:
 *     int operator()(const std::pair<BSONObj, int>& lhs,
 *                    const std::pair<BSONObj, int>& rhs) {
 *         int ret = lhs.first.woCompare(rhs.first, _ord);
 *         if (ret)
 *             return ret;
 *
 *        if (lhs.second >  rhs.second) return 1;
 *        if (lhs.second == rhs.second) return 0;
 *        return -1;
 *     }
 *     Ordering _ord;
 * };
 */

namespace mongo {
    namespace sorter {
        // Everything in this namespace is internal to the sorter
        class FileDeleter;
    }

    /**
     * Runtime options that control the Sorter's behavior
     */
    struct SortOptions {
        long long limit; /// number of KV pairs to be returned. 0 for no limit.
        size_t maxMemoryUsageBytes; /// Approximate.
        bool extSortAllowed; /// If false, uassert if more mem needed than allowed.
        SortOptions()
            : limit(0)
            , maxMemoryUsageBytes(64*1024*1024)
            , extSortAllowed(false)
        {}
    };

    /// This is the output from the sorting framework
    template <typename Key, typename Value>
    class SortIteratorInterface {
        MONGO_DISALLOW_COPYING(SortIteratorInterface);
    public:
        typedef std::pair<Key, Value> Data;

        // Unowned objects are only valid until next call to any method

        virtual bool more() =0;
        virtual std::pair<Key, Value> next() =0;

        virtual ~SortIteratorInterface() {}

        /// Returns an iterator that merges the passed in iterators
        template <typename Comparator>
        static SortIteratorInterface* merge(
                const std::vector<boost::shared_ptr<SortIteratorInterface> >& iters,
                const SortOptions& opts,
                const Comparator& comp);
    protected:
        SortIteratorInterface() {} // can only be constructed as a base
    };

    /// This is the main way to input data to the sorting framework
    template <typename Key, typename Value>
    class Sorter {
        MONGO_DISALLOW_COPYING(Sorter);
    public:
        typedef std::pair<Key, Value> Data;
        typedef SortIteratorInterface<Key, Value> Iterator;
        typedef std::pair<typename Key::SorterDeserializeSettings
                         ,typename Value::SorterDeserializeSettings
                         > Settings;

        template <typename Comparator>
        static Sorter* make(const SortOptions& opts,
                            const Comparator& comp,
                            const Settings& settings = Settings());

        virtual void add(const Key&, const Value&) =0;
        virtual Iterator* done() =0; /// Can't add more data after calling done()

        virtual ~Sorter() {}

        // TEMP these are here for compatibility. Will be replaced with a general stats API
        virtual int numFiles() const =0;
        virtual size_t memUsed() const =0;

    protected:
        Sorter() {} // can only be constructed as a base
    };

    /// Writes pre-sorted data to a sorted file and hands-back an Iterator over that file.
    template <typename Key, typename Value>
    class SortedFileWriter {
        MONGO_DISALLOW_COPYING(SortedFileWriter);
    public:
        typedef SortIteratorInterface<Key, Value> Iterator;
        typedef std::pair<typename Key::SorterDeserializeSettings
                         ,typename Value::SorterDeserializeSettings
                         > Settings;

        explicit SortedFileWriter(const Settings& settings = Settings());

        void addAlreadySorted(const Key&, const Value&);
        Iterator* done(); /// Can't add more data after calling done()

    private:
        const Settings _settings;
        std::string _fileName;
        boost::shared_ptr<sorter::FileDeleter> _fileDeleter; // Must outlive _file
        std::ofstream _file;
    };
}

/**
 * #include "mongo/db/sorter/sorter.cpp" and call this in a single translation
 * unit once for each unique set of template parameters.
 */
#define MONGO_CREATE_SORTER(Key, Value, Comparator) \
    template class ::mongo::Sorter<Key, Value>; \
    template class ::mongo::NoLimitSorter<Key, Value, Comparator>; \
    template class ::mongo::SortIteratorInterface<Key, Value>; \
    template class ::mongo::SortedFileWriter<Key, Value>; \
    template class ::mongo::sorter::MergeIterator<Key, Value, Comparator>; \
    template class ::mongo::sorter::InMemIterator<Key, Value>; \
    template class ::mongo::sorter::FileIterator<Key, Value>; \
    template ::mongo::SortIteratorInterface<Key, Value>* \
                ::mongo::SortIteratorInterface<Key, Value>::merge<Comparator>( \
                    const std::vector<boost::shared_ptr<SortIteratorInterface> >& iters, \
                    const SortOptions& opts, \
                    const Comparator& comp); \
    template ::mongo::Sorter<Key, Value>* \
                ::mongo::Sorter<Key, Value>::make<Comparator>( \
                    const SortOptions& opts, \
                    const Comparator& comp, \
                    const Settings& settings);
