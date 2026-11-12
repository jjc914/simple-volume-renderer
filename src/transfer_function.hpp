#ifndef TRANSFER_FUNCTION_HPP
#define TRANSFER_FUNCTION_HPP

#include <glm/glm.hpp>

#include <array>
#include <vector>

// Maps a scalar in [0, 1] to RGBA. Each channel is an independent cubic Bezier
// curve edited with keys + tangent handles, then baked into a lookup table
// that a renderer uploads as a 1D texture.
//
//   TransferFunction tf = TransferFunction::Default();
//   if (tf.DrawEditor()) upload(tf.Bake());   // inside an ImGui window
class TransferFunction {
public:
    enum Channel { Red, Green, Blue, Alpha, ChannelCount };

    struct Key {
        glm::vec2 pos;                 // (scalar, channel value), both in [0, 1]
        glm::vec2 in{-0.1f, 0.0f};     // tangent handles, as offsets from pos
        glm::vec2 out{0.1f, 0.0f};
    };

    // A Bezier curve through `keys`, which are sorted by strictly increasing x.
    // The first key is at x = 0 and the last at x = 1.
    struct Curve {
        std::vector<Key> keys;

        // Value at x in [0, 1], clamped to [0, 1].
        float Sample(float x) const;
    };

    // Blue -> white -> orange ramp with opacity rising with value.
    static TransferFunction Default();

    Curve& GetCurve(Channel c) { return curves_[c]; }
    const Curve& GetCurve(Channel c) const { return curves_[c]; }

    // Sample all channels at x in [0, 1].
    glm::vec4 Sample(float x) const;

    // `size` evenly spaced samples over [0, 1].
    std::vector<glm::vec4> Bake(int size = 256) const;

    // ImGui editor: channel picker, curve canvas and a color/opacity preview.
    //   - drag a key or tangent handle to move it
    //   - double-click empty space to add a key
    //   - right-click a key to remove it (end keys can't be removed)
    // Returns true if the function changed this frame.
    bool DrawEditor();

private:
    bool DrawCurveCanvas(Curve& curve);
    void DrawPreview() const;

    std::array<Curve, ChannelCount> curves_;

    // Editor state.
    int channel_ = Red;
    int selectedKey_ = -1;
    enum class Drag { None, Key, In, Out } drag_ = Drag::None;
};

#endif
