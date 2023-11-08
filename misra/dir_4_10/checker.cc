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

#include "misra/dir_4_10/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace dir_4_10 {

void PrecautionCheck::Init(analyzer::proto::ResultsList* results_list,
                           SourceManager* source_manager) {
  results_list_ = results_list;
  source_manager_ = source_manager;
}

// State Flow Design
// State::New
//  --> State::kIgnored, if current file is an ignored header file (eg. system
//  headers)
//  --> State::Reported, if meet '#ifndef NAME', and NAME is used, terminate.
//  --> State::FoundIfndef, if meet '#ifndef NAME', and NAME is not used
//    --> State::FoundDefinedAfterIfndef, if meet '#define NAME', terminate.
//    --> State::Fatal, otherwise, terminate.
//  --> State::Reported, if meet 'defined NAME', and NAME is used, terminate.
//  --> State::FoundDefined, if meet 'defined NAME', and NAME is not used
//    --> State::FoundIfndef if meet '#if !defined NAME' in the same location
//    --> State::Fatal, otherwise, terminate.

enum class PrecautionCheck::FileState {
  kNew = 0,
  kFoundIfndef,
  kFoundDefined,
  kFoundDefineAfterIfndef,
  kFatal,
  kReported,
  kIgnored
};

bool PrecautionCheck::IsTerminated(FileState state) {
  return (file_states_[current_file_] == FileState::kFoundDefineAfterIfndef ||
          file_states_[current_file_] == FileState::kFatal ||
          file_states_[current_file_] == FileState::kReported ||
          file_states_[current_file_] == FileState::kIgnored);
}

void PrecautionCheck::FileChanged(SourceLocation loc, FileChangeReason reason,
                                  SrcMgr::CharacteristicKind file_type,
                                  FileID prev_fid) {
  if (reason == EnterFile) {
    current_file_ = libtooling_utils::CleanPath(
        tooling::getAbsolutePath(source_manager_->getFilename(loc).str()));
    if (file_type == SrcMgr::CharacteristicKind::C_System ||
        file_type == SrcMgr::CharacteristicKind::C_ExternCSystem) {
      file_states_[current_file_] = FileState::kIgnored;
      return;
    }
    if (file_states_.find(current_file_) == file_states_.end()) {
      file_states_[current_file_] = FileState::kNew;
    }
  } else if (reason == ExitFile) {
    SourceLocation prev_loc = source_manager_->getLocForStartOfFile(prev_fid);
    string exited_file = libtooling_utils::CleanPath(
        tooling::getAbsolutePath(source_manager_->getFilename(prev_loc).str()));
    if (exited_file != current_file_) {
      LOG(FATAL) << "PrecautionCheck Execution Error: Unkonwn precossing path";
    } else {
      if (IsNotIgnoredHeaderFile(current_file_)) CheckHeaderFile(current_file_);
    }
    current_file_ = libtooling_utils::CleanPath(
        tooling::getAbsolutePath(source_manager_->getFilename(loc).str()));
  }
}

void PrecautionCheck::Defined(const Token& macro_name_tok,
                              const MacroDefinition& md, SourceRange range) {
  if (IsTerminated(file_states_[current_file_])) {
    return;
  }
  if (file_states_[current_file_] != FileState::kNew) {
    file_states_[current_file_] = FileState::kFatal;
    return;
  }
  current_macro_ = macro_name_tok.getIdentifierInfo()->getName().str();
  auto prev_macro_file = macro_files_.find(current_macro_);
  if (prev_macro_file == macro_files_.end()) {
    macro_files_[current_macro_] = current_file_;
    file_states_[current_file_] = FileState::kFoundDefined;
  } else {
    ReportError(current_file_, prev_macro_file->second);
    file_states_[current_file_] = FileState::kReported;
  }
}

void PrecautionCheck::If(SourceLocation loc, SourceRange condition_range,
                         ConditionValueKind conditionValue) {
  if (IsTerminated(file_states_[current_file_])) {
    return;
  }
  string logical_symbol =
      string(source_manager_->getCharacterData(condition_range.getBegin()), 1);
  if (logical_symbol != "!" ||
      file_states_[current_file_] != FileState::kFoundDefined) {
    file_states_[current_file_] = FileState::kFatal;
    return;
  }
  file_states_[current_file_] = FileState::kFoundIfndef;
}

void PrecautionCheck::Ifndef(SourceLocation loc, const Token& macro_name_tok,
                             const MacroDefinition& md) {
  if (IsTerminated(file_states_[current_file_])) {
    return;
  }
  if (file_states_[current_file_] != FileState::kNew) {
    file_states_[current_file_] = FileState::kFatal;
    return;
  }
  current_macro_ = macro_name_tok.getIdentifierInfo()->getName().str();
  auto prev_macro_file = macro_files_.find(current_macro_);
  if (prev_macro_file == macro_files_.end()) {
    macro_files_[current_macro_] = current_file_;
    file_states_[current_file_] = FileState::kFoundIfndef;
  } else {
    ReportError(current_file_, prev_macro_file->second);
    file_states_[current_file_] = FileState::kReported;
  }
}

// meet '#define NAME'
void PrecautionCheck::MacroDefined(const Token& macro_name_tok,
                                   const MacroDirective* md) {
  if (IsTerminated(file_states_[current_file_])) {
    return;
  }
  if (file_states_[current_file_] != FileState::kFoundIfndef) {
    file_states_[current_file_] = FileState::kFatal;
    return;
  }
  if (current_macro_ != macro_name_tok.getIdentifierInfo()->getName().str()) {
    file_states_[current_file_] = FileState::kFatal;
    return;
  }
  file_states_[current_file_] = FileState::kFoundDefineAfterIfndef;
}

bool PrecautionCheck::IsNotIgnoredHeaderFile(string filename) {
  if (file_states_[current_file_] == FileState::kIgnored) {
    return false;
  }
  unsigned long pos = filename.find_last_of(".");
  if (pos == string::npos) return false;
  string postfix = filename.substr(pos);
  return postfix == ".h";
}

void PrecautionCheck::CheckHeaderFile(string filename) {
  if (file_states_.find(filename) == file_states_.end()) {
    return;
  }
  if (file_states_[filename] == FileState::kFoundDefineAfterIfndef ||
      file_states_[filename] == FileState::kReported) {
    return;
  }
  ReportError(filename);
}

void PrecautionCheck::ReportError(string filename) {
  string error_message = absl::StrFormat(
      "[C2313][misra-c2012-dir-4.10]: %s has no precaution", filename);
  analyzer::proto::Result* result =
      AddResultToResultsList(results_list_, filename, 1, error_message);
  result->set_error_kind(
      analyzer::proto::
          Result_ErrorKind_MISRA_C_2012_DIR_4_10_HAS_NO_PRECAUTION);
  result->set_filename(filename);
  LOG(INFO) << error_message;
}

void PrecautionCheck::ReportError(string filename, string other_filename) {
  // TODO: use multiple place.
  string error_message = absl::StrFormat(
      "[C2313][misra-c2012-dir-4.10]: %s and %s has same file identifier",
      filename, other_filename);
  std::vector<string> locations{filename + ":1:1", other_filename + ":1:1"};
  analyzer::proto::Result* result = AddMultipleLocationsResultToResultsList(
      results_list_, filename, 1, error_message, locations);
  result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_DIR_4_10_HAS_SAME_FILE_ID);
  result->set_filename(filename);
  result->set_other_filename(other_filename);
  LOG(INFO) << error_message;
}

bool PrecautionAction::BeginSourceFileAction(CompilerInstance& ci) {
  std::unique_ptr<PrecautionCheck> precaution_callback(new PrecautionCheck());
  precaution_callback->Init(results_list_, &ci.getSourceManager());
  Preprocessor& pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::move(precaution_callback));
  return true;
}

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
}

}  // namespace dir_4_10
}  // namespace misra
