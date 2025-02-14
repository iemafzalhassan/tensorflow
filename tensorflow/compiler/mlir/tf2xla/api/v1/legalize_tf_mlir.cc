/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/tf2xla/api/v1/legalize_tf_mlir.h"

#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "llvm/ADT/StringRef.h"
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/DialectRegistry.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/OwningOpRef.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "stablehlo/dialect/Register.h"  // from @stablehlo
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/set_tpu_infeed_layout.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/serialize_mlir_module_utils.h"
#include "tensorflow/compiler/mlir/tf2xla/api/v0/compile_mlir_util.h"
#include "tensorflow/compiler/tf2xla/layout_util.h"
#include "tensorflow/compiler/tf2xla/xla_compiler.h"
#include "tensorflow/compiler/tf2xla/xla_helpers.h"
#include "tensorflow/compiler/xla/mlir_hlo/mhlo/IR/register.h"
#include "tensorflow/compiler/xla/shape.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/profile_utils/cpu_utils.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/protobuf/tpu/compile_metadata.pb.h"
#include "tensorflow/core/tpu/kernels/tpu_compile_op_support.h"
#include "tensorflow/core/tpu/tpu_compile.h"
#include "tensorflow/tsl/lib/monitoring/sampler.h"
#include "tensorflow/tsl/platform/error_logging.h"
#include "tensorflow/tsl/platform/errors.h"
#include "tensorflow/tsl/platform/status.h"
#include "tensorflow/tsl/platform/statusor.h"

namespace tensorflow {
namespace tf2xla {
namespace internal {
// TODO(b/294891109) Move this new internal code to a better suited directory.

auto* phase2_bridge_compilation_time = tsl::monitoring::Sampler<1>::New(
    {"/tensorflow/core/tf2xla/api/v1/phase2_compilation_time",
     "The wall-clock time spent on executing graphs in milliseconds.",
     "configuration"},
    // Power of 1.5 with bucket count 45 (> 23 hours)
    {tsl::monitoring::Buckets::Exponential(1, 1.5, 45)});

// Name of component for error logging. This name is fixed and required to
// enable logging.
constexpr char kBridgeComponent[] = "TFXLABridge";

using tpu::MlirToHloArgs;
using tpu::ShardingAndIndex;

// Time the execution of kernels (in CPU cycles). Meant to be used as RAII.
struct CompilationTimer {
  uint64 start_cycles = profile_utils::CpuUtils::GetCurrentClockCycle();

  uint64 ElapsedCycles() {
    return profile_utils::CpuUtils::GetCurrentClockCycle() - start_cycles;
  }

  int64_t ElapsedCyclesInMilliseconds() {
    std::chrono::duration<double> duration =
        profile_utils::CpuUtils::ConvertClockCycleToTime(ElapsedCycles());

    return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
        .count();
  }
};

Status CompileFromMlirToXlaHlo(
    const MlirToHloArgs& computation,
    const tpu::TPUCompileMetadataProto& metadata, llvm::StringRef device_type,
    const XlaShapeLayoutHelpers::ShapeDeterminationFns& shape_determination_fns,
    bool use_tuple_args, XlaCompiler::CompilationResult* compilation_result,
    std::vector<std::unique_ptr<mlir::Pass>>& custom_legalization_passes,
    const std::vector<TensorShape>& arg_shapes,
    std::vector<ShardingAndIndex>* arg_core_mapping,
    std::vector<std::vector<xla::Shape>>* per_core_arg_shapes) {
  LOG_FIRST_N(INFO, 1)
      << "Compiling MLIR computation to XLA HLO using MLIR tf2xla bridge in "
         "the op by op fallback mode. This is Phase 2 of the TF2XLA Bridge. "
         "Old (non-MLIR) bridge may be used in case of unsupported feature "
         "or compilation failure from the MLIR bridge (full fallback mode).";

  mlir::DialectRegistry registry;
  mlir::RegisterAllTensorFlowDialects(registry);
  mlir::mhlo::registerAllMhloDialects(registry);
  mlir::stablehlo::registerAllDialects(registry);
  mlir::MLIRContext context(registry);
  mlir::OwningOpRef<mlir::ModuleOp> mlir_module;
  TF_RETURN_IF_ERROR(
      DeserializeMlirModule(computation.mlir_module, &context, &mlir_module));
  if (!mlir::SetTPUInfeedLayout(mlir_module))
    return errors::Internal("Failed to set layouts attribute");

  TF_RETURN_IF_ERROR(CompileSerializedMlirToXlaHlo(
      SerializeMlirModule(mlir_module.get()), arg_shapes, device_type,
      use_tuple_args, true, shape_determination_fns, compilation_result,
      custom_legalization_passes, metadata.module_name()));

  // Compute how arguments are shared across different cores.
  auto sharding_result =
      tpu::GetShardingInfo(metadata, arg_shapes, shape_determination_fns,
                           arg_core_mapping, per_core_arg_shapes);
  if (!sharding_result.ok()) {
    return sharding_result;
  }
  // TODO(b/288289388) return serialized mlir module generated by all the MLIR
  // bridge transformations.
  return tsl::OkStatus();
}

tsl::StatusOr<XlaCompilationResult> LegalizeWithMlirBridge(
    const tpu::MlirToHloArgs& computation,
    const tpu::TPUCompileMetadataProto& metadata, bool use_tuple_args,
    llvm::StringRef device_type,
    XlaShapeLayoutHelpers::ShapeDeterminationFns shape_determination_fns,
    const std::vector<tensorflow::TensorShape>& arg_shapes,
    std::vector<tpu::ShardingAndIndex>* arg_core_mapping,
    std::vector<std::vector<xla::Shape>>* per_core_arg_shapes,
    std::vector<std::unique_ptr<mlir::Pass>>& custom_legalization_passes,
    XlaCompilationResult* compilation_result) {
  // We could only end up here if the MLIR bridge was explicitly enabled or
  // if it was in the default/unspecified state and graph analysis in the first
  // phase has not identified unsupported features.
  // Enabling op fallback also enables whole graph fallback if op by op
  // fallback failed.

  Status mlir_bridge_status;
  {
    CompilationTimer timer;
    const std::string kMlirBridgeFallback = "mlir_bridge_op_fallback_enabled";

    mlir_bridge_status = CompileFromMlirToXlaHlo(
        computation, metadata, device_type, shape_determination_fns,
        use_tuple_args, compilation_result, custom_legalization_passes,
        arg_shapes, arg_core_mapping, per_core_arg_shapes);

    phase2_bridge_compilation_time->GetCell(kMlirBridgeFallback)
        ->Add(timer.ElapsedCyclesInMilliseconds());
  }

  if (mlir_bridge_status.ok()) {
    VLOG(1) << "Successfully compiled MLIR computation to XLA HLO using MLIR "
               "tf2xla bridge";
    return *compilation_result;
  }

  tsl::error_logging::Log(kBridgeComponent,
                          "TFXLA_API_V1_BRIDGE_WITH_FALLBACK_FAIL",
                          mlir_bridge_status.ToString())
      .IgnoreError();

  return mlir_bridge_status;
}

};  // namespace internal
};  // namespace tf2xla
};  // namespace tensorflow
