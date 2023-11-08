/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef ANALYZER_MISRA_PROTO_UTIL_H_
#define ANALYZER_MISRA_PROTO_UTIL_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "analyzer/proto/analyzer.pb.h"
#include "analyzer/proto/results.pb.h"

namespace misra {
namespace proto_util {

analyzer::proto::Result* AddMultipleLocationsResultToResultsList(
    analyzer::proto::ResultsList* results_list, const std::string& path,
    int line_number, const std::string& error_message,
    std::vector<std::string> locations, bool false_positive = false);
analyzer::proto::Result* AddResultToResultsList(
    analyzer::proto::ResultsList* results_list, const std::string& path,
    int line_number, const std::string& error_message,
    bool false_positive = false);
absl::Status GenerateProtoFile(const analyzer::proto::ResultsList& results_list,
                               const std::string& path);
absl::Status ParseFromProtoFile(const std::string& path,
                                analyzer::proto::ResultsList* results_list);
absl::Status ParseFromProtoString(const std::string& proto,
                                  analyzer::proto::ResultsList* results_list);

}  // namespace proto_util
}  // namespace misra

#endif  // ANALYZER_MISRA_PROTO_UTIL_H_
