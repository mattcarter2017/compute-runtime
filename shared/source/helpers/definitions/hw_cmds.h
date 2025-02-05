/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef SUPPORT_GEN12LP
#include "shared/source/gen12lp/hw_cmds.h"
#endif
#ifdef SUPPORT_XE_HPG_CORE
#include "shared/source/xe_hpg_core/hw_cmds.h"
#endif
#ifdef SUPPORT_XE_HPC_CORE
#include "shared/source/xe_hpc_core/hw_cmds.h"
#endif
#ifdef SUPPORT_XE2_HPG_CORE
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#endif
#ifdef SUPPORT_XE3_CORE
#include "hw_cmds_xe3_core.h"
#endif
