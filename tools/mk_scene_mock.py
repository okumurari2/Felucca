#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Still mock-ups of CORNERS' middle as a stylised nature scene (docs/SCENE_PLAN.md): three subjects (water / forest / sea),
four seasons from the key (ROOT), calm and busy states. Not firmware: it only fixes the picture and the mapping.
  python3 tools/mk_scene_mock.py [OUT_DIR]     (default docs/shots; writes mk_scene_seasons.png, mk_scene_states.png)"""
import math, random, sys, os
from PIL import Image, ImageDraw, ImageFilter

S = 4; N = 240; W = N * S; BG = (5, 6, 10); R = 84 * S           # supersample 4x; the window's radius 80 px
CX = CY = W // 2

SEASON = {  # sky top, sky bottom, deep (water / soil / sea), light (glint / mist / foam), leaves, ground
    'spring': dict(s0=(150, 190, 220), s1=(238, 222, 224), deep=(70, 120, 140), light=(235, 245, 250), leaf=[(244, 168, 190), (250, 206, 214), (150, 200, 120)], ground=(120, 100, 80), sand=(226, 204, 170)),
    'summer': dict(s0=(80, 150, 214), s1=(200, 228, 232), deep=(30, 96, 130), light=(240, 250, 245), leaf=[(52, 150, 84), (90, 180, 90), (30, 110, 70)], ground=(84, 70, 52), sand=(238, 214, 160)),
    'autumn': dict(s0=(204, 148, 110), s1=(244, 214, 168), deep=(70, 90, 100), light=(250, 232, 200), leaf=[(214, 84, 40), (236, 150, 44), (170, 60, 36)], ground=(104, 70, 44), sand=(214, 184, 140)),
    'winter': dict(s0=(130, 150, 184), s1=(222, 228, 238), deep=(60, 82, 112), light=(244, 248, 255), leaf=[(240, 244, 250), (210, 220, 236), (255, 255, 255)], ground=(150, 156, 170), sand=(226, 228, 236)),
}
def season_of(root): return ['spring', 'summer', 'autumn', 'winter'][(root % 12) // 3]

def mix(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
def dusk(c, minor): return mix(c, (200, 110, 90), 0.22) if minor else c
def dim(c, k): return tuple(int(v * k) for v in c)

class Cv:
    def __init__(s): s.im = Image.new('RGBA', (W, W), BG + (255,))
    def layer(s): 
        l = Image.new('RGBA', (W, W), (0, 0, 0, 0)); return l, ImageDraw.Draw(l)
    def put(s, l): s.im = Image.alpha_composite(s.im, l)

def grad(d, y0, y1, c0, c1, x0=0, x1=W, a=255):
    for y in range(int(y0), int(y1)): d.line([(x0, y), (x1, y)], fill=mix(c0, c1, (y - y0) / max(1, y1 - y0)) + (a,))

def wave(d, y0, amp, k, ph, col, width, a=255, x0=0, x1=W, step=6):
    pts = [(x, y0 + amp * math.sin(x * k + ph)) for x in range(int(x0), int(x1), step)]
    d.line(pts, fill=col + (a,), width=int(width))

def stroke(d, p0, p1, bend, col, w, a, gap=0.0, rnd=None):      # a tapering curved stroke (wind); gap: chance a piece is missing
    n = 14
    for i in range(n):
        if rnd and rnd.random() < gap: continue
        t0, t1 = i / n, (i + 1) / n
        def pt(t): return (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t + bend * math.sin(t * math.pi))
        k = math.sin((t0 + t1) / 2 * math.pi)
        d.line([pt(t0), pt(t1)], fill=col + (int(a * k),), width=max(1, int(w * k)))

def leaf(d, x, y, ang, size, col, a=255, dark=None):
    pts = []
    for i in range(0, 17):
        t = i / 16 * 2 * math.pi
        u = math.cos(t) * size; v = math.sin(t) * size * 0.42 * (1 - 0.3 * math.cos(t))
        pts.append((x + u * math.cos(ang) - v * math.sin(ang), y + u * math.sin(ang) + v * math.cos(ang)))
    d.polygon(pts, fill=col + (a,))
    ex, ey = size * math.cos(ang), size * math.sin(ang)
    d.line([(x - ex * 0.9, y - ey * 0.9), (x + ex * 0.9, y + ey * 0.9)], fill=(dark or dim(col, 0.6)) + (a,), width=max(1, S))

def flake(d, x, y, size, col, a=255):
    d.ellipse([x - size * .5, y - size * .5, x + size * .5, y + size * .5], fill=col + (a,))

def ring(d, x, y, rx, ry, col, a, w): d.ellipse([x - rx, y - ry, x + rx, y + ry], outline=col + (int(a),), width=max(1, int(w)))

def finish(cv):
    im = cv.im.convert('RGB')
    mask = Image.new('L', (W, W), 0); md = ImageDraw.Draw(mask)
    for i in range(30):                                          # the edge melts into the background (no rim)
        r = R * (1 - i / 30 * 0.3)
        md.ellipse([CX - r, CY - r, CX + r, CY + r], fill=int(255 * (1 - (i / 30) ** 1.6) if i else 255))
    m2 = Image.new('L', (W, W), 0); m2d = ImageDraw.Draw(m2)
    steps = 40
    for i in range(steps):                                       # alpha: 1 inside 0.84 R, 0 at R
        r = R * (1 - i / steps * 0.28)
    m2 = Image.new('L', (W, W), 0)
    px = m2.load()
    for y in range(0, W):
        for x in range(0, W):
            dd = math.hypot(x - CX, y - CY) / R
            if dd < 1: 
                t = 1.0 if dd < 0.84 else (1 - (dd - 0.84) / 0.16)
                px[x, y] = int(255 * t * t * (3 - 2 * t))
    out = Image.composite(im, Image.new('RGB', (W, W), BG), m2)
    return out.resize((N, N), Image.LANCZOS)

# ------------------------------------------------------------------ the three subjects
# st: lv = [drop, floor, wind, voice] 0..1; m = 12 macros 0..1 (dens tone wash | open tone sway | grain tone air | loose tone trail)
def water(P, st, rnd):
    cv = Cv(); sp, minor, lv, m = P['sp'], P['minor'], st['lv'], st['m']
    hz = int(W * 0.52); s0, s1 = dusk(sp['s0'], minor), dusk(sp['s1'], minor)
    l, d = cv.layer(); grad(d, 0, hz, s0, s1)
    far = mix(s1, sp['deep'], 0.35)
    d.polygon([(0, hz)] + [(x, hz - 14 * S - 10 * S * math.sin(x / W * 5 + 1) - 6 * S * math.sin(x / W * 11)) for x in range(0, W + 8, 8)] + [(W, hz)], fill=far + (255,))
    wc0 = mix(sp['deep'], s1, 0.2 + 0.5 * m[4]); wc1 = dim(sp['deep'], 0.7)
    grad(d, hz, W, wc0, wc1); cv.put(l)
    l, d = cv.layer()                                             # the stream: bands that flow, glints by TONE, bends by SWAY
    nb = 3 + int(m[3] * 9)
    for i in range(nb):
        t = (i + 1) / (nb + 1); y = hz + (W * 0.5 - hz) * (t ** 1.7) * 1.05
        wave(d, y, (1 + 7 * m[5]) * S * (0.4 + t), 0.02 / S * 3 * (1.2 - 0.6 * t), i * 1.7 + 0.5, mix(wc0, sp['light'], 0.25 + 0.3 * m[4]), (0.8 + 2.4 * t) * S, int(30 + 90 * lv[1]))
    for i in range(int(lv[1] * (4 + 40 * m[4] ** 2))):           # glints
        y = hz + rnd.random() ** 1.5 * (W * 0.45 - hz) + 6 * S; x = rnd.random() * W
        d.line([(x, y), (x + (6 + 8 * rnd.random()) * S * (0.5 + (y - hz) / W), y)], fill=sp['light'] + (int(70 + 120 * m[4]),), width=S)
    cv.put(l)
    l, d = cv.layer()                                             # drops and their rings
    for i in range(1 + int(m[0] * 5)):
        x = (0.18 + 0.64 * rnd.random()) * W; y = hz + (0.15 + 0.7 * rnd.random()) * (W * 0.42 - hz) * 2.2; age = rnd.random()
        sz = (7 + 12 * lv[0]) * S * (0.5 + (y - hz) / W)
        for k in range(1 + int(m[2] * 3)):
            ra = age * (0.6 + 0.4 * m[2]) * 0.6 + k * 0.17
            if ra < 1: ring(d, x, y, sz * (0.4 + 2.6 * ra), sz * (0.4 + 2.6 * ra) * 0.38, (255, 255, 255), 255 * min(1, lv[0] * 1.4) * (1 - ra) ** 1.1, (2.0 + 1.6 * (1 - ra)) * S * 0.7)
    cv.put(l)
    l, d = cv.layer()                                             # wind: strokes, broken by GRAIN, long and high by AIR
    nw = int(lv[2] * (2 + 5 * (0.3 + m[6])))
    for i in range(nw):
        y = (0.12 + 0.32 * rnd.random() + (1 - m[8]) * 0.12) * W; x = (rnd.random() * 0.7 - 0.1) * W
        L = (0.15 + 0.4 * m[8]) * W
        stroke(d, (x, y), (x + L, y + (rnd.random() - .5) * 20 * S), (rnd.random() - .5) * 26 * S, mix(sp['light'], (255, 255, 255), m[7]), (1.5 + 2 * lv[2]) * S, 150 * lv[2], gap=0.45 * m[6], rnd=rnd)
    cv.put(l)
    l, d = cv.layer()                                             # branch and leaves, on the water too
    bw = 5 * S
    d.line([(-10 * S, 36 * S), (40 * S, 40 * S), (80 * S, 36 * S), (118 * S, 50 * S)], fill=dim(sp['ground'], 0.8) + (255,), width=bw, joint='curve')
    d.line([(60 * S, 38 * S), (84 * S, 22 * S)], fill=dim(sp['ground'], 0.8) + (255,), width=3 * S)
    wsh = lv[2] * 18 * S
    for i in range(int(1 + lv[3] * 6)):
        t = rnd.random(); col = rnd.choice(sp['leaf']); col = mix(col, (255, 230, 120), 0.3 * m[10]) if P['sp_name'] != 'winter' else col
        x = 70 * S + t * 80 * S + wsh * t * 1.4; y = 44 * S + t * (hz - 40 * S) * rnd.random() + rnd.random() * 60 * S
        ang = (rnd.random() - .5) * (0.5 + 5 * m[9]) + 0.6 * (lv[2] > .5)
        sz = (6 + 7 * lv[3]) * S
        if P['sp_name'] == 'winter': flake(d, x, y, sz * .6, col)
        else: leaf(d, x, y, ang, sz, col)
    for i in range(int(lv[3] * 3 + 1)):                           # on the water, a wake behind (TRAIL)
        x = (0.25 + 0.5 * rnd.random()) * W; y = hz + (0.2 + 0.5 * rnd.random()) * (W * 0.4 - hz) * 2; col = rnd.choice(sp['leaf'])
        d.line([(x - (6 + 40 * m[11]) * S, y + 1 * S), (x, y)], fill=sp['light'] + (int(90 * lv[3]),), width=S * 2)
        if P['sp_name'] == 'winter': flake(d, x, y, 5 * S, col)
        else: leaf(d, x, y, 0.05, (5 + 4 * lv[3]) * S, col)
    cv.put(l)
    return finish(cv)

def forest(P, st, rnd):
    cv = Cv(); sp, minor, lv, m = P['sp'], P['minor'], st['lv'], st['m']
    gh = int(W * 0.62)
    l, d = cv.layer(); top = dim(dusk(mix(sp['s0'], (40, 70, 50), 0.62), minor), 0.9); bot = dim(dusk(mix(sp['s1'], (60, 80, 56), 0.7), minor), 0.9)
    if P['sp_name'] == 'autumn': top = mix(top, (110, 64, 30), .4); bot = mix(bot, (150, 96, 40), .35)
    if P['sp_name'] == 'winter': top = mix(top, (110, 124, 150), .5); bot = mix(bot, (200, 208, 222), .5)
    grad(d, 0, gh, top, bot)
    for i in range(7):                                            # trunks, far to near
        x = (0.08 + i * 0.14 + 0.03 * math.sin(i * 3)) * W; wd = (6 + (i % 3) * 5) * S; c = mix(bot, (30, 24, 20), 0.35 + 0.1 * (i % 3))
        d.rectangle([x - wd / 2, 0, x + wd / 2, gh + 10 * S], fill=c + (255,))
    cv.put(l)
    l, d = cv.layer()                                             # light shafts (HAZE): count by GRAIN, length by AIR, colour by TONE
    ns = int(lv[2] * (1 + 5 * (0.3 + m[6])))
    for i in range(ns):
        x0 = (0.1 + rnd.random() * 0.8) * W; w0 = (5 + 10 * rnd.random()) * S; L = (0.35 + 0.55 * m[8]) * W; sh = 0.22 * W
        col = mix((255, 240, 190), (255, 255, 255), m[7]); a = int((26 + 60 * lv[2]) * (0.5 + 0.5 * rnd.random()))
        d.polygon([(x0, 0), (x0 + w0, 0), (x0 + w0 + sh * L / W + 8 * S, L), (x0 + sh * L / W - 6 * S, L)], fill=col + (a,))
    cv.put(l)
    l, d = cv.layer()                                             # the ground: soil, moss mounds
    gc = dusk(sp['ground'], minor); 
    if P['sp_name'] == 'winter': gc = mix(gc, (236, 240, 248), .75)
    grad(d, gh, W, mix(gc, (20, 24, 18), .1), dim(gc, .55))
    for i in range(7):
        x = (0.1 + i * 0.14) * W; r = (14 + 10 * ((i * 5) % 3)) * S
        mc = mix((60, 110, 60), gc, .0 if P['sp_name'] in ('spring', 'summer') else .55)
        if P['sp_name'] == 'winter': mc = (232, 238, 246)
        d.ellipse([x - r, gh + 4 * S - r * .45, x + r, gh + 4 * S + r * .45], fill=mc + (255,))
    cv.put(l)
    l, d = cv.layer()                                             # mist along the ground (FLOOR): bands, count by OPEN, bend by SWAY
    nb = 2 + int(m[3] * 5)
    for i in range(nb):
        y = gh - (4 + 22 * (i / nb)) * S + (i % 2) * 6 * S; amp = (1 + 8 * m[5]) * S
        for j in range(5): wave(d, y + j * 2 * S, amp, 0.012 / S * 3, i * 2.1 + j * .3, mix((220, 230, 230), sp['light'], m[4]), (4 + 3 * (nb - i) / nb) * S, int((14 + 40 * lv[1]) * (0.4 + 0.6 * m[4])))
    cv.put(l)
    l, d = cv.layer()                                             # acorns falling, a ring of dust where they hit (PULSE)
    for i in range(1 + int(m[0] * 5)):
        x = (0.2 + 0.6 * rnd.random()) * W; y = gh + (0.1 + 0.8 * rnd.random()) * (W - gh) * 0.7; age = rnd.random(); sz = (3 + 5 * lv[0]) * S
        for k in range(1 + int(m[2] * 3)):
            ra = age * 0.7 + k * 0.2
            if ra < 1: ring(d, x, y, sz * (0.6 + 3.2 * ra), sz * (0.6 + 3.2 * ra) * .35, mix(sp['light'], (240, 220, 180), .5), 200 * lv[0] * (1 - ra) ** 1.5, 1.6 * S)
        d.ellipse([x - sz * .45, y - sz * .55, x + sz * .45, y + sz * .55], fill=(150, 100, 56, int(255 * lv[0] ** .5)))
        d.pieslice([x - sz * .55, y - sz * .8, x + sz * .55, y - sz * .1], 180, 360, fill=(104, 68, 40, int(255 * lv[0] ** .5)))
    cv.put(l)
    l, d = cv.layer()                                             # leaves (VOICE), leaning with the light-shaft's wind
    wsh = lv[2] * 16 * S
    for i in range(int(1 + lv[3] * 7)):
        t = rnd.random(); col = rnd.choice(sp['leaf']); col = mix(col, (255, 230, 120), .3 * m[10]) if P['sp_name'] != 'winter' else col
        x = (0.15 + 0.7 * rnd.random()) * W + wsh * t; y = (0.1 + 0.8 * t) * W * .85; sz = (6 + 6 * lv[3]) * S; ang = (rnd.random() - .5) * (0.5 + 5 * m[9]) + .5 * lv[2]
        if P['sp_name'] == 'winter': flake(d, x, y, sz * .55, col)
        else: leaf(d, x, y, ang, sz, col)
    for i in range(int(1 + lv[3] * 3)):
        x = (0.2 + 0.6 * rnd.random()) * W; y = gh + (0.2 + 0.6 * rnd.random()) * (W - gh) * .6; col = rnd.choice(sp['leaf'])
        d.line([(x - (4 + 30 * m[11]) * S, y + S), (x, y)], fill=sp['light'] + (int(70 * lv[3]),), width=S)   # TRAIL: rests on the moss, a while
        if P['sp_name'] == 'winter': flake(d, x, y, 5 * S, col)
        else: leaf(d, x, y, .1, (5 + 4 * lv[3]) * S, col)
    cv.put(l)
    return finish(cv)

def sea(P, st, rnd):
    cv = Cv(); sp, minor, lv, m = P['sp'], P['minor'], st['lv'], st['m']
    hz = int(W * 0.42); sh = int(W * 0.7)
    l, d = cv.layer(); s0, s1 = dusk(sp['s0'], minor), dusk(sp['s1'], minor)
    grad(d, 0, hz, s0, s1)
    sea0 = mix(sp['deep'], s1, .15 + .35 * m[4]); sea1 = dim(sp['deep'], .75)
    grad(d, hz, sh, sea0, sea1)
    sand = dusk(sp['sand'], minor); grad(d, sh, W, sand, dim(sand, .8)); cv.put(l)
    l, d = cv.layer()                                             # the sea (FLOOR): swell bands, OPEN how many, SWAY how big
    nb = 3 + int(m[3] * 5)
    for i in range(nb):
        t = (i + 1) / (nb + 1); y = hz + (sh - hz) * t ** 1.4
        wave(d, y, (1 + 6 * m[5]) * S * (.4 + t), 0.015 / S * 3, i * 1.3, mix(sea0, sp['light'], .15 + .25 * m[4]), (0.8 + 1.8 * t) * S, int(25 + 70 * lv[1]))
    for i in range(int(lv[1] * (3 + 30 * m[4] ** 2))):
        y = hz + rnd.random() ** 1.5 * (sh - hz - 6 * S); x = rnd.random() * W
        d.line([(x, y), (x + 7 * S, y)], fill=sp['light'] + (int(60 + 120 * m[4]),), width=S)
    cv.put(l)
    l, d = cv.layer()                                             # waves (PULSE): a foam line comes up the beach at each beat: count by DENS, how far it spreads by WASH
    for i in range(1 + int(m[0] * 4)):
        age = rnd.random(); y = sh - (4 - 2 * age) * S - i * 5 * S * (1 + m[2] * 1.5)
        amp = (2 + 3 * lv[0]) * S
        for k in range(1 + int(m[2] * 2)):
            yy = y + k * 7 * S + age * 14 * S * (0.4 + m[2]); a = int(230 * lv[0] * (1 - age * .7) / (1 + k))
            wave(d, yy, amp, 0.02 / S * 3, i * 2 + k, sp['light'], (2 + 2 * lv[0]) * S * (1 - k * .25), a, x0=.1 * W, x1=.9 * W)
    d.polygon([(0, sh), (W, sh), (W, sh + 5 * S), (0, sh + 5 * S)], fill=dim(sand, .9) + (60,))
    cv.put(l)
    l, d = cv.layer()                                             # wind: clouds streaking, sand blowing along the beach
    for i in range(int(lv[2] * (2 + 4 * (.3 + m[6])))):
        y = (.1 + .25 * rnd.random() + (1 - m[8]) * .08) * W; x = (rnd.random() * .7 - .1) * W; L = (.15 + .4 * m[8]) * W
        stroke(d, (x, y), (x + L, y + (rnd.random() - .5) * 14 * S), (rnd.random() - .5) * 14 * S, mix(sp['light'], (255, 255, 255), m[7]), (2 + 3 * lv[2]) * S, 140 * lv[2], gap=.4 * m[6], rnd=rnd)
    for i in range(int(lv[2] * (2 + 5 * m[6]))):
        y = sh + (.15 + .7 * rnd.random()) * (W - sh) * .9; x = (rnd.random() * .6) * W; L = (.12 + .25 * m[8]) * W
        stroke(d, (x, y), (x + L, y + 2 * S), (rnd.random() - .5) * 5 * S, mix(sand, (255, 255, 255), .5), 2 * S, 130 * lv[2], gap=.6 * m[6], rnd=rnd)
    cv.put(l)
    l, d = cv.layer()                                             # birds (VOICE): one per note; the pitch is the height; LOOSE how they swing; TRAIL how long they linger as a streak
    for i in range(int(1 + lv[3] * 5)):
        x = (.15 + .7 * rnd.random()) * W + lv[2] * 12 * S * rnd.random(); y = (.1 + .6 * rnd.random()) * hz * 1.5; sz = (7 + 7 * lv[3]) * S
        col = (250, 250, 252) if P['sp_name'] != 'autumn' else (255, 244, 230); tilt = (rnd.random() - .5) * (.3 + 2.5 * m[9])
        d.line([(x - sz * 1.3, y - sz * .2 + sz * .5 * math.sin(tilt)), (x, y + sz * .15), (x + sz * 1.3, y - sz * .2 - sz * .5 * math.sin(tilt))], fill=col + (255,), width=3 * S, joint='curve')
        d.line([(x - (8 + 50 * m[11]) * S, y + 2 * S), (x - sz, y)], fill=sp['light'] + (int(60 * lv[3]),), width=S)
    cv.put(l)
    return finish(cv)

THEMES = [('WATER', water), ('FOREST', forest), ('SEA', sea)]
CALM = dict(lv=[.55, .6, .35, .5], m=[.45, .5, .4, .4, .5, .4, .4, .5, .45, .4, .5, .45])
BUSY = dict(lv=[.95, .9, .85, .95], m=[.9, .8, .85, .85, .85, .85, .9, .85, .8, .9, .8, .85])
THIN = dict(lv=[.18, .35, .1, .25], m=[.15, .3, .15, .2, .25, .1, .1, .3, .2, .1, .3, .15])

def render(fn, season, st, minor=False, seed=7):
    P = dict(sp=SEASON[season], sp_name=season, minor=minor)
    return fn(P, st, random.Random(seed))

def sheet(rows, cols, tiles, labels_r, labels_c, path):
    from PIL import ImageFont
    pad = 6; lw = 70; th = 22
    im = Image.new('RGB', (lw + cols * (N + pad) + pad, th + rows * (N + pad) + pad), (30, 30, 34)); d = ImageDraw.Draw(im)
    try: f = ImageFont.load_default(14)
    except TypeError: f = ImageFont.load_default()
    for c, t in enumerate(labels_c): d.text((lw + pad + c * (N + pad) + 6, 4), t, fill=(220, 220, 225), font=f)
    for r, t in enumerate(labels_r): d.text((6, th + pad + r * (N + pad) + N // 2), t, fill=(220, 220, 225), font=f)
    for i, t in enumerate(tiles): im.paste(t, (lw + pad + (i % cols) * (N + pad), th + pad + (i // cols) * (N + pad)))
    im.save(path); print(path, im.size)

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'docs/shots'; os.makedirs(out, exist_ok=True)
    seasons = ['spring', 'summer', 'autumn', 'winter']
    tiles = [render(fn, s, CALM, minor=(s == 'autumn')) for _, fn in THEMES for s in seasons]
    sheet(3, 4, tiles, [n for n, _ in THEMES], ['SPRING  C..D', 'SUMMER  D#..F', 'AUTUMN  F#..G# (minor)', 'WINTER  A..B'], os.path.join(out, 'mk_scene_seasons.png'))
    tiles = [render(fn, 'summer', st, seed=sd) for _, fn in THEMES for st, sd in ((THIN, 7), (CALM, 7), (BUSY, 7), (BUSY, 11))]
    sheet(3, 4, tiles, [n for n, _ in THEMES], ['THIN (quiet)', 'CALM', 'BUSY', 'BUSY (other seed)'], os.path.join(out, 'mk_scene_states.png'))
