/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/stdio.h"

#include "file_io.h"

#include <cstring>
#include <new>

std::unique_ptr<char[]> loadDataFromFile(
    const char *filename,
    size_t &retSize) {
    FILE *fp = nullptr;
    size_t nsize = 0;
    std::unique_ptr<char[]> ret;

    DEBUG_BREAK_IF(nullptr == filename);
    // Open the file
    fopen_s(&fp, filename, "rb");
    if (fp) {
        // Allocate a buffer for the file contents
        fseek(fp, 0, SEEK_END);
        nsize = static_cast<size_t>(ftell(fp));
        UNRECOVERABLE_IF(nsize == static_cast<size_t>(-1));

        fseek(fp, 0, SEEK_SET);

        ret.reset(new (std::nothrow) char[nsize + 1]);

        if (ret) {
            // we initialize to all zeroes before reading in data
            memset(ret.get(), 0x00, nsize + 1);
            [[maybe_unused]] auto read = fread(ret.get(), sizeof(unsigned char), nsize, fp);
            DEBUG_BREAK_IF(read != nsize);
        } else {
            nsize = 0;
        }

        fclose(fp);
    }

    retSize = nsize;
    return ret;
}

void dumpFileIncrement(const char *data, size_t dataSize, const std::string &filename, const std::string &extension) {
    auto filenameWithExt = filename + extension;
    int suffix = 0;
    while (fileExists(filenameWithExt)) {
        filenameWithExt = filename + "_" + std::to_string(suffix) + extension;
        suffix++;
    }
    writeDataToFile(filenameWithExt.c_str(), data, dataSize);
}
