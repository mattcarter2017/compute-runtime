/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/stdio.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include "gtest/gtest.h"

#include <cstdio>

TEST(FileIO, GivenNonEmptyFileWhenCheckingIfHasSizeThenReturnTrue) {
    std::string fileName("fileIO.bin");
    if (virtualFileExists(fileName)) {
        removeVirtualFile(fileName);
    }

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    writeDataToFile(fileName.c_str(), "TEST", 4);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_TRUE(fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}

TEST(FileIO, GivenEmptyFileWhenCheckingIfHasSizeThenReturnFalse) {
    std::string fileName("fileIO.bin");
    if (virtualFileExists(fileName)) {
        removeVirtualFile(fileName);
    }

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    writeDataToFile(fileName.c_str(), "", 0);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_FALSE(fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}
