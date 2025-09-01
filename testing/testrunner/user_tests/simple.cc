// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/status_macros.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testing/testrunner/cel_expression_source.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "google/protobuf/text_format.h"

namespace cel::testing {

using ::cel::test::CelTestContext;
using ::cel::expr::CheckedExpr;

template <typename T>
T ParseTextProtoOrDie(absl::string_view text_proto) {
  T result;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text_proto, &result));
  return result;
}

CEL_REGISTER_TEST_SUITE_FACTORY([]() {
  return ParseTextProtoOrDie<cel::expr::conformance::test::TestSuite>(R"pb(
    name: "custom_test_suite_tests"
    description: "Simple tests to validate the test runner."
    sections: {
      name: "simple_map_operations"
      description: "Tests for map operations."
      tests: {
        name: "literal_and_sum"
        description: "Test that a map can be created and values can be accessed."
        input: {
          key: "x"
          value { value { int64_value: 1 } }
        }
        input {
          key: "y"
          value { value { int64_value: 2 } }
        }
        output {
          result_value {
            map_value {
              entries {
                key { string_value: "literal" }
                value { int64_value: 3 }
              }
              entries {
                key { string_value: "sum" }
                value { int64_value: 3 }
              }
            }
          }
        }
      }
    }
  )pb");
});

CEL_REGISTER_TEST_CONTEXT_FACTORY(
    []() -> absl::StatusOr<std::unique_ptr<CelTestContext>> {
      ABSL_LOG(INFO) << "Creating test context";

      // Create a compiler.
      CEL_ASSIGN_OR_RETURN(
          std::unique_ptr<cel::CompilerBuilder> builder,
          cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
      CEL_RETURN_IF_ERROR(builder->AddLibrary(cel::StandardCompilerLibrary()));
      cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
      CEL_RETURN_IF_ERROR(checker_builder.AddVariable(
          cel::MakeVariableDecl("x", cel::IntType())));
      CEL_RETURN_IF_ERROR(checker_builder.AddVariable(
          cel::MakeVariableDecl("y", cel::IntType())));
      CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Compiler> compiler,
                           builder->Build());

      // Compile the expression.
      CEL_ASSIGN_OR_RETURN(cel::ValidationResult validation_result,
                           compiler->Compile("{'sum': x + y, 'literal': 3}"));
      CheckedExpr checked_expr;
      CEL_RETURN_IF_ERROR(
          cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr));

      // Create a runtime.
      CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder runtime_builder,
                           cel::CreateStandardRuntimeBuilder(
                               cel::internal::GetTestingDescriptorPool(), {}));
      CEL_ASSIGN_OR_RETURN(std::unique_ptr<const cel::Runtime> runtime,
                           std::move(runtime_builder).Build());

      return CelTestContext::CreateFromRuntime(
          std::move(runtime),
          /*options=*/{.expression_source =
                           test::CelExpressionSource::FromCheckedExpr(
                               std::move(checked_expr))});
    });
}  // namespace cel::testing
