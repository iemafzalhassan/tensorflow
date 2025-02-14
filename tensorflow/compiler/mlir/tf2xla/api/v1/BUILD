load("//tensorflow/core/platform:build_config.bzl", "tf_proto_library")
load("//tensorflow/core/platform:rules_cc.bzl", "cc_library")
load("//tensorflow:tensorflow.bzl", "tf_cc_test")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = [
        ":__subpackages__",
        ":tf2xla_users",
    ],
)

# Please reach out to tf-bridge-team@ before using the TF2XLA bridge.
package_group(name = "tf2xla_users")

cc_library(
    name = "legalize_tf_mlir",
    srcs = ["legalize_tf_mlir.cc"],
    hdrs = ["legalize_tf_mlir.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow/compiler/mlir/tensorflow",
        "//tensorflow/compiler/mlir/tensorflow:serialize_mlir_module_utils",
        "//tensorflow/compiler/mlir/tensorflow:set_tpu_infeed_layout",
        "//tensorflow/compiler/mlir/tf2xla:compile_mlir_util",
        "//tensorflow/compiler/mlir/tf2xla/api/v0:compile_tf_graph",
        "//tensorflow/compiler/tf2xla:layout_util",
        "//tensorflow/compiler/tf2xla:xla_compiler",
        "//tensorflow/compiler/tf2xla:xla_helpers",
        "//tensorflow/compiler/xla:shape_util",
        "//tensorflow/compiler/xla/mlir_hlo:hlo_dialect_registration",
        "//tensorflow/core:framework",
        "//tensorflow/core:lib",
        "//tensorflow/core:protos_all_cc",
        "//tensorflow/core/platform:statusor",
        "//tensorflow/core/protobuf/tpu:compile_metadata_proto_cc",
        "//tensorflow/core/tpu:tpu_compile",
        "//tensorflow/core/tpu/kernels:tpu_compile_op_support",
        "//tensorflow/tsl/platform:error_logging",
        "//tensorflow/tsl/platform:status",
        "//tensorflow/tsl/platform:statusor",
        "@com_google_absl//absl/log",
        "@llvm-project//llvm:Support",
        "@llvm-project//mlir:IR",
        "@llvm-project//mlir:Pass",
        "@stablehlo//:register",
    ],
)

cc_library(
    name = "legalize_tf",
    srcs = ["legalize_tf.cc"],
    hdrs = ["legalize_tf.h"],
    deps = [
        ":device_type_proto_cc",
        ":legalize_tf_mlir",
        "//tensorflow/compiler/jit:flags_headers",
        "//tensorflow/compiler/jit:shape_inference",
        "//tensorflow/compiler/mlir/tensorflow",
        "//tensorflow/compiler/mlir/tensorflow:dump_mlir_util",
        "//tensorflow/compiler/mlir/tensorflow:error_util",
        "//tensorflow/compiler/mlir/tensorflow:export_graphdef",
        "//tensorflow/compiler/mlir/tensorflow:mlir_roundtrip_flags",
        "//tensorflow/compiler/mlir/tensorflow:serialize_mlir_module_utils",
        "//tensorflow/compiler/mlir/tensorflow:set_tpu_infeed_layout",
        "//tensorflow/compiler/mlir/tensorflow:tensorflow_passes",
        "//tensorflow/compiler/mlir/tensorflow:translate_utils",
        "//tensorflow/compiler/mlir/tf2xla:compile_mlir_util",
        "//tensorflow/compiler/mlir/tf2xla/api/v0:compile_tf_graph",
        "//tensorflow/compiler/tf2xla:layout_util",
        "//tensorflow/compiler/tf2xla:tf2xla_util",
        "//tensorflow/compiler/tf2xla:xla_helpers",
        "//tensorflow/compiler/xla:xla_proto_cc",
        "//tensorflow/compiler/xla/client:compile_only_client",
        "//tensorflow/compiler/xla/hlo/ir:hlo",
        "//tensorflow/compiler/xla/mlir_hlo:hlo_dialect_registration",
        "//tensorflow/compiler/xla/pjrt:compile_options_proto_cc",
        "//tensorflow/core:framework",
        "//tensorflow/core:lib",
        "//tensorflow/core/platform:statusor",
        "//tensorflow/core/tpu:tpu_compile",
        "//tensorflow/core/tpu/kernels:tpu_compile_op_support",
        "//tensorflow/core/tpu/kernels:tpu_compile_proto_cc",
        "//tensorflow/core/tpu/kernels:tpu_util_hdrs",
        "//tensorflow/tsl/platform:error_logging",
        "//tensorflow/tsl/platform:status",
        "//tensorflow/tsl/platform:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:variant",
        "@llvm-project//mlir:IR",
        "@llvm-project//mlir:Pass",
        "@stablehlo//:register",
    ],
)

tf_cc_test(
    name = "legalize_tf_mlir_test",
    srcs = ["legalize_tf_mlir_test.cc"],
    deps = [
        ":legalize_tf_mlir",
        "//tensorflow/compiler/jit",
        "//tensorflow/compiler/mlir/tf2xla:compile_mlir_util",
        "//tensorflow/compiler/tf2xla:xla_compiler",
        "//tensorflow/compiler/tf2xla:xla_helpers",
        "//tensorflow/compiler/xla:shape_util",
        "//tensorflow/core:framework",
        "//tensorflow/core:test_main",
        "//tensorflow/core/protobuf/tpu:compile_metadata_proto_cc",
        "//tensorflow/core/tpu/kernels:tpu_compile_op_support",
        "//tensorflow/tsl/platform:statusor",
        "@com_google_googletest//:gtest",
        "@llvm-project//mlir:Pass",
    ],
)

tf_cc_test(
    name = "legalize_tf_test",
    srcs = ["legalize_tf_test.cc"],
    deps = [
        ":device_type_proto_cc",
        ":legalize_tf",
        "//tensorflow/compiler/jit",
        "//tensorflow/compiler/tf2xla:xla_helpers",
        "//tensorflow/compiler/xla/client:client_library",
        "//tensorflow/core:test",
        "//tensorflow/core:test_main",
        "//tensorflow/core/lib/monitoring:cell_reader",
        "//tensorflow/core/protobuf/tpu:compile_metadata_proto_cc",
        "//tensorflow/core/tpu/kernels:tpu_compile_op_support",
        "//tensorflow/tsl/lib/monitoring:test_utils",
        "//tensorflow/tsl/platform:statusor",
        "@com_google_googletest//:gtest",
    ],
)

tf_cc_test(
    name = "legalize_tf_quant_test",
    srcs = ["legalize_tf_quant_test.cc"],
    tags = [
        "no_oss",  # TODO(b/288215766): Temporarily disable OSS.
    ],
    deps = [
        "//tensorflow/compiler/jit",
        "//tensorflow/compiler/mlir/tf2xla:compile_mlir_util",
        "//tensorflow/core:test",
        "//tensorflow/core:test_main",
        "//tensorflow/tsl/platform:statusor",
        "@com_google_googletest//:gtest",
    ],
)

tf_proto_library(
    name = "device_type_proto",
    srcs = ["device_type.proto"],
    cc_api_version = 2,
)
