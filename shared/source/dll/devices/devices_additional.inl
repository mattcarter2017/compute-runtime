/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
DEVICE(0x0BD0, PVC_CONFIG)
DEVICE(0x0BD5, PVC_CONFIG)
DEVICE(0x0BD6, PVC_CONFIG)
DEVICE(0x0BD7, PVC_CONFIG)
DEVICE(0x0BD8, PVC_CONFIG)
DEVICE(0x0BD9, PVC_CONFIG)
DEVICE(0x0BDA, PVC_CONFIG)
DEVICE(0x0BDB, PVC_CONFIG)
#endif
#endif

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_DG2
DEVICE(0x4F80, DG2_CONFIG)
DEVICE(0x4F81, DG2_CONFIG)
DEVICE(0x4F82, DG2_CONFIG)
DEVICE(0x4F83, DG2_CONFIG)
DEVICE(0x4F84, DG2_CONFIG)
DEVICE(0x4F87, DG2_CONFIG)
DEVICE(0x4F88, DG2_CONFIG)
DEVICE(0x5690, DG2_CONFIG)
DEVICE(0x5691, DG2_CONFIG)
DEVICE(0x5692, DG2_CONFIG)
DEVICE(0x5693, DG2_CONFIG)
DEVICE(0x5694, DG2_CONFIG)
DEVICE(0x5695, DG2_CONFIG)
DEVICE(0x56A0, DG2_CONFIG)
DEVICE(0x56A1, DG2_CONFIG)
DEVICE(0x56A2, DG2_CONFIG)
DEVICE(0x56A5, DG2_CONFIG)
DEVICE(0x56A6, DG2_CONFIG)
DEVICE(0x56C0, DG2_CONFIG)
DEVICE(0x56C1, DG2_CONFIG)
#endif
#endif

#ifdef SUPPORT_GEN12LP
#ifdef SUPPORT_ADLP
DEVICE(0x46A0, ADLP_CONFIG)
DEVICE(0x46B0, ADLP_CONFIG)
DEVICE(0x46A1, ADLP_CONFIG)
DEVICE(0x46A2, ADLP_CONFIG)
DEVICE(0x46A3, ADLP_CONFIG)
DEVICE(0x46A6, ADLP_CONFIG)
DEVICE(0x46A8, ADLP_CONFIG)
DEVICE(0x46AA, ADLP_CONFIG)
DEVICE(0x462A, ADLP_CONFIG)
DEVICE(0x4626, ADLP_CONFIG)
DEVICE(0x4628, ADLP_CONFIG)
DEVICE(0x46B1, ADLP_CONFIG)
DEVICE(0x46B2, ADLP_CONFIG)
DEVICE(0x46B3, ADLP_CONFIG)
DEVICE(0x46C0, ADLP_CONFIG)
DEVICE(0x46C1, ADLP_CONFIG)
DEVICE(0x46C2, ADLP_CONFIG)
DEVICE(0x46C3, ADLP_CONFIG)
// RPL-P
DEVICE(0xA7A0, ADLP_CONFIG)
DEVICE(0xA720, ADLP_CONFIG)
DEVICE(0xA7A8, ADLP_CONFIG)
DEVICE(0xA7A1, ADLP_CONFIG)
DEVICE(0xA721, ADLP_CONFIG)
DEVICE(0xA7A9, ADLP_CONFIG)
#endif
#endif
