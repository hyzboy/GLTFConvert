#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include "math/TRS.h"

struct NodeTransform
{
    enum class Type : uint8_t { None, Matrix, TRS };

    Type type { Type::None };

    union
    {
        glm::mat4 m;
        TRS trs;
    };

    // Constructors
    NodeTransform() noexcept;
    explicit NodeTransform(const glm::mat4 &mat) noexcept;
    explicit NodeTransform(const TRS &t) noexcept;

    // Rule of 5
    NodeTransform(const NodeTransform &rhs) noexcept;
    NodeTransform &operator=(const NodeTransform &rhs) noexcept;
    NodeTransform(NodeTransform &&rhs) noexcept;
    NodeTransform &operator=(NodeTransform &&rhs) noexcept;

    // State queries
    bool isNone()   const noexcept { return type == Type::None; }
    bool isMatrix() const noexcept { return type == Type::Matrix; }
    bool isTRS()    const noexcept { return type == Type::TRS; }

    // Mutators
    void setNone() noexcept;
    void setMatrix(const glm::mat4 &mat) noexcept;
    void setTRS(const TRS &t) noexcept;

    // Matrix extraction
    glm::mat4 rawMat4() const;

    // Coordinate system helpers (Y-up <-> Z-up)
    glm::mat4 toZUpMat4() const;               // Y-up local -> Z-up matrix
    void convertInPlaceYUpToZUp();              // Modify internal representation
};
