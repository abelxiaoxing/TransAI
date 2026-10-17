#ifndef VISION_MODEL_CAPABILITY_H
#define VISION_MODEL_CAPABILITY_H

#include <QString>

// Heuristic vision-capability detection for chat model ids.
//
// Used for two purposes only:
//  1. Settings UI: show a non-blocking hint when the effective screenshot
//     model does not look vision-capable.
//  2. Routing/UX: pick a sensible default error message.
// It is NOT a hard gate — the authoritative answer comes from the endpoint
// itself (a 400 about image input is rewritten into a guided error by
// VisionTranslationService).
namespace VisionModelCapability {

// Well-known vision markers matched case-insensitively as substrings of the
// model id. Ordered roughly by vendor; extend freely.
inline const char* kVisionMarkers[] = {
    // OpenAI
    "gpt-4o", "gpt-4.1", "gpt-5", "o4-", "chatgpt-4o",
    // Generic / relay naming
    "vision", "-vl", "vl-",
    // Anthropic / Google / xAI
    "claude", "gemini", "grok-4",
    // Qwen / GLM / DeepSeek / others
    "qwen-vl", "qvq", "glm-4v", "glm-5v", "llava", "minicpm-v", "minicpm-o",
    "pixtral", "doubao-vision", "step-1v", "step-1o", "hunyuan-vision",
    "internvl", "moonshot-v1-vision", "kimi-latest",
};

inline bool isLikelyVisionModel(const QString& modelId)
{
    const QString id = modelId.trimmed().toLower();
    if (id.isEmpty()) {
        return false;
    }
    for (const char* marker : kVisionMarkers) {
        if (id.contains(QLatin1String(marker))) {
            return true;
        }
    }
    return false;
}

} // namespace VisionModelCapability

#endif // VISION_MODEL_CAPABILITY_H
