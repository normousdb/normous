// index_set.h

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
*/

#pragma once

#include <set>

#include "mongo/base/string_data.h"

namespace mongo {

    /**
     * a.$ -> a
     * @return true if out is set and we made a change
     */
    bool getCanonicalIndexField( const StringData& fullName, std::string* out );

    class IndexPathSet {
    public:
        void addPath( const StringData& path );

        void clear();

        bool mightBeIndexed( const StringData& path ) const;

    private:
        std::set<std::string> _canonical;
    };

}
