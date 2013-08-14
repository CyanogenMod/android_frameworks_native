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

#ifndef SF_RENDER_ENGINE_MESH_H
#define SF_RENDER_ENGINE_MESH_H

#include <stdint.h>

namespace android {

class Mesh {
public:
    enum Primitive {
        TRIANGLES       = 0x0004,
        TRIANGLE_STRIP  = 0x0005,
        TRIANGLE_FAN    = 0x0006
    };

    Mesh(Primitive primitive, size_t vertexCount, size_t vertexSize, size_t texCoordsSize = 0);
    ~Mesh();

    float const* operator[](size_t index) const;
    float* operator[](size_t index);


    Primitive getPrimitive() const;

    float const* getVertices() const;
    float* getVertices();

    float const* getTexCoords() const;
    float* getTexCoords();

    size_t getVertexCount() const;
    size_t getVertexSize() const;
    size_t getTexCoordsSize() const;

    size_t getByteStride() const;
    size_t getStride() const;

private:
    float* mVertices;
    size_t mVertexCount;
    size_t mVertexSize;
    size_t mTexCoordsSize;
    size_t mStride;
    Primitive mPrimitive;
};


} /* namespace android */
#endif /* SF_RENDER_ENGINE_MESH_H */
