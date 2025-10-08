#pragma once

#include <cstdint>
#include <optional>
#include <string>

// glTF sampler states (mirrors glTF spec subset)
struct GLTFSampler
{
    // Wrap modes (33071=CLAMP_TO_EDGE, 33648=MIRRORED_REPEAT, 10497=REPEAT)
    int wrapS=10497; // REPEAT
    int wrapT=10497; // REPEAT

    // Mag / Min filters (9728=NEAREST, 9729=LINEAR, others mipmap variants)
    std::optional<int> magFilter; // glTF magFilter
    std::optional<int> minFilter; // glTF minFilter

    std::string name; // optional name
};
