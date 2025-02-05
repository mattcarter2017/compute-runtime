/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

namespace NEO {
namespace Linux {

void adjustLibraryFlags(int &dlopenFlag);

class OsLibrary : public NEO::OsLibrary {
  private:
    void *handle;

  public:
    OsLibrary(const OsLibraryCreateProperties &properties);
    ~OsLibrary() override;

    bool isLoaded() override;
    void *getProcAddress(const std::string &procName) override;
    std::string getFullPath() override;
};
} // namespace Linux
} // namespace NEO
