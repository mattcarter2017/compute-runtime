/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"

#include <sstream>
namespace NEO::Zebin::ZeInfo {
void readZeInfoValueCheckedExtra(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &execEnvMetadataNd, KernelExecutionEnvBaseT &kernelExecEnv, ConstStringRef context,
                                 ConstStringRef key, std::string &outErrReason, std::string &outWarning, bool &validExecEnv, DecodeError &error) {

    std::ostringstream entry;
    entry << "\"" << key.str() << "\" in context of " << context.str();
    encounterUnknownZeInfoAttribute(entry.str(), outErrReason, outWarning, error);
}

namespace Types::Kernel::ExecutionEnv {
ExecutionEnvExt *allocateExecEnvExt() {
    return nullptr;
}
void freeExecEnvExt(ExecutionEnvExt *envExt) {
}
} // namespace Types::Kernel::ExecutionEnv

void populateKernelExecutionEnvironmentExt(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv, const Types::Version &srcZeInfoVersion) {
}

} // namespace NEO::Zebin::ZeInfo
