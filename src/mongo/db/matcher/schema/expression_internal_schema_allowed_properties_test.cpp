/**
 * Copyright (C) 2017 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/bson/json.h"
#include "mongo/db/matcher/expression_parser.h"
#include "mongo/db/matcher/schema/expression_internal_schema_allowed_properties.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
constexpr auto kSimpleCollator = nullptr;

TEST(InternalSchemaAllowedPropertiesMatchExpression, MatchesObjectsWithListedProperties) {
    auto filter = fromjson(
        "{$_internalSchemaAllowedProperties: {properties: ['a', 'b'],"
        "namePlaceholder: 'i', patternProperties: [], otherwise: {i: 0}}}");
    auto expr = MatchExpressionParser::parse(filter, kSimpleCollator);
    ASSERT_OK(expr.getStatus());

    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{a: 1, b: 1}")));
    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{a: 1}")));
    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{b: 1}")));
}

TEST(InternalSchemaAllowedPropertiesMatchExpression, MatchesObjectsWithMatchingPatternProperties) {
    auto filter = fromjson(R"(
        {$_internalSchemaAllowedProperties: {
            properties: [],
            namePlaceholder: 'i',
            patternProperties: [
                {regex: /s$/, expression: {i: {$gt: 0}}},
                {regex: /[nN]um/, expression: {i: {$type: 'number'}}}
            ],
            otherwise: {i: {$type: 'string'}}
        }})");
    auto expr = MatchExpressionParser::parse(filter, kSimpleCollator);
    ASSERT_OK(expr.getStatus());

    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{puppies: 2, kittens: 3, phoneNum: 1234}")));
    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{puppies: 2}")));
    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{phoneNum: 1234}")));
}

TEST(InternalSchemaAllowedPropertiesMatchExpression,
     PatternPropertiesStillEnforcedEvenIfFieldListedInProperties) {
    auto filter = fromjson(
        "{$_internalSchemaAllowedProperties: {properties: ['a'], namePlaceholder: 'a',"
        "patternProperties: [{regex: /a/, expression: {a: {$gt: 5}}}], otherwise: {a: 0}}}");
    auto expr = MatchExpressionParser::parse(filter, kSimpleCollator);
    ASSERT_OK(expr.getStatus());

    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{a: 6}")));
    ASSERT_FALSE(expr.getValue()->matchesBSON(fromjson("{a: 5}")));
    ASSERT_FALSE(expr.getValue()->matchesBSON(fromjson("{a: 4}")));
}

TEST(InternalSchemaAllowedPropertiesMatchExpression, OtherwiseEnforcedWhenAppropriate) {
    auto filter = fromjson(R"(
        {$_internalSchemaAllowedProperties: {
            properties: [],
            namePlaceholder: 'i',
            patternProperties: [
                {regex: /s$/, expression: {i: {$gt: 0}}},
                {regex: /[nN]um/, expression: {i: {$type: 'number'}}}
            ],
            otherwise: {i: {$type: 'string'}}
        }})");
    auto expr = MatchExpressionParser::parse(filter, kSimpleCollator);
    ASSERT_OK(expr.getStatus());

    ASSERT_TRUE(expr.getValue()->matchesBSON(fromjson("{foo: 'bar'}")));
    ASSERT_FALSE(expr.getValue()->matchesBSON(fromjson("{foo: 7}")));
}

TEST(InternalSchemaAllowedPropertiesMatchExpression, EquivalentToClone) {
    auto filter = fromjson(
        "{$_internalSchemaAllowedProperties: {properties: ['a'], namePlaceholder: 'i',"
        "patternProperties: [{regex: /a/, expression: {i: 1}}], otherwise: {i: 7}}}");
    auto expr = MatchExpressionParser::parse(filter, kSimpleCollator);
    ASSERT_OK(expr.getStatus());
    auto clone = expr.getValue()->shallowClone();
    ASSERT_TRUE(expr.getValue()->equivalent(clone.get()));
}
}  // namespace mongo
