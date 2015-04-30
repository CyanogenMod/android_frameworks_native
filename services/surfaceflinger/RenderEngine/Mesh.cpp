/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Mesh.h"

#include <utils/Log.h>

namespace android {

Mesh::Mesh(Primitive primitive, size_t vertexCount, size_t vertexSize, size_t texCoordSize)
    : mVertexCount(vertexCount), mVertexSize(vertexSize), mTexCoordsSize(texCoordSize),
      mPrimitive(primitive)
{
    if (vertexCount == 0) {
        mVertices = new float[1];
        mVertices[0] = 0.0f;
        mStride = 0;
        return;
    }

    size_t stride = vertexSize + texCoordSize;
    size_t remainder = (stride * vertexCount) / vertexCount;
    // Since all of the input parameters are unsigned, if stride is less than
    // either vertexSize or texCoordSize, it must have overflowed. remainder
    // will be equal to stride as long as stride * vertexCount doesn't overflow.
    if ((stride < vertexSize) || (remainder != stride)) {
        ALOGE("Overflow in Mesh(..., %zu, %zu, %zu)", vertexCount, vertexSize,
                texCoordSize);
        mVertices = new float[1];
        mVertices[0] = 0.0f;
        mVertexCount = 0;
        mVertexSize = 0;
        mTexCoordsSize = 0;
        mStride = 0;
        return;
    }

    mVertices = new float[stride * vertexCount];
    mStride = stride;
}

Mesh::~Mesh() {
    delete [] mVertices;
}

Mesh::Primitive Mesh::getPrimitive() const {
    return mPrimitive;
}


float const* Mesh::getPositions() const {
    return mVertices;
}
float* Mesh::getPositions() {
    return mVertices;
}

float const* Mesh::getTexCoords() const {
    return mVertices + mVertexSize;
}
float* Mesh::getTexCoords() {
    return mVertices + mVertexSize;
}


size_t Mesh::getVertexCount() const {
    return mVertexCount;
}

size_t Mesh::getVertexSize() const {
    return mVertexSize;
}

size_t Mesh::getTexCoordsSize() const {
    return mTexCoordsSize;
}

size_t Mesh::getByteStride() const {
    return mStride*sizeof(float);
}

size_t Mesh::getStride() const {
    return mStride;
}

} /* namespace android */
