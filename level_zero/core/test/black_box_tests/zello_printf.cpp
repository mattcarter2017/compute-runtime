/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#ifdef _WIN32
#include <windows.h>

#include <io.h>

#pragma warning(disable : 4996)
#else
#include <unistd.h>
#endif

static constexpr std::array<const char *, 2> kernelNames = {"printf_kernel",
                                                            "printf_kernel1"};

enum class PrintfExecutionMode : uint32_t {
    commandQueue,
    immSyncCmdList
};

void createModule(const ze_context_handle_t context, const ze_device_handle_t device, ze_module_handle_t &module) {
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::printfKernelSource, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "";

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));
}

void createKernel(const ze_module_handle_t module, ze_kernel_handle_t &kernel, const char *kernelName) {

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = kernelName;
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
}

void runPrintfKernel(const ze_module_handle_t &module, const ze_kernel_handle_t &kernel,
                     ze_context_handle_t &context, ze_device_handle_t &device, uint32_t id, PrintfExecutionMode mode) {

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = (mode == PrintfExecutionMode::immSyncCmdList);

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    ze_group_count_t dispatchTraits;
    if (id == 0) {

        [[maybe_unused]] int8_t byteValue = std::numeric_limits<int8_t>::max();
        [[maybe_unused]] int16_t shortValue = std::numeric_limits<int16_t>::max();
        [[maybe_unused]] int32_t intValue = std::numeric_limits<int32_t>::max();
        [[maybe_unused]] int64_t longValue = std::numeric_limits<int64_t>::max();

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(byteValue), &byteValue));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(shortValue), &shortValue));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(intValue), &intValue));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(longValue), &longValue));

        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 1U, 1U, 1U));

        dispatchTraits.groupCountX = 1u;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;
    } else if (id == 1) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 32U, 1U, 1U));
        dispatchTraits.groupCountX = 10u;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;
    }
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(kernel, dispatchTraits));

    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());
}

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    const char *fileName = "zello_printf_output.txt";
    bool validatePrintfOutput = true;
    bool printfValidated = false;
    int stdoutFd = -1;

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_module_handle_t module = nullptr;
    createModule(context, device, module);

    std::array<std::string, 2> expectedStrings = {
        "byte = 127\nshort = 32767\nint = 2147483647\nlong = 9223372036854775807",
        "id == 0\nid == 0\nid == 0\nid == 0\nid == 0\n"
        "id == 0\nid == 0\nid == 0\nid == 0\nid == 0\n"};

    PrintfExecutionMode executionModes[] = {PrintfExecutionMode::commandQueue, PrintfExecutionMode::immSyncCmdList};

    for (auto mode : executionModes) {
        for (uint32_t i = 0; i < 2; i++) {

            if (validatePrintfOutput) {
                // duplicate stdout descriptor
                stdoutFd = dup(fileno(stdout));
                auto newFile = freopen(fileName, "w", stdout);
                if (newFile == nullptr) {
                    std::cerr << "Failed in freopen()" << std::endl;
                    abort();
                }
            }

            ze_kernel_handle_t kernel = nullptr;
            createKernel(module, kernel, kernelNames[i]);

            runPrintfKernel(module, kernel, context, device, i, mode);

            if (validatePrintfOutput) {
                printfValidated = false;
                auto sizeOfBuffer = expectedStrings[0].size() + 1024;
                auto kernelOutput = std::make_unique<char[]>(sizeOfBuffer);
                memset(kernelOutput.get(), 0, sizeOfBuffer);
                auto kernelOutputFile = fopen(fileName, "r");
                [[maybe_unused]] auto result = fread(kernelOutput.get(), sizeof(char), sizeOfBuffer - 1, kernelOutputFile);
                fclose(kernelOutputFile);
                fflush(stdout);
                // adjust/restore stdout to previous descriptor
                dup2(stdoutFd, fileno(stdout));
                // close duplicate
                close(stdoutFd);
                std::remove(fileName);

                char *foundString = strstr(kernelOutput.get(), expectedStrings[i].c_str());
                if (foundString != nullptr && result == expectedStrings[i].size()) {
                    printfValidated = true;
                }

                if (result > 0) {
                    std::cout << "\nPrintf output:\n"
                              << "------------------------------\n"
                              << kernelOutput.get() << "\n------------------------------\n"
                              << "valid output = " << printfValidated << std::endl;
                    if (!printfValidated) {
                        std::cout << "Expected printf output:\n"
                                  << "------------------------------\n"
                                  << expectedStrings[i] << "\n------------------------------\n"
                                  << std::endl
                                  << std::endl;
                    }
                } else {
                    std::cout << "Printf output empty!" << std::endl;
                }
            }

            SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));

            if (validatePrintfOutput && !printfValidated) {
                SUCCESS_OR_TERMINATE(zeContextDestroy(context));
                std::cerr << "\nZello Printf FAILED " << std::endl;
                return -1;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
    std::cout << "\nZello Printf PASSED " << std::endl;

    return 0;
}
