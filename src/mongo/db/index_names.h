/**
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
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#pragma once

#include <string>

#include "mongo/base/string_data.h"

namespace mongo {

class BSONObj;

/**
 * We need to know what 'type' an index is in order to plan correctly.
 */
enum IndexType {
    INDEX_ALLPATHS,
    INDEX_BTREE,
    INDEX_2D,
    INDEX_HAYSTACK,
    INDEX_2DSPHERE,
    INDEX_TEXT,
    INDEX_HASHED,
};

/**
 * We use the std::string representation of index names all over the place, so we declare them all
 * once here.
 */
class IndexNames {
public:
    static const std::string ALLPATHS;
    static const std::string BTREE;
    static const std::string GEO_2D;
    static const std::string GEO_2DSPHERE;
    static const std::string GEO_HAYSTACK;
    static const std::string HASHED;
    static const std::string TEXT;

    /**
     * Return the first std::string value in the provided object.  For an index key pattern,
     * a field with a non-string value indicates a "special" (not straight Btree) index.
     */
    static std::string findPluginName(const BSONObj& keyPattern);

    /**
     * Is the provided access method name one we recognize?
     */
    static bool isKnownName(const std::string& name);

    /**
     * Convert an index name to an IndexType.
     */
    static IndexType nameToType(StringData accessMethod);
};

}  // namespace mongo
