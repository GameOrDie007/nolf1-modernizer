// Shared with the Game Or Die Hands runtime (runtime/gdh_fit.h).
// gdh_fit.h: Game Or Die Hands: fit our hand onto a game's own hand at run time. Header-only.
//
// A game's first-person weapon model usually carries the game's own hand, posed on the gun. Drawing
// our hand where that hand is, rather than moving the gun to ours, keeps the gun exactly where the
// port already puts it. The fit is the least-squares similarity (scale, rotation, translation) that
// takes a few of our joints onto the game's matching joints: the wrist, the thumb's base and the four
// knuckles, which a finger's curl does not move.
//
//   gdh::Similarity s = gdh::FitSimilarity(ours, theirs, 6);   // theirs ~ s.scale * s.rot * ours + s.pos
//
// Copyright (c) 2026 Game Or Die. MIT licence (see LICENSE).
#pragma once
#include <cmath>

namespace gdh {

struct Similarity {
    double scale = 1;
    double rot[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};   // proper rotation, row-major
    double pos[3] = {0, 0, 0};
    bool ok = false;
};

// Jacobi eigen decomposition of a symmetric 4x4 (Horn's absolute orientation).
inline void Jacobi4(double a[4][4], double vec[4][4], double val[4]) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) vec[i][j] = (i == j) ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 64; sweep++) {
        double off = 0;
        for (int p = 0; p < 4; p++) for (int q = p + 1; q < 4; q++) off += a[p][q] * a[p][q];
        if (off < 1e-24) break;
        for (int p = 0; p < 4; p++) for (int q = p + 1; q < 4; q++) {
            if (std::fabs(a[p][q]) < 1e-300) continue;
            double th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
            double t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1));
            double c = 1 / std::sqrt(t * t + 1), s = t * c;
            for (int k = 0; k < 4; k++) { double akp = a[k][p], akq = a[k][q]; a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq; }
            for (int k = 0; k < 4; k++) { double apk = a[p][k], aqk = a[q][k]; a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk; }
            for (int k = 0; k < 4; k++) { double vkp = vec[k][p], vkq = vec[k][q]; vec[k][p] = c * vkp - s * vkq; vec[k][q] = s * vkp + c * vkq; }
        }
    }
    for (int i = 0; i < 4; i++) val[i] = a[i][i];
}

// src and dst: n points each, xyz packed. Same method as tools/retarget_goldsrc.py's umeyama.
inline Similarity FitSimilarity(const double (*src)[3], const double (*dst)[3], int n) {
    Similarity r;
    if (n < 3) return r;
    double ms[3] = {0, 0, 0}, md[3] = {0, 0, 0};
    for (int i = 0; i < n; i++) for (int k = 0; k < 3; k++) { ms[k] += src[i][k] / n; md[k] += dst[i][k] / n; }
    double S[3][3] = {}, var = 0;
    for (int i = 0; i < n; i++) {
        double a[3], b[3];
        for (int k = 0; k < 3; k++) { a[k] = src[i][k] - ms[k]; b[k] = dst[i][k] - md[k]; var += a[k] * a[k]; }
        for (int u = 0; u < 3; u++) for (int v = 0; v < 3; v++) S[u][v] += a[u] * b[v];
    }
    if (var < 1e-18) return r;
    double N[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2], S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0], S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2], S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1], -S[0][0] - S[1][1] + S[2][2]}};
    double vec[4][4], val[4];
    Jacobi4(N, vec, val);
    int k = 0;
    for (int i = 1; i < 4; i++) if (val[i] > val[k]) k = i;
    const double w = vec[0][k], x = vec[1][k], y = vec[2][k], z = vec[3][k];
    const double R[3][3] = {
        {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
        {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
        {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    double num = 0;
    for (int i = 0; i < n; i++) {
        double a[3], b[3], Ra[3];
        for (int c = 0; c < 3; c++) { a[c] = src[i][c] - ms[c]; b[c] = dst[i][c] - md[c]; }
        for (int u = 0; u < 3; u++) Ra[u] = R[u][0] * a[0] + R[u][1] * a[1] + R[u][2] * a[2];
        num += b[0] * Ra[0] + b[1] * Ra[1] + b[2] * Ra[2];
    }
    r.scale = num / var;
    for (int u = 0; u < 3; u++) for (int v = 0; v < 3; v++) r.rot[u][v] = R[u][v];
    for (int u = 0; u < 3; u++)
        r.pos[u] = md[u] - r.scale * (R[u][0] * ms[0] + R[u][1] * ms[1] + R[u][2] * ms[2]);
    r.ok = r.scale > 0;
    return r;
}

}  // namespace gdh
