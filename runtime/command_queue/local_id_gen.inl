/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_queue/local_id_gen.h"

#include <array>

namespace OCLRT {

template <typename Vec, int simd>
inline void generateLocalIDsSimd(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup,
                                 const std::array<uint8_t, 3> &dimensionsOrder) {
    const int passes = simd / Vec::numChannels;
    int pass = 0;

    uint32_t xDimNum = dimensionsOrder[0];
    uint32_t yDimNum = dimensionsOrder[1];
    uint32_t zDimNum = dimensionsOrder[2];

    const Vec vLwsX(localWorkgroupSize[xDimNum]);
    const Vec vLwsY(localWorkgroupSize[yDimNum]);

    auto zero = Vec::zero();
    auto one = Vec::one();

    const auto threadSkipSize = (simd == 32 ? 32 : 16) * sizeof(uint16_t);
    Vec vSimdX(simd);
    Vec vSimdY = zero;
    Vec vSimdZ = zero;

    Vec xWrap;
    Vec yWrap;
    // We need to convert simd into appropriate delta adders
    do {
        xWrap = vSimdX >= vLwsX;

        // xWrap ? lwsX : 0;
        auto deltaX = blend(vLwsX, zero, xWrap);

        // x -= xWrap ? lwsX : 0;
        vSimdX -= deltaX;

        // xWrap ? 1 : 0;
        auto deltaY = blend(one, zero, xWrap);

        // y += xWrap ? 1 : 0;
        vSimdY += deltaY;

        yWrap = vSimdY >= vLwsY;

        // yWrap ? lwsY : 0;
        auto deltaY2 = blend(vLwsY, zero, yWrap);

        // y -= yWrap ? lwsY : 0;
        vSimdY -= deltaY2;

        // yWrap ? 1 : 0;
        auto deltaZ = blend(one, zero, yWrap);

        // z += yWrap ? 1 : 0;
        vSimdZ += deltaZ;
    } while (xWrap || yWrap);

    // Loop for each of the passes
    do {
        auto buffer = b;

        Vec x(&initialLocalID[pass * Vec::numChannels]);
        Vec y = zero;
        Vec z = zero;

        // Convert the initial SIMD lanes to localIDs
        do {
            xWrap = x >= vLwsX;

            // xWrap ? lwsX : 0;
            auto deltaX = blend(vLwsX, zero, xWrap);

            // x -= xWrap ? lwsX : 0;
            x -= deltaX;

            // xWrap ? 1 : 0;
            auto deltaY = blend(one, zero, xWrap);

            // y += xWrap ? 1 : 0;
            y += deltaY;

            yWrap = y >= vLwsY;

            // yWrap ? lwsY : 0;
            auto deltaY2 = blend(vLwsY, zero, yWrap);

            // y -= yWrap ? lwsY : 0;
            y -= deltaY2;

            // yWrap ? 1 : 0;
            auto deltaZ = blend(one, zero, yWrap);

            // z += yWrap ? 1 : 0;
            z += deltaZ;
        } while (xWrap);

        for (size_t i = 0; i < threadsPerWorkGroup; ++i) {
            x.store(ptrOffset(buffer, xDimNum * threadSkipSize));
            y.store(ptrOffset(buffer, yDimNum * threadSkipSize));
            z.store(ptrOffset(buffer, zDimNum * threadSkipSize));

            x += vSimdX;
            y += vSimdY;
            z += vSimdZ;

            xWrap = x >= vLwsX;

            // xWrap ? lwsX : 0;
            auto deltaX = blend(vLwsX, zero, xWrap);

            // x -= xWrap ? lwsX : 0;
            x -= deltaX;

            // xWrap ? 1 : 0;
            auto deltaY = blend(one, zero, xWrap);

            // y += xWrap ? 1 : 0;
            y += deltaY;

            yWrap = y >= vLwsY;

            // yWrap ? lwsY : 0;
            auto deltaY2 = blend(vLwsY, zero, yWrap);

            // y -= yWrap ? lwsY : 0;
            y -= deltaY2;

            // yWrap ? 1 : 0;
            auto deltaZ = blend(one, zero, yWrap);

            // z += yWrap ? 1 : 0;
            z += deltaZ;

            buffer = ptrOffset(buffer, 3 * threadSkipSize);
        }

        // Adjust buffer for next pass
        b = ptrOffset(b, Vec::numChannels * sizeof(uint16_t));

    } while (++pass < passes);
}
} // namespace OCLRT
