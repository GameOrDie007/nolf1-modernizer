// Shared with the Game Or Die Hands runtime (runtime/gdh_hand.h).
// gdh_hand.h: Game Or Die Hands runtime: load a .gdh hand, pose its fingers from controller
// input, and produce skinning matrices. Engine-neutral, header-only, no dependencies.
//
//   gdh::Hand hand;
//   hand.Load("hand_r.gdh");                        // once
//   gdh::FingerDriver drv; drv.Bind(hand);          // once
//   drv.Update(input, dt, hand);                    // every frame: input -> finger pose
//   hand.Skin();                                    // model-space skinning matrices in hand.skin[]
//
// Space: OpenXR frame (+X right, +Y up, -Z forward), metres, origin at the wrist root of the
// source rig. Placing the hand on a controller is the caller's job (see HandAttach).
//
// Copyright (c) 2026 Game Or Die. MIT licence (see LICENSE). The hand art is CC0 (see ASSETS.md).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gdh {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
// Column-major 4x4, m[col*4+row], same as OpenGL and the .gdh file.
struct Mat4 { float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; };

inline Quat Normalize(Quat q) {
    float l = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (l <= 0) return Quat{};
    return {q.x/l, q.y/l, q.z/l, q.w/l};
}
inline Quat Slerp(Quat a, Quat b, float t) {
    float d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (d < 0) { b = {-b.x, -b.y, -b.z, -b.w}; d = -d; }
    if (d > 0.9995f)
        return Normalize({a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t, a.w + (b.w-a.w)*t});
    float th = std::acos(d), s = std::sin(th);
    float wa = std::sin((1-t)*th)/s, wb = std::sin(t*th)/s;
    return {a.x*wa + b.x*wb, a.y*wa + b.y*wb, a.z*wa + b.z*wb, a.w*wa + b.w*wb};
}
inline Vec3 Lerp(Vec3 a, Vec3 b, float t) { return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t}; }
inline Mat4 FromTR(Vec3 t, Quat q) {
    Mat4 r;
    float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z, xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z, wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    r.m[0]=1-2*(yy+zz); r.m[1]=2*(xy+wz);   r.m[2]=2*(xz-wy);   r.m[3]=0;
    r.m[4]=2*(xy-wz);   r.m[5]=1-2*(xx+zz); r.m[6]=2*(yz+wx);   r.m[7]=0;
    r.m[8]=2*(xz+wy);   r.m[9]=2*(yz-wx);   r.m[10]=1-2*(xx+yy);r.m[11]=0;
    r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z; r.m[15]=1;
    return r;
}
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k*4+rr] * b.m[c*4+k];
            r.m[c*4+rr] = s;
        }
    return r;
}
inline Vec3 TransformPoint(const Mat4& a, Vec3 p) {
    return {a.m[0]*p.x + a.m[4]*p.y + a.m[8]*p.z + a.m[12],
            a.m[1]*p.x + a.m[5]*p.y + a.m[9]*p.z + a.m[13],
            a.m[2]*p.x + a.m[6]*p.y + a.m[10]*p.z + a.m[14]};
}

struct BoneTR { Vec3 pos; Quat rot; };

struct Bone {
    char name[32];
    int32_t parent;
    BoneTR rest;
    Mat4 invBind;
};

#pragma pack(push, 1)
struct Vertex {
    float pos[3];
    float nrm[3];
    float uv[2];
    uint8_t bone[4];
    float weight[4];
};
#pragma pack(pop)
static_assert(sizeof(Vertex) == 52, "Vertex must match the .gdh layout");

struct Mesh {
    std::string name;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
};

struct Pose {
    std::string name;
    std::vector<BoneTR> bones;
};

// Finger groups, found by bone-name prefix. Wrist and palm are not fingers.
enum Finger { kThumb, kIndex, kMiddle, kRing, kLittle, kNotFinger };
inline Finger FingerOf(const char* name) {
    if (!strncmp(name, "Thumb_", 6))  return kThumb;
    if (!strncmp(name, "Index_", 6))  return kIndex;
    if (!strncmp(name, "Middle_", 7)) return kMiddle;
    if (!strncmp(name, "Ring_", 5))   return kRing;
    if (!strncmp(name, "Little_", 7)) return kLittle;
    return kNotFinger;
}

struct Hand {
    std::vector<Bone> bones;
    std::vector<Mesh> meshes;
    std::vector<Pose> poses;
    std::vector<BoneTR> local;   // current pose, parent-relative (written by FingerDriver or SetPose)
    std::vector<Mat4> model;     // current pose, model space
    std::vector<Mat4> skin;      // model * invBind, what a skinning shader wants
    std::string error;

    bool Load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) { error = std::string("cannot open ") + path; return false; }
        std::vector<uint8_t> d;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        d.resize(n > 0 ? (size_t)n : 0);
        size_t got = d.empty() ? 0 : fread(d.data(), 1, d.size(), f);
        fclose(f);
        if (got != d.size()) { error = "short read"; return false; }
        return LoadMemory(d.data(), d.size());
    }

    bool LoadMemory(const uint8_t* p, size_t n) {
        size_t o = 0;
        auto need = [&](size_t k) { return o + k <= n; };
        auto rd = [&](void* dst, size_t k) { memcpy(dst, p + o, k); o += k; };
        if (!need(20) || memcmp(p, "GDH1", 4)) { error = "not a GDH1 file"; return false; }
        o = 4;
        uint32_t ver, nb, nm, np;
        rd(&ver, 4); rd(&nb, 4); rd(&nm, 4); rd(&np, 4);
        if (ver != 1 || nb == 0 || nb > 255) { error = "unsupported version or bone count"; return false; }
        bones.resize(nb);
        for (auto& b : bones) {
            if (!need(32 + 4 + 28 + 64)) { error = "truncated bones"; return false; }
            rd(b.name, 32); b.name[31] = 0;
            rd(&b.parent, 4);
            rd(&b.rest.pos, 12); rd(&b.rest.rot, 16);
            rd(b.invBind.m, 64);
        }
        for (uint32_t i = 0; i < nb; ++i)
            if (bones[i].parent >= (int32_t)i) { error = "bones not parent-first"; return false; }
        meshes.resize(nm);
        for (auto& m : meshes) {
            char nm32[32]; uint32_t nv, ni;
            if (!need(40)) { error = "truncated mesh"; return false; }
            rd(nm32, 32); nm32[31] = 0; m.name = nm32;
            rd(&nv, 4); rd(&ni, 4);
            if (!need((size_t)nv * sizeof(Vertex) + (size_t)ni * 4)) { error = "truncated mesh data"; return false; }
            m.verts.resize(nv); rd(m.verts.data(), (size_t)nv * sizeof(Vertex));
            m.indices.resize(ni); rd(m.indices.data(), (size_t)ni * 4);
            for (auto& v : m.verts) for (int k = 0; k < 4; ++k) if (v.bone[k] >= nb) { error = "bad bone index"; return false; }
            for (auto i : m.indices) if (i >= nv) { error = "bad vertex index"; return false; }
        }
        poses.resize(np);
        for (auto& ps : poses) {
            char nm32[32];
            if (!need(32 + (size_t)nb * 28)) { error = "truncated poses"; return false; }
            rd(nm32, 32); nm32[31] = 0; ps.name = nm32;
            ps.bones.resize(nb);
            for (auto& b : ps.bones) { rd(&b.pos, 12); rd(&b.rot, 16); }
        }
        local.resize(nb);
        for (uint32_t i = 0; i < nb; ++i) local[i] = bones[i].rest;
        model.resize(nb); skin.resize(nb);
        Skin();
        return true;
    }

    int FindPose(const char* name) const {
        for (size_t i = 0; i < poses.size(); ++i) if (poses[i].name == name) return (int)i;
        return -1;
    }
    int FindMesh(const char* name) const {
        for (size_t i = 0; i < meshes.size(); ++i) if (meshes[i].name.find(name) != std::string::npos) return (int)i;
        return -1;
    }
    void SetPose(int pose) { if (pose >= 0 && pose < (int)poses.size()) local = poses[pose].bones; }

    void Skin() {
        for (size_t i = 0; i < bones.size(); ++i) {
            Mat4 l = FromTR(local[i].pos, local[i].rot);
            model[i] = bones[i].parent < 0 ? l : Mul(model[bones[i].parent], l);
            skin[i] = Mul(model[i], bones[i].invBind);
        }
    }

    // CPU skinning of one mesh with the current pose, for tests and for engines with no GPU skinning.
    void SkinPositions(int mesh, std::vector<Vec3>& out) const {
        const Mesh& m = meshes[mesh];
        out.resize(m.verts.size());
        for (size_t i = 0; i < m.verts.size(); ++i) {
            const Vertex& v = m.verts[i];
            Vec3 acc, p{v.pos[0], v.pos[1], v.pos[2]};
            for (int k = 0; k < 4; ++k) {
                if (v.weight[k] <= 0) continue;
                Vec3 q = TransformPoint(skin[v.bone[k]], p);
                acc.x += q.x * v.weight[k]; acc.y += q.y * v.weight[k]; acc.z += q.z * v.weight[k];
            }
            out[i] = acc;
        }
    }
};

// Controller state for one hand, as OpenXR reports it. Missing touch sensors: pass touch = value > 0.
struct HandInput {
    float trigger = 0;          // /input/trigger/value
    bool  triggerTouch = false; // /input/trigger/touch
    float grip = 0;             // /input/squeeze/value
    bool  thumbTouch = false;   // any of thumbstick/thumbrest/a/b/x/y touch
};

// Turns controller input into a finger pose by blending key poses per finger:
// index follows the trigger, middle/ring/little follow the grip, the thumb lies down when
// touching a button and stands up (thumbs-up) when the hand is closed without touching.
// Pose names are the source rig's; override them before Bind() for another rig.
struct FingerDriver {
    const char* openPose = "Grip 5";      // relaxed open hand
    const char* fistPose = "Grip";        // closed hand
    const char* thumbUpPose = "Thumb";    // thumbs-up
    float indexRestOnTrigger = 0.30f;     // index curl when the finger rests on the trigger
    float smoothTime = 0.06f;             // seconds, for the touch sensors that jump 0/1
    // How far into the fist pose a full squeeze goes. At 1.0 the fingertips of our slender hands
    // sink 2-3.5 mm into the palm (tests/fist_depth.py); they first touch at 0.92.
    float fistLimit = 0.92f;

    int open = -1, fist = -1, thumbUp = -1;
    std::vector<Finger> fingerOf;
    float wIndex = 0, wGrip = 0, wThumbDown = 0, wThumbUp = 0;

    bool Bind(const Hand& h) {
        open = h.FindPose(openPose); fist = h.FindPose(fistPose); thumbUp = h.FindPose(thumbUpPose);
        fingerOf.clear();
        for (auto& b : h.bones) fingerOf.push_back(FingerOf(b.name));
        return open >= 0 && fist >= 0 && thumbUp >= 0;
    }

    static float Approach(float cur, float target, float dt, float tau) {
        if (tau <= 0 || dt <= 0) return target;
        float a = 1.0f - std::exp(-dt / tau);
        return cur + (target - cur) * a;
    }

    void Update(const HandInput& in, float dt, Hand& h) {
        float tIndex = in.triggerTouch ? indexRestOnTrigger + (1 - indexRestOnTrigger) * in.trigger : in.trigger;
        float tThumbDown = in.thumbTouch ? 1.0f : 0.0f;
        float tThumbUp = in.thumbTouch ? 0.0f : in.grip;   // thumbs-up only when the hand closes
        // Analog values are already smooth; smooth them lightly anyway so a jump from touch stays soft.
        wIndex = Approach(wIndex, tIndex, dt, smoothTime * 0.5f);
        wGrip = Approach(wGrip, in.grip, dt, smoothTime * 0.5f);
        wThumbDown = Approach(wThumbDown, tThumbDown, dt, smoothTime);
        wThumbUp = Approach(wThumbUp, tThumbUp, dt, smoothTime);
        Apply(h);
    }

    void Apply(Hand& h) const {
        if (open < 0) return;
        const auto& O = h.poses[open].bones; const auto& F = h.poses[fist].bones; const auto& U = h.poses[thumbUp].bones;
        for (size_t i = 0; i < h.bones.size(); ++i) {
            BoneTR r = O[i];
            switch (fingerOf[i]) {
            case kIndex: r.rot = Slerp(O[i].rot, F[i].rot, wIndex * fistLimit); r.pos = Lerp(O[i].pos, F[i].pos, wIndex * fistLimit); break;
            case kMiddle: case kRing: case kLittle:
                r.rot = Slerp(O[i].rot, F[i].rot, wGrip * fistLimit); r.pos = Lerp(O[i].pos, F[i].pos, wGrip * fistLimit); break;
            case kThumb: {
                // up/open first, then down onto the controller when touching
                Quat upOrOpen = Slerp(O[i].rot, U[i].rot, wThumbUp);
                Quat downRot = Slerp(O[i].rot, F[i].rot, std::fmax(wGrip, 0.5f));
                r.rot = Slerp(upOrOpen, downRot, wThumbDown);
                r.pos = Lerp(O[i].pos, F[i].pos, wThumbDown * 0.5f);
                break;
            }
            default: break;
            }
            h.local[i] = r;
        }
    }
};

// Where the hand sits relative to the controller's OpenXR GRIP pose (right hand values; the left
// mirrors X, yaw and roll). Default from tools/fit_grip.py: the palm centroid on the grip origin, as
// the spec defines the grip, tilted 45 degrees down the way real runtimes report it (Godot XR
// Tools' measured default). The viewer's tuner refines it in the headset.
struct HandAttach {
    float x = 0.0308f, y = 0.0290f, z = 0.0275f;  // metres, in grip space
    float pitch = -45, yaw = 0, roll = 0;         // degrees
};

inline Quat QuatFromEulerDeg(float pitch, float yaw, float roll) {
    const float d = 3.14159265358979f / 180.0f;
    float cp = std::cos(pitch*d/2), sp = std::sin(pitch*d/2);
    float cy = std::cos(yaw*d/2),   sy = std::sin(yaw*d/2);
    float cr = std::cos(roll*d/2),  sr = std::sin(roll*d/2);
    // yaw (Y) * pitch (X) * roll (Z)
    Quat qy{0, sy, 0, cy}, qx{sp, 0, 0, cp}, qz{0, 0, sr, cr};
    auto mul = [](Quat a, Quat b) {
        return Quat{a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
                    a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
                    a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
                    a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};
    };
    return mul(mul(qy, qx), qz);
}

inline Mat4 AttachMatrix(const HandAttach& a, bool leftHand) {
    float s = leftHand ? -1.0f : 1.0f;
    return FromTR({a.x * s, a.y, a.z}, QuatFromEulerDeg(a.pitch, a.yaw * s, a.roll * s));
}

} // namespace gdh
