#include "collision/CollisionPrimitives.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kEpsilon = 1e-5f;

float length2(const glm::vec3& v) noexcept
{
    return glm::dot(v, v);
}

glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback) noexcept
{
    const float len2 = length2(v);
    if (len2 <= kEpsilon * kEpsilon) {
        return fallback;
    }
    return v * (1.0f / std::sqrt(len2));
}

glm::vec3 closestPointAABB(const glm::vec3& p, const AABB& box) noexcept
{
    return glm::clamp(p, box.min, box.max);
}

glm::vec3 closestPointOBB(const glm::vec3& p, const OBB& box) noexcept
{
    glm::vec3 q = box.center;
    const glm::vec3 d = p - box.center;
    for (int i = 0; i < 3; ++i) {
        const float dist = glm::dot(d, box.axis[static_cast<std::size_t>(i)]);
        const float clamped = std::clamp(dist, -box.halfExtents[i], box.halfExtents[i]);
        q += box.axis[static_cast<std::size_t>(i)] * clamped;
    }
    return q;
}

Contact capsuleBoxContactLocal(const glm::vec3& a, const glm::vec3& b, float radius,
                               const glm::vec3& boxHalfExtents) noexcept
{
    Contact best;
    float bestDistance2 = std::numeric_limits<float>::max();

    auto distance2At = [&](float t, glm::vec3* outPoint, glm::vec3* outClosest) {
        const glm::vec3 p = a + (b - a) * t;
        const glm::vec3 q = glm::clamp(p, -boxHalfExtents, boxHalfExtents);
        if (outPoint != nullptr) {
            *outPoint = p;
        }
        if (outClosest != nullptr) {
            *outClosest = q;
        }
        return length2(p - q);
    };

    // Squared distance from a segment to an AABB is convex over the segment parameter.
    // Ternary refinement gives a robust nearest point without enumerating box features.
    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < 32; ++i) {
        const float m1 = lo + (hi - lo) / 3.0f;
        const float m2 = hi - (hi - lo) / 3.0f;
        if (distance2At(m1, nullptr, nullptr) < distance2At(m2, nullptr, nullptr)) {
            hi = m2;
        } else {
            lo = m1;
        }
    }

    glm::vec3 spinePoint(0.0f);
    glm::vec3 closest(0.0f);
    bestDistance2 = distance2At(0.5f * (lo + hi), &spinePoint, &closest);
    best.point = closest;
    best.normal = safeNormalize(spinePoint - closest, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 endpointPoint;
    glm::vec3 endpointClosest;
    const float endpointA = distance2At(0.0f, &endpointPoint, &endpointClosest);
    if (endpointA < bestDistance2) {
        bestDistance2 = endpointA;
        best.point = endpointClosest;
        best.normal = safeNormalize(endpointPoint - endpointClosest, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    const float endpointB = distance2At(1.0f, &endpointPoint, &endpointClosest);
    if (endpointB < bestDistance2) {
        bestDistance2 = endpointB;
        best.point = endpointClosest;
        best.normal = safeNormalize(endpointPoint - endpointClosest, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    const float radius2 = radius * radius;
    if (bestDistance2 > radius2) {
        return {};
    }

    best.hit = true;
    const float distance = std::sqrt(std::max(bestDistance2, 0.0f));
    best.penetration = radius - distance;

    if (distance <= kEpsilon) {
        const glm::vec3 mid = 0.5f * (a + b);
        const glm::vec3 absMid = glm::abs(mid);
        const glm::vec3 margin = boxHalfExtents - absMid;
        if (margin.x <= margin.y && margin.x <= margin.z) {
            best.normal = glm::vec3(mid.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
            best.penetration = radius + std::max(margin.x, 0.0f);
        } else if (margin.y <= margin.z) {
            best.normal = glm::vec3(0.0f, mid.y < 0.0f ? -1.0f : 1.0f, 0.0f);
            best.penetration = radius + std::max(margin.y, 0.0f);
        } else {
            best.normal = glm::vec3(0.0f, 0.0f, mid.z < 0.0f ? -1.0f : 1.0f);
            best.penetration = radius + std::max(margin.z, 0.0f);
        }
    }

    best.penetration = std::max(best.penetration, 0.0f);
    return best;
}

bool testOBBAxis(const OBB& a, const OBB& b, const glm::vec3& axis) noexcept
{
    const float axisLen2 = length2(axis);
    if (axisLen2 <= kEpsilon * kEpsilon) {
        return true;
    }

    const glm::vec3 n = axis * (1.0f / std::sqrt(axisLen2));
    const float centerDistance = std::abs(glm::dot(b.center - a.center, n));

    float ra = 0.0f;
    float rb = 0.0f;
    for (int i = 0; i < 3; ++i) {
        ra += a.halfExtents[i] * std::abs(glm::dot(a.axis[static_cast<std::size_t>(i)], n));
        rb += b.halfExtents[i] * std::abs(glm::dot(b.axis[static_cast<std::size_t>(i)], n));
    }
    return centerDistance <= ra + rb + kEpsilon;
}

} // namespace

OBB OBB::fromAABB(const AABB& box) noexcept
{
    OBB out;
    out.center = 0.5f * (box.min + box.max);
    out.halfExtents = 0.5f * glm::max(box.max - box.min, glm::vec3(0.0f));
    return out;
}

OBB OBB::fromLocalAABB(const glm::vec3& localMin, const glm::vec3& localMax,
                       const glm::mat4& transform) noexcept
{
    OBB out;
    const glm::vec3 localCenter = 0.5f * (localMin + localMax);
    const glm::vec3 localHalfExtents = 0.5f * glm::max(localMax - localMin, glm::vec3(0.0f));
    out.center = glm::vec3(transform * glm::vec4(localCenter, 1.0f));

    for (int i = 0; i < 3; ++i) {
        const glm::vec3 column = glm::vec3(transform[static_cast<std::size_t>(i)]);
        const float scale = std::sqrt(length2(column));
        out.axis[static_cast<std::size_t>(i)] = safeNormalize(column, i == 0 ? glm::vec3(1.0f, 0.0f, 0.0f)
            : (i == 1 ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f)));
        out.halfExtents[i] = localHalfExtents[i] * scale;
    }
    return out;
}

std::array<glm::vec3, 8> OBB::corners() const noexcept
{
    std::array<glm::vec3, 8> out{};
    int index = 0;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                out[static_cast<std::size_t>(index++)] =
                    center
                    + axis[0] * halfExtents.x * static_cast<float>(x)
                    + axis[1] * halfExtents.y * static_cast<float>(y)
                    + axis[2] * halfExtents.z * static_cast<float>(z);
            }
        }
    }
    return out;
}

bool intersect(const OBB& a, const OBB& b) noexcept
{
    for (int i = 0; i < 3; ++i) {
        if (!testOBBAxis(a, b, a.axis[static_cast<std::size_t>(i)])) return false;
        if (!testOBBAxis(a, b, b.axis[static_cast<std::size_t>(i)])) return false;
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const glm::vec3 axis = glm::cross(a.axis[static_cast<std::size_t>(i)],
                                             b.axis[static_cast<std::size_t>(j)]);
            if (!testOBBAxis(a, b, axis)) return false;
        }
    }
    return true;
}

Contact capsuleAABBContact(const Capsule& capsule, const AABB& box) noexcept
{
    if (!box.valid() || capsule.radius <= 0.0f) {
        return {};
    }

    const OBB obb = OBB::fromAABB(box);
    return capsuleOBBContact(capsule, obb);
}

Contact capsuleOBBContact(const Capsule& capsule, const OBB& box) noexcept
{
    if (capsule.radius <= 0.0f) {
        return {};
    }

    const auto toLocal = [&](const glm::vec3& p) {
        const glm::vec3 d = p - box.center;
        return glm::vec3(
            glm::dot(d, box.axis[0]),
            glm::dot(d, box.axis[1]),
            glm::dot(d, box.axis[2]));
    };
    const auto dirToWorld = [&](const glm::vec3& v) {
        return box.axis[0] * v.x + box.axis[1] * v.y + box.axis[2] * v.z;
    };
    const auto pointToWorld = [&](const glm::vec3& p) {
        return box.center + dirToWorld(p);
    };

    Contact local = capsuleBoxContactLocal(
        toLocal(capsule.segmentA),
        toLocal(capsule.segmentB),
        capsule.radius,
        glm::max(box.halfExtents, glm::vec3(0.0f)));
    if (!local.hit) {
        return {};
    }

    local.normal = safeNormalize(dirToWorld(local.normal), glm::vec3(0.0f, 1.0f, 0.0f));
    local.point = pointToWorld(local.point);
    return local;
}

glm::vec3 slideVector(const glm::vec3& velocity, const glm::vec3& normal) noexcept
{
    const glm::vec3 n = safeNormalize(normal, glm::vec3(0.0f, 1.0f, 0.0f));
    const float intoSurface = glm::dot(velocity, n);
    if (intoSurface >= 0.0f) {
        return velocity;
    }
    return velocity - n * intoSurface;
}
