/*    Copyright 2012 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include "mongo/db/auth/principal.h"
#include "mongo/db/auth/privilege.h"

namespace mongo {

    /**
     * A representation that a given principal has the permission to perform a set of actions on a
     * specific resource.
     */
    class AcquiredPrivilege {
    public:

        AcquiredPrivilege(const Privilege& privilege, Principal* principal) :
            _privilege(privilege), _principal(principal) {}
        ~AcquiredPrivilege() {}

        const Principal* getPrincipal() const { return _principal; }

        const Privilege& getPrivilege() const { return _privilege; }

    private:

        Privilege _privilege;
        Principal* _principal;
    };

} // namespace mongo
