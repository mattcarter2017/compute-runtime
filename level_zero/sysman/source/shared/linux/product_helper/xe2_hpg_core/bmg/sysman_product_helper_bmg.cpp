/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_BMG;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x1e2f8200", // BMG PUNIT rev 1
     {{"VRAM_BANDWIDTH", 56}}},
    {"0x5e2f8210", // BMG OOBMSM Rev 15
     {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 164},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 168},
      {"reg_PCIESS_rx_bytecount_lsb", 280},
      {"reg_PCIESS_rx_bytecount_msb", 276},
      {"reg_PCIESS_tx_bytecount_lsb", 288},
      {"reg_PCIESS_tx_bytecount_msb", 284},
      {"reg_PCIESS_rx_pktcount_lsb", 296},
      {"reg_PCIESS_rx_pktcount_msb", 292},
      {"reg_PCIESS_tx_pktcount_lsb", 304},
      {"reg_PCIESS_tx_pktcount_msb", 300},
      {"MSU_BITMASK", 3688},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 368},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 372},
      {"GDDR0_CH0_GT_32B_RD_REQ_UPPER", 376},
      {"GDDR0_CH0_GT_32B_RD_REQ_LOWER", 380},
      {"GDDR1_CH0_GT_32B_RD_REQ_UPPER", 536},
      {"GDDR1_CH0_GT_32B_RD_REQ_LOWER", 540},
      {"GDDR2_CH0_GT_32B_RD_REQ_UPPER", 696},
      {"GDDR2_CH0_GT_32B_RD_REQ_LOWER", 700},
      {"GDDR3_CH0_GT_32B_RD_REQ_UPPER", 856},
      {"GDDR3_CH0_GT_32B_RD_REQ_LOWER", 860},
      {"GDDR4_CH0_GT_32B_RD_REQ_UPPER", 1016},
      {"GDDR4_CH0_GT_32B_RD_REQ_LOWER", 1020},
      {"GDDR5_CH0_GT_32B_RD_REQ_UPPER", 1176},
      {"GDDR5_CH0_GT_32B_RD_REQ_LOWER", 1180},
      {"GDDR0_CH1_GT_32B_RD_REQ_UPPER", 456},
      {"GDDR0_CH1_GT_32B_RD_REQ_LOWER", 460},
      {"GDDR1_CH1_GT_32B_RD_REQ_UPPER", 616},
      {"GDDR1_CH1_GT_32B_RD_REQ_LOWER", 620},
      {"GDDR2_CH1_GT_32B_RD_REQ_UPPER", 776},
      {"GDDR2_CH1_GT_32B_RD_REQ_LOWER", 780},
      {"GDDR3_CH1_GT_32B_RD_REQ_UPPER", 936},
      {"GDDR3_CH1_GT_32B_RD_REQ_LOWER", 940},
      {"GDDR4_CH1_GT_32B_RD_REQ_UPPER", 1096},
      {"GDDR4_CH1_GT_32B_RD_REQ_LOWER", 1100},
      {"GDDR5_CH1_GT_32B_RD_REQ_UPPER", 1256},
      {"GDDR5_CH1_GT_32B_RD_REQ_LOWER", 1260},
      {"GDDR0_CH0_GT_32B_WR_REQ_UPPER", 392},
      {"GDDR0_CH0_GT_32B_WR_REQ_LOWER", 396},
      {"GDDR1_CH0_GT_32B_WR_REQ_UPPER", 552},
      {"GDDR1_CH0_GT_32B_WR_REQ_LOWER", 556},
      {"GDDR2_CH0_GT_32B_WR_REQ_UPPER", 712},
      {"GDDR2_CH0_GT_32B_WR_REQ_LOWER", 716},
      {"GDDR3_CH0_GT_32B_WR_REQ_UPPER", 872},
      {"GDDR3_CH0_GT_32B_WR_REQ_LOWER", 876},
      {"GDDR4_CH0_GT_32B_WR_REQ_UPPER", 1032},
      {"GDDR4_CH0_GT_32B_WR_REQ_LOWER", 1036},
      {"GDDR5_CH0_GT_32B_WR_REQ_UPPER", 1192},
      {"GDDR5_CH0_GT_32B_WR_REQ_LOWER", 1196},
      {"GDDR0_CH1_GT_32B_WR_REQ_UPPER", 472},
      {"GDDR0_CH1_GT_32B_WR_REQ_LOWER", 476},
      {"GDDR1_CH1_GT_32B_WR_REQ_UPPER", 632},
      {"GDDR1_CH1_GT_32B_WR_REQ_LOWER", 636},
      {"GDDR2_CH1_GT_32B_WR_REQ_UPPER", 792},
      {"GDDR2_CH1_GT_32B_WR_REQ_LOWER", 796},
      {"GDDR3_CH1_GT_32B_WR_REQ_UPPER", 952},
      {"GDDR3_CH1_GT_32B_WR_REQ_LOWER", 956},
      {"GDDR4_CH1_GT_32B_WR_REQ_UPPER", 1112},
      {"GDDR4_CH1_GT_32B_WR_REQ_LOWER", 1116},
      {"GDDR5_CH1_GT_32B_WR_REQ_UPPER", 1272},
      {"GDDR5_CH1_GT_32B_WR_REQ_LOWER", 1276},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 408},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 412},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 568},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 572},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 728},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 732},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 888},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 892},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 1048},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 1052},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 1208},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 1212},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 488},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 492},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 648},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 652},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 808},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 812},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 968},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 972},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 564},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 568},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 1288},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 1292},
      {"GDDR0_CH0_SOC_32B_RD_REQ_UPPER", 424},
      {"GDDR0_CH0_SOC_32B_RD_REQ_LOWER", 428},
      {"GDDR1_CH0_SOC_32B_RD_REQ_UPPER", 584},
      {"GDDR1_CH0_SOC_32B_RD_REQ_LOWER", 588},
      {"GDDR2_CH0_SOC_32B_RD_REQ_UPPER", 744},
      {"GDDR2_CH0_SOC_32B_RD_REQ_LOWER", 748},
      {"GDDR3_CH0_SOC_32B_RD_REQ_UPPER", 904},
      {"GDDR3_CH0_SOC_32B_RD_REQ_LOWER", 908},
      {"GDDR4_CH0_SOC_32B_RD_REQ_UPPER", 1064},
      {"GDDR4_CH0_SOC_32B_RD_REQ_LOWER", 1068},
      {"GDDR5_CH0_SOC_32B_RD_REQ_UPPER", 1224},
      {"GDDR5_CH0_SOC_32B_RD_REQ_LOWER", 1228},
      {"GDDR0_CH1_SOC_32B_RD_REQ_UPPER", 504},
      {"GDDR0_CH1_SOC_32B_RD_REQ_LOWER", 508},
      {"GDDR1_CH1_SOC_32B_RD_REQ_UPPER", 664},
      {"GDDR1_CH1_SOC_32B_RD_REQ_LOWER", 668},
      {"GDDR2_CH1_SOC_32B_RD_REQ_UPPER", 824},
      {"GDDR2_CH1_SOC_32B_RD_REQ_LOWER", 828},
      {"GDDR3_CH1_SOC_32B_RD_REQ_UPPER", 984},
      {"GDDR3_CH1_SOC_32B_RD_REQ_LOWER", 988},
      {"GDDR4_CH1_SOC_32B_RD_REQ_UPPER", 1144},
      {"GDDR4_CH1_SOC_32B_RD_REQ_LOWER", 1148},
      {"GDDR5_CH1_SOC_32B_RD_REQ_UPPER", 1304},
      {"GDDR5_CH1_SOC_32B_RD_REQ_LOWER", 1308},
      {"GDDR0_CH0_SOC_32B_WR_REQ_UPPER", 440},
      {"GDDR0_CH0_SOC_32B_WR_REQ_LOWER", 444},
      {"GDDR1_CH0_SOC_32B_WR_REQ_UPPER", 600},
      {"GDDR1_CH0_SOC_32B_WR_REQ_LOWER", 604},
      {"GDDR2_CH0_SOC_32B_WR_REQ_UPPER", 760},
      {"GDDR2_CH0_SOC_32B_WR_REQ_LOWER", 764},
      {"GDDR3_CH0_SOC_32B_WR_REQ_UPPER", 920},
      {"GDDR3_CH0_SOC_32B_WR_REQ_LOWER", 924},
      {"GDDR4_CH0_SOC_32B_WR_REQ_UPPER", 1080},
      {"GDDR4_CH0_SOC_32B_WR_REQ_LOWER", 1084},
      {"GDDR5_CH0_SOC_32B_WR_REQ_UPPER", 1240},
      {"GDDR5_CH0_SOC_32B_WR_REQ_LOWER", 1244},
      {"GDDR0_CH1_SOC_32B_WR_REQ_UPPER", 520},
      {"GDDR0_CH1_SOC_32B_WR_REQ_LOWER", 524},
      {"GDDR1_CH1_SOC_32B_WR_REQ_UPPER", 680},
      {"GDDR1_CH1_SOC_32B_WR_REQ_LOWER", 684},
      {"GDDR2_CH1_SOC_32B_WR_REQ_UPPER", 840},
      {"GDDR2_CH1_SOC_32B_WR_REQ_LOWER", 844},
      {"GDDR3_CH1_SOC_32B_WR_REQ_UPPER", 1000},
      {"GDDR3_CH1_SOC_32B_WR_REQ_LOWER", 1004},
      {"GDDR4_CH1_SOC_32B_WR_REQ_UPPER", 1160},
      {"GDDR4_CH1_SOC_32B_WR_REQ_LOWER", 1164},
      {"GDDR5_CH1_SOC_32B_WR_REQ_UPPER", 1320},
      {"GDDR5_CH1_SOC_32B_WR_REQ_LOWER", 1324},
      {"GDDR0_CH0_GT_64B_RD_REQ_UPPER", 384},
      {"GDDR0_CH0_GT_64B_RD_REQ_LOWER", 388},
      {"GDDR1_CH0_GT_64B_RD_REQ_UPPER", 544},
      {"GDDR1_CH0_GT_64B_RD_REQ_LOWER", 548},
      {"GDDR2_CH0_GT_64B_RD_REQ_UPPER", 704},
      {"GDDR2_CH0_GT_64B_RD_REQ_LOWER", 708},
      {"GDDR3_CH0_GT_64B_RD_REQ_UPPER", 864},
      {"GDDR3_CH0_GT_64B_RD_REQ_LOWER", 868},
      {"GDDR4_CH0_GT_64B_RD_REQ_UPPER", 1024},
      {"GDDR4_CH0_GT_64B_RD_REQ_LOWER", 1028},
      {"GDDR5_CH0_GT_64B_RD_REQ_UPPER", 1184},
      {"GDDR5_CH0_GT_64B_RD_REQ_LOWER", 1188},
      {"GDDR0_CH1_GT_64B_RD_REQ_UPPER", 464},
      {"GDDR0_CH1_GT_64B_RD_REQ_LOWER", 468},
      {"GDDR1_CH1_GT_64B_RD_REQ_UPPER", 624},
      {"GDDR1_CH1_GT_64B_RD_REQ_LOWER", 628},
      {"GDDR2_CH1_GT_64B_RD_REQ_UPPER", 784},
      {"GDDR2_CH1_GT_64B_RD_REQ_LOWER", 788},
      {"GDDR3_CH1_GT_64B_RD_REQ_UPPER", 944},
      {"GDDR3_CH1_GT_64B_RD_REQ_LOWER", 948},
      {"GDDR4_CH1_GT_64B_RD_REQ_UPPER", 1104},
      {"GDDR4_CH1_GT_64B_RD_REQ_LOWER", 1108},
      {"GDDR5_CH1_GT_64B_RD_REQ_UPPER", 1264},
      {"GDDR5_CH1_GT_64B_RD_REQ_LOWER", 1268},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 416},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 420},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 576},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 580},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 736},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 740},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 896},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 890},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 1056},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 1060},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 1216},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 1220},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 496},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 500},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 656},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 660},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 816},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 820},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 976},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 980},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 1136},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 1140},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 1296},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 1300},
      {"GDDR0_CH0_SOC_64B_RD_REQ_UPPER", 432},
      {"GDDR0_CH0_SOC_64B_RD_REQ_LOWER", 436},
      {"GDDR1_CH0_SOC_64B_RD_REQ_UPPER", 592},
      {"GDDR1_CH0_SOC_64B_RD_REQ_LOWER", 596},
      {"GDDR2_CH0_SOC_64B_RD_REQ_UPPER", 752},
      {"GDDR2_CH0_SOC_64B_RD_REQ_LOWER", 756},
      {"GDDR3_CH0_SOC_64B_RD_REQ_UPPER", 912},
      {"GDDR3_CH0_SOC_64B_RD_REQ_LOWER", 916},
      {"GDDR4_CH0_SOC_64B_RD_REQ_UPPER", 1072},
      {"GDDR4_CH0_SOC_64B_RD_REQ_LOWER", 1076},
      {"GDDR5_CH0_SOC_64B_RD_REQ_UPPER", 1232},
      {"GDDR5_CH0_SOC_64B_RD_REQ_LOWER", 1236},
      {"GDDR0_CH1_SOC_64B_RD_REQ_UPPER", 512},
      {"GDDR0_CH1_SOC_64B_RD_REQ_LOWER", 516},
      {"GDDR1_CH1_SOC_64B_RD_REQ_UPPER", 672},
      {"GDDR1_CH1_SOC_64B_RD_REQ_LOWER", 676},
      {"GDDR2_CH1_SOC_64B_RD_REQ_UPPER", 832},
      {"GDDR2_CH1_SOC_64B_RD_REQ_LOWER", 836},
      {"GDDR3_CH1_SOC_64B_RD_REQ_UPPER", 992},
      {"GDDR3_CH1_SOC_64B_RD_REQ_LOWER", 996},
      {"GDDR4_CH1_SOC_64B_RD_REQ_UPPER", 1152},
      {"GDDR4_CH1_SOC_64B_RD_REQ_LOWER", 1156},
      {"GDDR5_CH1_SOC_64B_RD_REQ_UPPER", 1312},
      {"GDDR5_CH1_SOC_64B_RD_REQ_LOWER", 1316},
      {"GDDR0_CH0_SOC_64B_WR_REQ_UPPER", 448},
      {"GDDR0_CH0_SOC_64B_WR_REQ_LOWER", 452},
      {"GDDR1_CH0_SOC_64B_WR_REQ_UPPER", 608},
      {"GDDR1_CH0_SOC_64B_WR_REQ_LOWER", 612},
      {"GDDR2_CH0_SOC_64B_WR_REQ_UPPER", 768},
      {"GDDR2_CH0_SOC_64B_WR_REQ_LOWER", 772},
      {"GDDR3_CH0_SOC_64B_WR_REQ_UPPER", 928},
      {"GDDR3_CH0_SOC_64B_WR_REQ_LOWER", 932},
      {"GDDR4_CH0_SOC_64B_WR_REQ_UPPER", 1088},
      {"GDDR4_CH0_SOC_64B_WR_REQ_LOWER", 1092},
      {"GDDR5_CH0_SOC_64B_WR_REQ_UPPER", 1248},
      {"GDDR5_CH0_SOC_64B_WR_REQ_LOWER", 1252},
      {"GDDR0_CH1_SOC_64B_WR_REQ_UPPER", 528},
      {"GDDR0_CH1_SOC_64B_WR_REQ_LOWER", 532},
      {"GDDR1_CH1_SOC_64B_WR_REQ_UPPER", 688},
      {"GDDR1_CH1_SOC_64B_WR_REQ_LOWER", 692},
      {"GDDR2_CH1_SOC_64B_WR_REQ_UPPER", 848},
      {"GDDR2_CH1_SOC_64B_WR_REQ_LOWER", 852},
      {"GDDR3_CH1_SOC_64B_WR_REQ_UPPER", 1008},
      {"GDDR3_CH1_SOC_64B_WR_REQ_LOWER", 1012},
      {"GDDR4_CH1_SOC_64B_WR_REQ_UPPER", 1168},
      {"GDDR4_CH1_SOC_64B_WR_REQ_LOWER", 1172},
      {"GDDR5_CH1_SOC_64B_WR_REQ_UPPER", 1328},
      {"GDDR5_CH1_SOC_64B_WR_REQ_LOWER", 1332},
      {"GDDR0_CH0_GT_64B_WR_REQ_UPPER", 400},
      {"GDDR0_CH0_GT_64B_WR_REQ_LOWER", 404},
      {"GDDR1_CH0_GT_64B_WR_REQ_UPPER", 560},
      {"GDDR1_CH0_GT_64B_WR_REQ_LOWER", 564},
      {"GDDR2_CH0_GT_64B_WR_REQ_UPPER", 720},
      {"GDDR2_CH0_GT_64B_WR_REQ_LOWER", 724},
      {"GDDR3_CH0_GT_64B_WR_REQ_UPPER", 880},
      {"GDDR3_CH0_GT_64B_WR_REQ_LOWER", 884},
      {"GDDR4_CH0_GT_64B_WR_REQ_UPPER", 1040},
      {"GDDR4_CH0_GT_64B_WR_REQ_LOWER", 1044},
      {"GDDR5_CH0_GT_64B_WR_REQ_UPPER", 1200},
      {"GDDR5_CH0_GT_64B_WR_REQ_LOWER", 1204},
      {"GDDR0_CH1_GT_64B_WR_REQ_UPPER", 480},
      {"GDDR0_CH1_GT_64B_WR_REQ_LOWER", 484},
      {"GDDR1_CH1_GT_64B_WR_REQ_UPPER", 640},
      {"GDDR1_CH1_GT_64B_WR_REQ_LOWER", 644},
      {"GDDR2_CH1_GT_64B_WR_REQ_UPPER", 800},
      {"GDDR2_CH1_GT_64B_WR_REQ_LOWER", 804},
      {"GDDR3_CH1_GT_64B_WR_REQ_UPPER", 960},
      {"GDDR3_CH1_GT_64B_WR_REQ_LOWER", 964},
      {"GDDR4_CH1_GT_64B_WR_REQ_UPPER", 1120},
      {"GDDR4_CH1_GT_64B_WR_REQ_LOWER", 1124},
      {"GDDR5_CH1_GT_64B_WR_REQ_UPPER", 1280},
      {"GDDR5_CH1_GT_64B_WR_REQ_LOWER", 1284}}},
    {"0x5e2f8211", // BMG OOBMSM Rev 16
     {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 164},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 168},
      {"reg_PCIESS_rx_bytecount_lsb", 280},
      {"reg_PCIESS_rx_bytecount_msb", 284},
      {"reg_PCIESS_tx_bytecount_lsb", 288},
      {"reg_PCIESS_tx_bytecount_msb", 292},
      {"reg_PCIESS_rx_pktcount_lsb", 296},
      {"reg_PCIESS_rx_pktcount_msb", 300},
      {"reg_PCIESS_tx_pktcount_lsb", 304},
      {"reg_PCIESS_tx_pktcount_msb", 308},
      {"MSU_BITMASK", 3688},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 372},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 368},
      {"GDDR0_CH0_GT_32B_RD_REQ_UPPER", 380},
      {"GDDR0_CH0_GT_32B_RD_REQ_LOWER", 376},
      {"GDDR1_CH0_GT_32B_RD_REQ_UPPER", 540},
      {"GDDR1_CH0_GT_32B_RD_REQ_LOWER", 536},
      {"GDDR2_CH0_GT_32B_RD_REQ_UPPER", 700},
      {"GDDR2_CH0_GT_32B_RD_REQ_LOWER", 696},
      {"GDDR3_CH0_GT_32B_RD_REQ_UPPER", 860},
      {"GDDR3_CH0_GT_32B_RD_REQ_LOWER", 856},
      {"GDDR4_CH0_GT_32B_RD_REQ_UPPER", 1020},
      {"GDDR4_CH0_GT_32B_RD_REQ_LOWER", 1016},
      {"GDDR5_CH0_GT_32B_RD_REQ_UPPER", 1180},
      {"GDDR5_CH0_GT_32B_RD_REQ_LOWER", 1176},
      {"GDDR0_CH1_GT_32B_RD_REQ_UPPER", 460},
      {"GDDR0_CH1_GT_32B_RD_REQ_LOWER", 456},
      {"GDDR1_CH1_GT_32B_RD_REQ_UPPER", 620},
      {"GDDR1_CH1_GT_32B_RD_REQ_LOWER", 616},
      {"GDDR2_CH1_GT_32B_RD_REQ_UPPER", 780},
      {"GDDR2_CH1_GT_32B_RD_REQ_LOWER", 776},
      {"GDDR3_CH1_GT_32B_RD_REQ_UPPER", 940},
      {"GDDR3_CH1_GT_32B_RD_REQ_LOWER", 936},
      {"GDDR4_CH1_GT_32B_RD_REQ_UPPER", 1100},
      {"GDDR4_CH1_GT_32B_RD_REQ_LOWER", 1096},
      {"GDDR5_CH1_GT_32B_RD_REQ_UPPER", 1260},
      {"GDDR5_CH1_GT_32B_RD_REQ_LOWER", 1256},
      {"GDDR0_CH0_GT_32B_WR_REQ_UPPER", 396},
      {"GDDR0_CH0_GT_32B_WR_REQ_LOWER", 392},
      {"GDDR1_CH0_GT_32B_WR_REQ_UPPER", 556},
      {"GDDR1_CH0_GT_32B_WR_REQ_LOWER", 552},
      {"GDDR2_CH0_GT_32B_WR_REQ_UPPER", 716},
      {"GDDR2_CH0_GT_32B_WR_REQ_LOWER", 712},
      {"GDDR3_CH0_GT_32B_WR_REQ_UPPER", 876},
      {"GDDR3_CH0_GT_32B_WR_REQ_LOWER", 872},
      {"GDDR4_CH0_GT_32B_WR_REQ_UPPER", 1036},
      {"GDDR4_CH0_GT_32B_WR_REQ_LOWER", 1032},
      {"GDDR5_CH0_GT_32B_WR_REQ_UPPER", 1196},
      {"GDDR5_CH0_GT_32B_WR_REQ_LOWER", 1192},
      {"GDDR0_CH1_GT_32B_WR_REQ_UPPER", 476},
      {"GDDR0_CH1_GT_32B_WR_REQ_LOWER", 472},
      {"GDDR1_CH1_GT_32B_WR_REQ_UPPER", 636},
      {"GDDR1_CH1_GT_32B_WR_REQ_LOWER", 632},
      {"GDDR2_CH1_GT_32B_WR_REQ_UPPER", 796},
      {"GDDR2_CH1_GT_32B_WR_REQ_LOWER", 792},
      {"GDDR3_CH1_GT_32B_WR_REQ_UPPER", 956},
      {"GDDR3_CH1_GT_32B_WR_REQ_LOWER", 952},
      {"GDDR4_CH1_GT_32B_WR_REQ_UPPER", 1116},
      {"GDDR4_CH1_GT_32B_WR_REQ_LOWER", 1112},
      {"GDDR5_CH1_GT_32B_WR_REQ_UPPER", 1276},
      {"GDDR5_CH1_GT_32B_WR_REQ_LOWER", 1272},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 412},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 408},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 572},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 568},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 732},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 728},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 892},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 888},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 1052},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 1048},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 1212},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 1208},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 492},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 488},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 652},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 648},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 812},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 808},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 972},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 968},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 1132},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 1128},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 1292},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 1288},
      {"GDDR0_CH0_SOC_32B_RD_REQ_UPPER", 428},
      {"GDDR0_CH0_SOC_32B_RD_REQ_LOWER", 424},
      {"GDDR1_CH0_SOC_32B_RD_REQ_UPPER", 588},
      {"GDDR1_CH0_SOC_32B_RD_REQ_LOWER", 584},
      {"GDDR2_CH0_SOC_32B_RD_REQ_UPPER", 748},
      {"GDDR2_CH0_SOC_32B_RD_REQ_LOWER", 744},
      {"GDDR3_CH0_SOC_32B_RD_REQ_UPPER", 908},
      {"GDDR3_CH0_SOC_32B_RD_REQ_LOWER", 904},
      {"GDDR4_CH0_SOC_32B_RD_REQ_UPPER", 1068},
      {"GDDR4_CH0_SOC_32B_RD_REQ_LOWER", 1064},
      {"GDDR5_CH0_SOC_32B_RD_REQ_UPPER", 1228},
      {"GDDR5_CH0_SOC_32B_RD_REQ_LOWER", 1224},
      {"GDDR0_CH1_SOC_32B_RD_REQ_UPPER", 508},
      {"GDDR0_CH1_SOC_32B_RD_REQ_LOWER", 504},
      {"GDDR1_CH1_SOC_32B_RD_REQ_UPPER", 668},
      {"GDDR1_CH1_SOC_32B_RD_REQ_LOWER", 664},
      {"GDDR2_CH1_SOC_32B_RD_REQ_UPPER", 828},
      {"GDDR2_CH1_SOC_32B_RD_REQ_LOWER", 824},
      {"GDDR3_CH1_SOC_32B_RD_REQ_UPPER", 988},
      {"GDDR3_CH1_SOC_32B_RD_REQ_LOWER", 984},
      {"GDDR4_CH1_SOC_32B_RD_REQ_UPPER", 1148},
      {"GDDR4_CH1_SOC_32B_RD_REQ_LOWER", 1144},
      {"GDDR5_CH1_SOC_32B_RD_REQ_UPPER", 1308},
      {"GDDR5_CH1_SOC_32B_RD_REQ_LOWER", 1304},
      {"GDDR0_CH0_SOC_32B_WR_REQ_UPPER", 444},
      {"GDDR0_CH0_SOC_32B_WR_REQ_LOWER", 440},
      {"GDDR1_CH0_SOC_32B_WR_REQ_UPPER", 604},
      {"GDDR1_CH0_SOC_32B_WR_REQ_LOWER", 600},
      {"GDDR2_CH0_SOC_32B_WR_REQ_UPPER", 764},
      {"GDDR2_CH0_SOC_32B_WR_REQ_LOWER", 760},
      {"GDDR3_CH0_SOC_32B_WR_REQ_UPPER", 924},
      {"GDDR3_CH0_SOC_32B_WR_REQ_LOWER", 920},
      {"GDDR4_CH0_SOC_32B_WR_REQ_UPPER", 1084},
      {"GDDR4_CH0_SOC_32B_WR_REQ_LOWER", 1080},
      {"GDDR5_CH0_SOC_32B_WR_REQ_UPPER", 1244},
      {"GDDR5_CH0_SOC_32B_WR_REQ_LOWER", 1240},
      {"GDDR0_CH1_SOC_32B_WR_REQ_UPPER", 524},
      {"GDDR0_CH1_SOC_32B_WR_REQ_LOWER", 520},
      {"GDDR1_CH1_SOC_32B_WR_REQ_UPPER", 684},
      {"GDDR1_CH1_SOC_32B_WR_REQ_LOWER", 680},
      {"GDDR2_CH1_SOC_32B_WR_REQ_UPPER", 844},
      {"GDDR2_CH1_SOC_32B_WR_REQ_LOWER", 840},
      {"GDDR3_CH1_SOC_32B_WR_REQ_UPPER", 1004},
      {"GDDR3_CH1_SOC_32B_WR_REQ_LOWER", 1000},
      {"GDDR4_CH1_SOC_32B_WR_REQ_UPPER", 1164},
      {"GDDR4_CH1_SOC_32B_WR_REQ_LOWER", 1160},
      {"GDDR5_CH1_SOC_32B_WR_REQ_UPPER", 1324},
      {"GDDR5_CH1_SOC_32B_WR_REQ_LOWER", 1320},
      {"GDDR0_CH0_GT_64B_RD_REQ_UPPER", 388},
      {"GDDR0_CH0_GT_64B_RD_REQ_LOWER", 384},
      {"GDDR1_CH0_GT_64B_RD_REQ_UPPER", 548},
      {"GDDR1_CH0_GT_64B_RD_REQ_LOWER", 544},
      {"GDDR2_CH0_GT_64B_RD_REQ_UPPER", 708},
      {"GDDR2_CH0_GT_64B_RD_REQ_LOWER", 704},
      {"GDDR3_CH0_GT_64B_RD_REQ_UPPER", 868},
      {"GDDR3_CH0_GT_64B_RD_REQ_LOWER", 864},
      {"GDDR4_CH0_GT_64B_RD_REQ_UPPER", 1028},
      {"GDDR4_CH0_GT_64B_RD_REQ_LOWER", 1024},
      {"GDDR5_CH0_GT_64B_RD_REQ_UPPER", 1188},
      {"GDDR5_CH0_GT_64B_RD_REQ_LOWER", 1184},
      {"GDDR0_CH1_GT_64B_RD_REQ_UPPER", 468},
      {"GDDR0_CH1_GT_64B_RD_REQ_LOWER", 464},
      {"GDDR1_CH1_GT_64B_RD_REQ_UPPER", 628},
      {"GDDR1_CH1_GT_64B_RD_REQ_LOWER", 624},
      {"GDDR2_CH1_GT_64B_RD_REQ_UPPER", 788},
      {"GDDR2_CH1_GT_64B_RD_REQ_LOWER", 784},
      {"GDDR3_CH1_GT_64B_RD_REQ_UPPER", 948},
      {"GDDR3_CH1_GT_64B_RD_REQ_LOWER", 944},
      {"GDDR4_CH1_GT_64B_RD_REQ_UPPER", 1108},
      {"GDDR4_CH1_GT_64B_RD_REQ_LOWER", 1104},
      {"GDDR5_CH1_GT_64B_RD_REQ_UPPER", 1268},
      {"GDDR5_CH1_GT_64B_RD_REQ_LOWER", 1264},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 420},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 416},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 580},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 576},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 740},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 736},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 900},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 896},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 1060},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 1056},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 1220},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 1216},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 500},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 496},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 660},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 656},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 820},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 816},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 980},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 976},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 1140},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 1136},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 1300},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 1296},
      {"GDDR0_CH0_SOC_64B_RD_REQ_UPPER", 436},
      {"GDDR0_CH0_SOC_64B_RD_REQ_LOWER", 432},
      {"GDDR1_CH0_SOC_64B_RD_REQ_UPPER", 596},
      {"GDDR1_CH0_SOC_64B_RD_REQ_LOWER", 592},
      {"GDDR2_CH0_SOC_64B_RD_REQ_UPPER", 756},
      {"GDDR2_CH0_SOC_64B_RD_REQ_LOWER", 752},
      {"GDDR3_CH0_SOC_64B_RD_REQ_UPPER", 916},
      {"GDDR3_CH0_SOC_64B_RD_REQ_LOWER", 912},
      {"GDDR4_CH0_SOC_64B_RD_REQ_UPPER", 1076},
      {"GDDR4_CH0_SOC_64B_RD_REQ_LOWER", 1072},
      {"GDDR5_CH0_SOC_64B_RD_REQ_UPPER", 1236},
      {"GDDR5_CH0_SOC_64B_RD_REQ_LOWER", 1232},
      {"GDDR0_CH1_SOC_64B_RD_REQ_UPPER", 516},
      {"GDDR0_CH1_SOC_64B_RD_REQ_LOWER", 512},
      {"GDDR1_CH1_SOC_64B_RD_REQ_UPPER", 676},
      {"GDDR1_CH1_SOC_64B_RD_REQ_LOWER", 672},
      {"GDDR2_CH1_SOC_64B_RD_REQ_UPPER", 836},
      {"GDDR2_CH1_SOC_64B_RD_REQ_LOWER", 832},
      {"GDDR3_CH1_SOC_64B_RD_REQ_UPPER", 996},
      {"GDDR3_CH1_SOC_64B_RD_REQ_LOWER", 992},
      {"GDDR4_CH1_SOC_64B_RD_REQ_UPPER", 1156},
      {"GDDR4_CH1_SOC_64B_RD_REQ_LOWER", 1152},
      {"GDDR5_CH1_SOC_64B_RD_REQ_UPPER", 1316},
      {"GDDR5_CH1_SOC_64B_RD_REQ_LOWER", 1312},
      {"GDDR0_CH0_SOC_64B_WR_REQ_UPPER", 452},
      {"GDDR0_CH0_SOC_64B_WR_REQ_LOWER", 448},
      {"GDDR1_CH0_SOC_64B_WR_REQ_UPPER", 612},
      {"GDDR1_CH0_SOC_64B_WR_REQ_LOWER", 608},
      {"GDDR2_CH0_SOC_64B_WR_REQ_UPPER", 772},
      {"GDDR2_CH0_SOC_64B_WR_REQ_LOWER", 768},
      {"GDDR3_CH0_SOC_64B_WR_REQ_UPPER", 932},
      {"GDDR3_CH0_SOC_64B_WR_REQ_LOWER", 928},
      {"GDDR4_CH0_SOC_64B_WR_REQ_UPPER", 1092},
      {"GDDR4_CH0_SOC_64B_WR_REQ_LOWER", 1088},
      {"GDDR5_CH0_SOC_64B_WR_REQ_UPPER", 1252},
      {"GDDR5_CH0_SOC_64B_WR_REQ_LOWER", 1248},
      {"GDDR0_CH1_SOC_64B_WR_REQ_UPPER", 532},
      {"GDDR0_CH1_SOC_64B_WR_REQ_LOWER", 528},
      {"GDDR1_CH1_SOC_64B_WR_REQ_UPPER", 692},
      {"GDDR1_CH1_SOC_64B_WR_REQ_LOWER", 688},
      {"GDDR2_CH1_SOC_64B_WR_REQ_UPPER", 852},
      {"GDDR2_CH1_SOC_64B_WR_REQ_LOWER", 848},
      {"GDDR3_CH1_SOC_64B_WR_REQ_UPPER", 1012},
      {"GDDR3_CH1_SOC_64B_WR_REQ_LOWER", 1008},
      {"GDDR4_CH1_SOC_64B_WR_REQ_UPPER", 1172},
      {"GDDR4_CH1_SOC_64B_WR_REQ_LOWER", 1168},
      {"GDDR5_CH1_SOC_64B_WR_REQ_UPPER", 1332},
      {"GDDR5_CH1_SOC_64B_WR_REQ_LOWER", 1328},
      {"GDDR0_CH0_GT_64B_WR_REQ_UPPER", 404},
      {"GDDR0_CH0_GT_64B_WR_REQ_LOWER", 400},
      {"GDDR1_CH0_GT_64B_WR_REQ_UPPER", 564},
      {"GDDR1_CH0_GT_64B_WR_REQ_LOWER", 560},
      {"GDDR2_CH0_GT_64B_WR_REQ_UPPER", 724},
      {"GDDR2_CH0_GT_64B_WR_REQ_LOWER", 720},
      {"GDDR3_CH0_GT_64B_WR_REQ_UPPER", 884},
      {"GDDR3_CH0_GT_64B_WR_REQ_LOWER", 880},
      {"GDDR4_CH0_GT_64B_WR_REQ_UPPER", 1044},
      {"GDDR4_CH0_GT_64B_WR_REQ_LOWER", 1040},
      {"GDDR5_CH0_GT_64B_WR_REQ_UPPER", 1204},
      {"GDDR5_CH0_GT_64B_WR_REQ_LOWER", 1200},
      {"GDDR0_CH1_GT_64B_WR_REQ_UPPER", 484},
      {"GDDR0_CH1_GT_64B_WR_REQ_LOWER", 480},
      {"GDDR1_CH1_GT_64B_WR_REQ_UPPER", 644},
      {"GDDR1_CH1_GT_64B_WR_REQ_LOWER", 640},
      {"GDDR2_CH1_GT_64B_WR_REQ_UPPER", 804},
      {"GDDR2_CH1_GT_64B_WR_REQ_LOWER", 800},
      {"GDDR3_CH1_GT_64B_WR_REQ_UPPER", 964},
      {"GDDR3_CH1_GT_64B_WR_REQ_LOWER", 960},
      {"GDDR4_CH1_GT_64B_WR_REQ_UPPER", 1124},
      {"GDDR4_CH1_GT_64B_WR_REQ_LOWER", 1120},
      {"GDDR5_CH1_GT_64B_WR_REQ_UPPER", 1284},
      {"GDDR5_CH1_GT_64B_WR_REQ_LOWER", 1280}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciProperties(zes_pci_properties_t *pProperties) {
    pProperties->haveBandwidthCounters = true;
    pProperties->havePacketCounters = true;
    pProperties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}

static ze_result_t getPciStatsValues(zes_pci_stats_t *pStats, std::map<std::string, uint64_t> &keyOffsetMap, const std::string &telemNodeDir) {
    uint32_t rxCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_lsb", 0, rxCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_msb", 0, rxCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxCounter = packInto64Bit(rxCounterMsb, rxCounterLsb);

    uint32_t txCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_lsb", 0, txCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_msb", 0, txCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txCounter = packInto64Bit(txCounterMsb, txCounterLsb);

    uint32_t rxPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_lsb", 0, rxPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_msb", 0, rxPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxPacketCounter = packInto64Bit(rxPacketCounterMsb, rxPacketCounterLsb);

    uint32_t txPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_lsb", 0, txPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_msb", 0, txPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txPacketCounter = packInto64Bit(txPacketCounterMsb, txPacketCounterLsb);

    uint32_t timeStampLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 0, timeStampLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t timeStampMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 0, timeStampMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t timeStamp = packInto64Bit(timeStampMsb, timeStampLsb);

    pStats->speed.gen = -1;
    pStats->speed.width = -1;
    pStats->speed.maxBandwidth = -1;
    pStats->replayCounter = 0;
    pStats->rxCounter = rxCounter;
    pStats->txCounter = txCounter;
    pStats->packetCounter = rxPacketCounter + txPacketCounter;
    pStats->timestamp = timeStamp * milliSecsToMicroSecs;

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) {
    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    for (auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        result = getPciStatsValues(pStats, keyOffsetMapIterator->second, telemNodeDir);
        if (result == ZE_RESULT_SUCCESS) {
            break;
        }
    }

    return result;
};

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    telemOffset = 0;

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    uint32_t gpuMaxTemperature = 0;
    std::string key("SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, gpuMaxTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    *pTemperature = static_cast<double>(gpuMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isMemoryMaxTemperatureSupported() {
    return true;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    telemOffset = 0;

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    uint32_t memoryMaxTemperature = 0;
    std::string key("VRAM_TEMPERATURE_0_2_0_GTTMMADR");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, memoryMaxTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    memoryMaxTemperature &= 0xFFu; // Extract least significant 8 bits
    *pTemperature = static_cast<double>(memoryMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    double gpuMaxTemperature = 0;
    ze_result_t result = this->getGpuMaxTemperature(pLinuxSysmanImp, &gpuMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double memoryMaxTemperature = 0;
    result = this->getMemoryMaxTemperature(pLinuxSysmanImp, &memoryMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *pTemperature = std::max(gpuMaxTemperature, memoryMaxTemperature);
    return result;
}

static ze_result_t getMemoryMaxBandwidth(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap,
                                         zes_mem_bandwidth_t *pBandwidth) {
    uint32_t maxBandwidth = 0;
    std::string key = "VRAM_BANDWIDTH";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key], key, 0, maxBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    maxBandwidth = maxBandwidth >> 16;
    pBandwidth->maxBandwidth = static_cast<uint64_t>(maxBandwidth) * megaBytesToBytes * 100;

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getMemoryBandwidthTimestamp(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap,
                                               zes_mem_bandwidth_t *pBandwidth) {
    uint32_t timeStampH = 0;
    uint32_t timeStampL = 0;
    pBandwidth->timestamp = 0;

    std::string key = "GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key], key, 0, timeStampH)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    key = "GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key], key, 0, timeStampL)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    pBandwidth->timestamp = packInto64Bit(timeStampH, timeStampL) * milliSecsToMicroSecs;

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getCounterValues(const std::vector<std::pair<const std::string, const std::string>> &registerList, const std::string &keyPrefix,
                                    const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap, uint64_t &totalCounter) {
    for (const auto &regPair : registerList) {
        uint32_t regL = 0;
        uint32_t regH = 0;

        std::string keyL = keyPrefix + regPair.first;
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[keyL], keyL, 0, regL)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        std::string keyH = keyPrefix + regPair.second;
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[keyH], keyH, 0, regH)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        totalCounter += packInto64Bit(regH, regL);
    }

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getMemoryBandwidthCounterValues(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap,
                                                   const uint32_t &supportedMsu, zes_mem_bandwidth_t *pBandwidth) {
    const std::vector<std::pair<const std::string, const std::string>> readBwRegisterList{
        {"_CH0_GT_32B_RD_REQ_LOWER", "_CH0_GT_32B_RD_REQ_UPPER"},
        {"_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", "_CH0_DISPLAYVC0_32B_RD_REQ_UPPER"},
        {"_CH0_SOC_32B_RD_REQ_LOWER", "_CH0_SOC_32B_RD_REQ_UPPER"},
        {"_CH1_GT_32B_RD_REQ_LOWER", "_CH1_GT_32B_RD_REQ_UPPER"},
        {"_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", "_CH1_DISPLAYVC0_32B_RD_REQ_UPPER"},
        {"_CH1_SOC_32B_RD_REQ_LOWER", "_CH1_SOC_32B_RD_REQ_UPPER"},
        {"_CH0_GT_64B_RD_REQ_LOWER", "_CH0_GT_64B_RD_REQ_UPPER"},
        {"_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", "_CH0_DISPLAYVC0_64B_RD_REQ_UPPER"},
        {"_CH0_SOC_64B_RD_REQ_LOWER", "_CH0_SOC_64B_RD_REQ_UPPER"},
        {"_CH1_GT_64B_RD_REQ_LOWER", "_CH1_GT_64B_RD_REQ_UPPER"},
        {"_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", "_CH1_DISPLAYVC0_64B_RD_REQ_UPPER"},
        {"_CH1_SOC_64B_RD_REQ_LOWER", "_CH1_SOC_64B_RD_REQ_UPPER"}};

    const std::vector<std::pair<const std::string, const std::string>> writeBwRegisterList{
        {"_CH0_GT_32B_WR_REQ_LOWER", "_CH0_GT_32B_WR_REQ_UPPER"},
        {"_CH0_SOC_32B_WR_REQ_LOWER", "_CH0_SOC_32B_WR_REQ_UPPER"},
        {"_CH1_GT_32B_WR_REQ_LOWER", "_CH1_GT_32B_WR_REQ_UPPER"},
        {"_CH1_SOC_32B_WR_REQ_LOWER", "_CH1_SOC_32B_WR_REQ_UPPER"},
        {"_CH0_GT_64B_WR_REQ_LOWER", "_CH0_GT_64B_WR_REQ_UPPER"},
        {"_CH0_SOC_64B_WR_REQ_LOWER", "_CH0_SOC_64B_WR_REQ_UPPER"},
        {"_CH1_GT_64B_WR_REQ_LOWER", "_CH1_GT_64B_WR_REQ_UPPER"},
        {"_CH1_SOC_64B_WR_REQ_LOWER", "_CH1_SOC_64B_WR_REQ_UPPER"}};

    constexpr uint64_t maxSupportedMsu = 8;
    constexpr uint64_t transactionSize = 32;

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;

    for (uint32_t i = 0; i < maxSupportedMsu; i++) {
        if (supportedMsu & (1 << i)) {
            std::ostringstream keyStream;
            keyStream << "GDDR" << i;
            std::string keyPrefix = keyStream.str();

            if (ZE_RESULT_SUCCESS != getCounterValues(readBwRegisterList, keyPrefix, keyOffsetMap, keyTelemInfoMap, pBandwidth->readCounter)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }

            if (ZE_RESULT_SUCCESS != getCounterValues(writeBwRegisterList, keyPrefix, keyOffsetMap, keyTelemInfoMap, pBandwidth->writeCounter)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
        }
    }

    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize) / microFactor;
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize) / microFactor;

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    std::unordered_map<std::string, std::string> keyTelemInfoMap;

    // Iterate through all the TelemNodes to find both OOBMSM and PUNIT guids along with their keyOffsetMap
    for (const auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        const auto &tempKeyOffsetMap = keyOffsetMapIterator->second;
        for (auto it = tempKeyOffsetMap.begin(); it != tempKeyOffsetMap.end(); it++) {
            keyOffsetMap[it->first] = it->second;
            keyTelemInfoMap[it->first] = telemNodeDir;
        }
    }

    if (keyOffsetMap.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // Get Memory Subsystem Bitmask
    uint32_t supportedMsu = 0;
    std::string key = "MSU_BITMASK";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key], key, 0, supportedMsu)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Read and Write Counter Values
    if (ZE_RESULT_SUCCESS != getMemoryBandwidthCounterValues(keyOffsetMap, keyTelemInfoMap, supportedMsu, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Timestamp Values
    if (ZE_RESULT_SUCCESS != getMemoryBandwidthTimestamp(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Max Bandwidth
    if (ZE_RESULT_SUCCESS != getMemoryMaxBandwidth(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
