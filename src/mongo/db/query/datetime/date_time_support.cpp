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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include <memory>
#include <timelib.h>

#include "mongo/db/query/datetime/date_time_support.h"

#include "mongo/base/init.h"
#include "mongo/bson/util/builder.h"
#include "mongo/db/service_context.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/duration.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

namespace {
const auto getTimeZoneDatabase =
    ServiceContext::declareDecoration<std::unique_ptr<TimeZoneDatabase>>();

// Converts a date to a number of seconds, being careful to round appropriately for negative numbers
// of seconds.
long long seconds(Date_t date) {
    auto millis = date.toMillisSinceEpoch();
    if (millis < 0) {
        // We want the division below to truncate toward -inf rather than 0
        // eg Dec 31, 1969 23:59:58.001 should be -2 seconds rather than -1
        // This is needed to get the correct values from coerceToTM
        if (-1999 / 1000 != -2) {  // this is implementation defined
            millis -= 1000 - 1;
        }
    }
    return durationCount<Seconds>(Milliseconds(millis));
}

}  // namespace

const TimeZoneDatabase* TimeZoneDatabase::get(ServiceContext* serviceContext) {
    invariant(getTimeZoneDatabase(serviceContext));
    return getTimeZoneDatabase(serviceContext).get();
}

void TimeZoneDatabase::set(ServiceContext* serviceContext,
                           std::unique_ptr<TimeZoneDatabase> dateTimeSupport) {
    getTimeZoneDatabase(serviceContext) = std::move(dateTimeSupport);
}

TimeZoneDatabase::TimeZoneDatabase() {
    loadTimeZoneInfo({const_cast<timelib_tzdb*>(timelib_builtin_db()), TimeZoneDBDeleter()});
}

TimeZoneDatabase::TimeZoneDatabase(
    std::unique_ptr<timelib_tzdb, TimeZoneDBDeleter> timeZoneDatabase) {
    loadTimeZoneInfo(std::move(timeZoneDatabase));
}

void TimeZoneDatabase::TimeZoneDBDeleter::operator()(timelib_tzdb* timeZoneDatabase) {
    if (timeZoneDatabase != timelib_builtin_db()) {
        timelib_zoneinfo_dtor(timeZoneDatabase);
    }
}

void TimeZoneDatabase::loadTimeZoneInfo(
    std::unique_ptr<timelib_tzdb, TimeZoneDBDeleter> timeZoneDatabase) {
    invariant(timeZoneDatabase);
    _timeZoneDatabase = std::move(timeZoneDatabase);
    int nTimeZones;
    auto timezone_identifier_list =
        timelib_timezone_identifiers_list(_timeZoneDatabase.get(), &nTimeZones);
    for (int i = 0; i < nTimeZones; ++i) {
        auto entry = timezone_identifier_list[i];
        int errorCode = TIMELIB_ERROR_NO_ERROR;
        auto tzInfo = timelib_parse_tzfile(entry.id, _timeZoneDatabase.get(), &errorCode);
        if (!tzInfo) {
            invariant(errorCode != TIMELIB_ERROR_NO_ERROR);
            fassertFailedWithStatusNoTrace(
                40475,
                {ErrorCodes::FailedToParse,
                 str::stream() << "failed to parse time zone file for time zone identifier \""
                               << entry.id
                               << "\": "
                               << timelib_get_error_message(errorCode)});
        }
        invariant(errorCode == TIMELIB_ERROR_NO_ERROR);
        _timeZones[entry.id] = TimeZone{tzInfo};
    }
}

TimeZone TimeZoneDatabase::utcZone() {
    return TimeZone{nullptr};
}

static timelib_tzinfo* timezonedatabase_gettzinfowrapper(char* tz_id,
                                                         const _timelib_tzdb* db,
                                                         int* error) {
    return nullptr;
}

Date_t TimeZoneDatabase::fromString(StringData dateString) const {
    std::unique_ptr<timelib_time, TimeZone::TimelibTimeDeleter> t(
        timelib_strtotime(const_cast<char*>(dateString.toString().c_str()),
                          dateString.size(),
                          nullptr,
                          _timeZoneDatabase.get(),
                          timezonedatabase_gettzinfowrapper));

    // If the time portion is fully missing, initialize to 0. This allows for the '%Y-%m-%d' format
    // to be passed too, which is what the BI connector may request
    if (t->h == TIMELIB_UNSET && t->i == TIMELIB_UNSET && t->s == TIMELIB_UNSET) {
        t->h = t->i = t->s = t->f = 0;
    }

    if (t->y == TIMELIB_UNSET || t->m == TIMELIB_UNSET || t->d == TIMELIB_UNSET ||
        t->h == TIMELIB_UNSET || t->i == TIMELIB_UNSET || t->s == TIMELIB_UNSET) {
        uasserted(40545,
                  str::stream()
                      << "an incomplete date/time string has been found, with elements missing: \""
                      << dateString
                      << "\"");
    }

    timelib_update_ts(t.get(), nullptr);
    timelib_unixtime2local(t.get(), t->sse);

    return Date_t::fromMillisSinceEpoch((static_cast<double>(t->sse) + static_cast<double>(t->f)) *
                                        1000);
}

TimeZone TimeZoneDatabase::getTimeZone(StringData timeZoneId) const {
    auto tz = _timeZones.find(timeZoneId);
    if (tz != _timeZones.end()) {
        return tz->second;
    }

    uasserted(40485,
              str::stream() << "unrecognized time zone identifier: \"" << timeZoneId << "\"");
}

Date_t TimeZone::createFromDateParts(
    int year, int month, int day, int hour, int minute, int second, int millisecond) const {
    auto t = timelib_time_ctor();

    t->y = year;
    t->m = month;
    t->d = day;
    t->h = hour;
    t->i = minute;
    t->s = second;
    t->f = millisecond / 1000.0;

    if (_tzInfo) {
        timelib_update_ts(t, _tzInfo.get());
        timelib_set_timezone(t, _tzInfo.get());
    } else {
        timelib_update_ts(t, nullptr);
    }
    timelib_unixtime2gmt(t, t->sse);

    auto returnValue = Date_t::fromMillisSinceEpoch((t->f + t->sse) * 1000);

    timelib_time_dtor(t);

    return returnValue;
}

Date_t TimeZone::createFromIso8601DateParts(int isoYear,
                                            int isoWeekYear,
                                            int isoDayOfWeek,
                                            int hour,
                                            int minute,
                                            int second,
                                            int millisecond) const {
    auto t = timelib_time_ctor();

    timelib_date_from_isodate(isoYear, isoWeekYear, isoDayOfWeek, &t->y, &t->m, &t->d);
    t->h = hour;
    t->i = minute;
    t->s = second;
    t->f = millisecond / 1000.0;

    if (_tzInfo) {
        timelib_update_ts(t, _tzInfo.get());
        timelib_set_timezone(t, _tzInfo.get());
    } else {
        timelib_update_ts(t, nullptr);
    }
    timelib_unixtime2gmt(t, t->sse);

    auto returnValue = Date_t::fromMillisSinceEpoch((t->f + t->sse) * 1000);

    timelib_time_dtor(t);

    return returnValue;
}

TimeZone::DateParts::DateParts(const timelib_time& timelib_time, Date_t date)
    : year(timelib_time.y),
      month(timelib_time.m),
      dayOfMonth(timelib_time.d),
      hour(timelib_time.h),
      minute(timelib_time.i),
      second(timelib_time.s) {
    const int ms = date.toMillisSinceEpoch() % 1000LL;
    // Add 1000 since dates before 1970 would have negative milliseconds.
    millisecond = ms >= 0 ? ms : 1000 + ms;
}

TimeZone::Iso8601DateParts::Iso8601DateParts(const timelib_time& timelib_time, Date_t date)
    : hour(timelib_time.h), minute(timelib_time.i), second(timelib_time.s) {

    timelib_sll tmpIsoYear, tmpIsoWeekOfYear, tmpIsoDayOfWeek;

    timelib_isodate_from_date(timelib_time.y,
                              timelib_time.m,
                              timelib_time.d,
                              &tmpIsoYear,
                              &tmpIsoWeekOfYear,
                              &tmpIsoDayOfWeek);

    year = static_cast<int>(tmpIsoYear);
    weekOfYear = static_cast<int>(tmpIsoWeekOfYear);
    dayOfWeek = static_cast<int>(tmpIsoDayOfWeek);

    const int ms = date.toMillisSinceEpoch() % 1000LL;
    // Add 1000 since dates before 1970 would have negative milliseconds.
    millisecond = ms >= 0 ? ms : 1000 + ms;
}


void TimeZone::TimelibTZInfoDeleter::operator()(timelib_tzinfo* tzInfo) {
    if (tzInfo) {
        timelib_tzinfo_dtor(tzInfo);
    }
}

TimeZone::TimeZone(timelib_tzinfo* tzInfo) : _tzInfo(tzInfo, TimelibTZInfoDeleter()) {}

void TimeZone::TimelibTimeDeleter::operator()(timelib_time* time) {
    timelib_time_dtor(time);
}

std::unique_ptr<timelib_time, TimeZone::TimelibTimeDeleter> TimeZone::getTimelibTime(
    Date_t date) const {
    std::unique_ptr<timelib_time, TimeZone::TimelibTimeDeleter> time(timelib_time_ctor());
    if (_tzInfo) {
        timelib_set_timezone(time.get(), _tzInfo.get());
        timelib_unixtime2local(time.get(), seconds(date));
    } else {
        timelib_unixtime2gmt(time.get(), seconds(date));
    }
    return time;
}

TimeZone::Iso8601DateParts TimeZone::dateIso8601Parts(Date_t date) const {
    auto time = getTimelibTime(date);
    return Iso8601DateParts(*time, date);
}

TimeZone::DateParts TimeZone::dateParts(Date_t date) const {
    auto time = getTimelibTime(date);
    return DateParts(*time, date);
}

int TimeZone::dayOfWeek(Date_t date) const {
    auto time = getTimelibTime(date);
    // timelib_day_of_week() returns a number in the range [0,6], we want [1,7], so add one.
    return timelib_day_of_week(time->y, time->m, time->d) + 1;
}

int TimeZone::week(Date_t date) const {
    int weekDay = dayOfWeek(date);
    int yearDay = dayOfYear(date);
    int prevSundayDayOfYear = yearDay - weekDay;        // may be negative
    int nextSundayDayOfYear = prevSundayDayOfYear + 7;  // must be positive

    // Return the zero based index of the week of the next sunday, equal to the one based index
    // of the week of the previous sunday, which is to be returned.
    int nextSundayWeek = nextSundayDayOfYear / 7;

    return nextSundayWeek;
}

int TimeZone::dayOfYear(Date_t date) const {
    auto time = getTimelibTime(date);
    // timelib_day_of_year() returns a number in the range [0,365], we want [1,366], so add one.
    return timelib_day_of_year(time->y, time->m, time->d) + 1;
}

int TimeZone::isoDayOfWeek(Date_t date) const {
    auto time = getTimelibTime(date);
    return timelib_iso_day_of_week(time->y, time->m, time->d);
}

int TimeZone::isoWeek(Date_t date) const {
    auto time = getTimelibTime(date);
    long long isoWeek;
    long long isoYear;
    timelib_isoweek_from_date(time->y, time->m, time->d, &isoWeek, &isoYear);
    return isoWeek;
}

long long TimeZone::isoYear(Date_t date) const {
    auto time = getTimelibTime(date);
    long long isoWeek;
    long long isoYear;
    timelib_isoweek_from_date(time->y, time->m, time->d, &isoWeek, &isoYear);
    return isoYear;
}

Seconds TimeZone::utcOffset(Date_t date) const {
    auto time = getTimelibTime(date);
    return Seconds(time->z);
}

void TimeZone::validateFormat(StringData format) {
    for (auto it = format.begin(); it != format.end(); ++it) {
        if (*it != '%') {
            continue;
        }

        ++it;  // next character must be format modifier
        uassert(18535, "Unmatched '%' at end of $dateToString format string", it != format.end());


        switch (*it) {
            // all of these fall through intentionally
            case '%':
            case 'Y':
            case 'm':
            case 'd':
            case 'H':
            case 'M':
            case 'S':
            case 'L':
            case 'j':
            case 'w':
            case 'U':
            case 'G':
            case 'V':
            case 'u':
            case 'z':
            case 'Z':
                break;
            default:
                uasserted(18536,
                          str::stream() << "Invalid format character '%" << *it
                                        << "' in $dateToString format string");
        }
    }
}

std::string TimeZone::formatDate(StringData format, Date_t date) const {
    StringBuilder formatted;
    outputDateWithFormat(formatted, format, date);
    return formatted.str();
}
}  // namespace mongo
