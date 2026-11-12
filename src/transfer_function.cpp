#include "transfer_function.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kMinKeyGap = 0.005f;   // keeps key x strictly increasing
constexpr float kHitRadius = 8.0f;     // px
constexpr float kPad = 8.0f;           // px, so end keys aren't clipped
constexpr float kCanvasHeight = 160.0f;

float Bezier(float t, float p0, float p1, float p2, float p3) {
    float u = 1.0f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

float Dist2(ImVec2 a, ImVec2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Keep a key's handles inside the neighbouring keys' x range (which keeps
// x(t) monotonic, so the curve stays a function) and inside [0, 1] in y.
void ClampHandles(TransferFunction::Curve& curve, int i) {
    if (i < 0 || i >= int(curve.keys.size())) return;
    auto& k = curve.keys[i];
    float minX = i > 0 ? curve.keys[i - 1].pos.x : k.pos.x;
    float maxX = i + 1 < int(curve.keys.size()) ? curve.keys[i + 1].pos.x : k.pos.x;
    k.in.x  = std::clamp(k.in.x,  minX - k.pos.x, 0.0f);
    k.out.x = std::clamp(k.out.x, 0.0f, maxX - k.pos.x);
    k.in.y  = std::clamp(k.pos.y + k.in.y,  0.0f, 1.0f) - k.pos.y;
    k.out.y = std::clamp(k.pos.y + k.out.y, 0.0f, 1.0f) - k.pos.y;
}

TransferFunction::Curve MakeCurve(std::initializer_list<glm::vec2> points) {
    TransferFunction::Curve c;
    for (glm::vec2 p : points) c.keys.push_back({p});
    for (int i = 0; i < int(c.keys.size()); ++i) {
        // Default handles span a third of the gap to each neighbour.
        float gapL = i > 0 ? c.keys[i].pos.x - c.keys[i - 1].pos.x : 0.0f;
        float gapR = i + 1 < int(c.keys.size()) ? c.keys[i + 1].pos.x - c.keys[i].pos.x : 0.0f;
        c.keys[i].in  = {-gapL / 3.0f, 0.0f};
        c.keys[i].out = { gapR / 3.0f, 0.0f};
    }
    return c;
}

}  // namespace

// ---------------------------------------------------------------------------
// Curve / evaluation
// ---------------------------------------------------------------------------

float TransferFunction::Curve::Sample(float x) const {
    if (keys.empty()) return 0.0f;
    if (x <= keys.front().pos.x) return keys.front().pos.y;
    if (x >= keys.back().pos.x) return keys.back().pos.y;

    size_t i = 0;
    while (i + 2 < keys.size() && x >= keys[i + 1].pos.x) ++i;

    const Key& a = keys[i];
    const Key& b = keys[i + 1];
    glm::vec2 p0 = a.pos, p1 = a.pos + a.out, p2 = b.pos + b.in, p3 = b.pos;

    // x(t) is monotonic (see ClampHandles), so bisect for t with x(t) = x.
    float lo = 0.0f, hi = 1.0f, t = 0.5f;
    for (int iter = 0; iter < 24; ++iter) {
        t = 0.5f * (lo + hi);
        if (Bezier(t, p0.x, p1.x, p2.x, p3.x) < x) lo = t; else hi = t;
    }
    return std::clamp(Bezier(t, p0.y, p1.y, p2.y, p3.y), 0.0f, 1.0f);
}

TransferFunction TransferFunction::Default() {
    TransferFunction tf;
    tf.curves_[Red]   = MakeCurve({{0.0f, 0.1f}, {0.5f, 1.0f}, {1.0f, 1.0f}});
    tf.curves_[Green] = MakeCurve({{0.0f, 0.2f}, {0.5f, 1.0f}, {1.0f, 0.5f}});
    tf.curves_[Blue]  = MakeCurve({{0.0f, 0.9f}, {0.5f, 1.0f}, {1.0f, 0.1f}});
    // Ease-in opacity, roughly t^2.
    tf.curves_[Alpha] = MakeCurve({{0.0f, 0.0f}, {1.0f, 1.0f}});
    tf.curves_[Alpha].keys[0].out = {0.5f, 0.0f};
    tf.curves_[Alpha].keys[1].in  = {-0.25f, -0.5f};
    return tf;
}

glm::vec4 TransferFunction::Sample(float x) const {
    return {curves_[Red].Sample(x), curves_[Green].Sample(x),
            curves_[Blue].Sample(x), curves_[Alpha].Sample(x)};
}

std::vector<glm::vec4> TransferFunction::Bake(int size) const {
    size = std::max(size, 2);
    std::vector<glm::vec4> table(size);
    for (int i = 0; i < size; ++i) table[i] = Sample(float(i) / float(size - 1));
    return table;
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

bool TransferFunction::DrawEditor() {
    bool changed = false;
    ImGui::PushID(this);

    static const char* names[ChannelCount] = {"R", "G", "B", "A"};
    for (int c = 0; c < ChannelCount; ++c) {
        if (c > 0) ImGui::SameLine();
        if (ImGui::RadioButton(names[c], channel_ == c)) {
            channel_ = c;
            selectedKey_ = -1;
            drag_ = Drag::None;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        *this = Default();
        changed = true;
    }

    changed |= DrawCurveCanvas(curves_[channel_]);
    DrawPreview();

    ImGui::PopID();
    return changed;
}

bool TransferFunction::DrawCurveCanvas(Curve& curve) {
    bool changed = false;

    ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 120.0f), kCanvasHeight);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    ImVec2 plotMin(origin.x + kPad, origin.y + kPad);
    ImVec2 plotSize(size.x - 2 * kPad, size.y - 2 * kPad);
    auto toScreen = [&](glm::vec2 p) {
        return ImVec2(plotMin.x + p.x * plotSize.x, plotMin.y + (1.0f - p.y) * plotSize.y);
    };
    auto toCurve = [&](ImVec2 s) {
        return glm::vec2((s.x - plotMin.x) / plotSize.x, 1.0f - (s.y - plotMin.y) / plotSize.y);
    };

    const int n = int(curve.keys.size());
    ImVec2 mouse = ImGui::GetMousePos();

    // Nearest key / handle of the selected key under the cursor, if any.
    auto hitTest = [&](int* key, Drag* handle) {
        *key = -1;
        *handle = Drag::None;
        const float r2 = kHitRadius * kHitRadius;
        if (selectedKey_ >= 0 && selectedKey_ < n) {
            const Key& k = curve.keys[selectedKey_];
            if (selectedKey_ > 0 &&
                Dist2(mouse, toScreen(k.pos + k.in)) <= r2) {
                *key = selectedKey_; *handle = Drag::In; return;
            }
            if (selectedKey_ < n - 1 &&
                Dist2(mouse, toScreen(k.pos + k.out)) <= r2) {
                *key = selectedKey_; *handle = Drag::Out; return;
            }
        }
        float best = r2;
        for (int i = 0; i < n; ++i) {
            float d = Dist2(mouse, toScreen(curve.keys[i].pos));
            if (d <= best) { best = d; *key = i; *handle = Drag::Key; }
        }
    };

    // --- Input -------------------------------------------------------------
    if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int key; Drag handle;
        hitTest(&key, &handle);
        if (key >= 0) {
            selectedKey_ = key;
            drag_ = handle;
        } else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            glm::vec2 p = toCurve(mouse);
            p.x = std::clamp(p.x, 0.0f, 1.0f);
            p.y = std::clamp(p.y, 0.0f, 1.0f);
            int at = 1;
            while (at < n - 1 && curve.keys[at].pos.x < p.x) ++at;
            float left = curve.keys[at - 1].pos.x, right = curve.keys[at].pos.x;
            if (p.x > left + kMinKeyGap && p.x < right - kMinKeyGap) {
                Key k{p};
                k.in  = {-(p.x - left) / 3.0f, 0.0f};
                k.out = { (right - p.x) / 3.0f, 0.0f};
                curve.keys.insert(curve.keys.begin() + at, k);
                selectedKey_ = at;
                changed = true;
            }
        } else {
            selectedKey_ = -1;
        }
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        int key; Drag handle;
        hitTest(&key, &handle);
        if (key > 0 && key < n - 1) {
            curve.keys.erase(curve.keys.begin() + key);
            ClampHandles(curve, key - 1);
            ClampHandles(curve, key);
            selectedKey_ = -1;
            drag_ = Drag::None;
            changed = true;
        }
    }

    if (drag_ != Drag::None) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || selectedKey_ < 0 || selectedKey_ >= int(curve.keys.size())) {
            drag_ = Drag::None;
        } else {
            const int i = selectedKey_;
            Key& k = curve.keys[i];
            glm::vec2 m = toCurve(mouse);
            if (drag_ == Drag::Key) {
                glm::vec2 before = k.pos;
                k.pos.y = std::clamp(m.y, 0.0f, 1.0f);
                if (i > 0 && i < int(curve.keys.size()) - 1) {   // end keys keep x = 0 / 1
                    k.pos.x = std::clamp(m.x, curve.keys[i - 1].pos.x + kMinKeyGap,
                                              curve.keys[i + 1].pos.x - kMinKeyGap);
                }
                changed |= (k.pos != before);
            } else {
                glm::vec2 offset = m - k.pos;
                glm::vec2& handle = drag_ == Drag::In ? k.in : k.out;
                changed |= (handle != offset);
                handle = offset;
            }
            ClampHandles(curve, i - 1);
            ClampHandles(curve, i);
            ClampHandles(curve, i + 1);
        }
    }

    // --- Drawing -----------------------------------------------------------
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(0, 0, 0, 255));
    for (int g = 0; g <= 4; ++g) {
        float f = g / 4.0f;
        dl->AddLine(toScreen({f, 0.0f}), toScreen({f, 1.0f}), IM_COL32(255, 255, 255, 30));
        dl->AddLine(toScreen({0.0f, f}), toScreen({1.0f, f}), IM_COL32(255, 255, 255, 30));
    }

    for (int i = 0; i + 1 < n; ++i) {
        const Key& a = curve.keys[i];
        const Key& b = curve.keys[i + 1];
        dl->AddBezierCubic(toScreen(a.pos), toScreen(a.pos + a.out),
                           toScreen(b.pos + b.in), toScreen(b.pos),
                           IM_COL32(255, 255, 255, 255), 1.5f);
    }

    for (int i = 0; i < n; ++i) {
        const Key& k = curve.keys[i];
        ImVec2 kp = toScreen(k.pos);
        if (i == selectedKey_) {
            if (i > 0) {
                ImVec2 h = toScreen(k.pos + k.in);
                dl->AddLine(h, kp, IM_COL32(155, 155, 155, 255));
                dl->AddCircleFilled(h, 3.5f, IM_COL32(50, 255, 50, 255));
            }
            if (i < n - 1) {
                ImVec2 h = toScreen(k.pos + k.out);
                dl->AddLine(kp, h, IM_COL32(155, 155, 155, 255));
                dl->AddCircleFilled(h, 3.5f, IM_COL32(50, 255, 50, 255));
            }
            dl->AddCircleFilled(kp, 4.5f, IM_COL32(255, 50, 50, 255));
        } else {
            dl->AddCircleFilled(kp, 3.5f, IM_COL32(255, 255, 255, 255));
        }
    }

    return changed;
}

void TransferFunction::DrawPreview() const {
    const int segments = 96;
    const float rowHeight = 14.0f;
    float width = std::max(ImGui::GetContentRegionAvail().x, 120.0f);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    std::vector<glm::vec4> table = Bake(segments);
    float w = width / segments;
    for (int i = 0; i < segments; ++i) {
        const glm::vec4& c = table[i];
        float x0 = origin.x + i * w, x1 = origin.x + (i + 1) * w;
        // Row 1: color; row 2: opacity as gray.
        dl->AddRectFilled(ImVec2(x0, origin.y), ImVec2(x1, origin.y + rowHeight),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1.0f)));
        dl->AddRectFilled(ImVec2(x0, origin.y + rowHeight), ImVec2(x1, origin.y + 2 * rowHeight),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(c.a, c.a, c.a, 1.0f)));
    }
    ImGui::Dummy(ImVec2(width, 2 * rowHeight));
}
