// draws the black hole over the finished picture: traces light near the hole, bends it by a formula far away,
// and adds a glowing gas disk that is only a picture, never part of the sim (see physics-notes, drawing the black hole)
// ported from my event-horizon project. units are G = c = 1 and M = 1, so every length here is in M
uniform sampler2D tDiffuse;
uniform vec3 camPos;    // camera position from the hole, in M, world axes
uniform mat3 camRot;    // turns camera space directions into world directions
uniform vec2 tanHalf;   // tan of half the view across and up
uniform mat4 viewProj;  // world to screen, used to find where an escaped ray's light sits in the picture
uniform float bTrace;   // rays passing closer than this get traced, the rest use the formula
uniform float outer;    // disk outer edge
uniform float peak;     // hottest ring temperature in kelvin
uniform float time;     // how far the gas has turned, in M (light crossing times of the hole)
varying vec2 vUv;

const float ESCAPE_R = 1e5;
const float STEP_FRAC = 0.01;
const float MAX_STEP_FRAC = 0.1;
// a ray heading out stops once the bend it has left, (3/4) b^3 / r^4, is under this many radians
const float BEND_TOL = 1e-4;
// about six laps of the photon sphere, anything still going counts as falling in
const int MAX_STEPS = 4000;

// the pull of gravity on light, -3M h^2 x / r^5, h is the ray's angular momentum
vec3 accel(vec3 pos, vec3 vel) {
  vec3 h = cross(pos, vel);
  float r2 = dot(pos, pos);
  return (-3.0 * dot(h, h) / (r2 * r2 * sqrt(r2))) * pos;
}

// one rk4 step: samples the pull four times across the step and blends them
void rk4_step(inout vec3 pos, inout vec3 vel, float dl) {
  vec3 p1 = pos, v1 = vel, a1 = accel(p1, v1);
  vec3 p2 = p1 + 0.5 * dl * v1, v2 = v1 + 0.5 * dl * a1, a2 = accel(p2, v2);
  vec3 p3 = p1 + 0.5 * dl * v2, v3 = v1 + 0.5 * dl * a2, a3 = accel(p3, v3);
  vec3 p4 = p1 + dl * v3, v4 = v1 + dl * a3, a4 = accel(p4, v4);
  pos = p1 + (dl / 6.0) * (v1 + 2.0 * v2 + 2.0 * v3 + v4);
  vel = v1 + (dl / 6.0) * (a1 + 2.0 * a2 + 2.0 * a3 + a4);
}

// thin disk heating, T^4 goes as r^-3 (1 - sqrt(6 / r)) with the isco at 6M
float heating(float r) { return (1.0 - sqrt(6.0 / r)) / (r * r * r); }

// thin disk temperature, scaled so the ring at 49/36 of the isco reads peak kelvin
float disk_temperature(float r) {
  return peak * pow(max(heating(r), 0.0) / heating(49.0 / 36.0 * 6.0), 0.25);
}

// one bump of the fitted cie color matching curves (wyman, sloan and shirley 2013)
float lobe(float nm, float a, float b, float g, float d) {
  float t = (nm - b) * (nm < b ? g : d);
  return a * exp(-0.5 * t * t);
}

// color of a blackbody at temp kelvin as cie xyz, walking the visible spectrum in 10 nm slices
vec3 blackbody_xyz(float temp) {
  if (temp <= 0.0) return vec3(0.0);
  vec3 xyz = vec3(0.0);
  for (int i = 0; i <= 40; ++i) {
    float nm = 380.0 + 10.0 * float(i);
    // planck's law, wavelength in micrometers
    float um = nm / 1000.0;
    // past e^80 this slice adds nothing, and skipping it keeps exp from overflowing on cold rings
    float e = 14387.77 / (um * temp);
    if (e > 80.0) continue;
    float planck = 1.0 / (um * um * um * um * um * (exp(e) - 1.0));
    // how much of each cie primary this wavelength adds
    float x = lobe(nm, 0.362, 442.0, 0.0624, 0.0374) + lobe(nm, 1.056, 599.8, 0.0264, 0.0323) +
              lobe(nm, -0.065, 501.1, 0.0490, 0.0382);
    float y = lobe(nm, 0.821, 568.8, 0.0213, 0.0247) + lobe(nm, 0.286, 530.9, 0.0613, 0.0322);
    float z = lobe(nm, 1.217, 437.0, 0.0845, 0.0278) + lobe(nm, 0.681, 459.0, 0.0385, 0.0725);
    xyz += planck * vec3(x, y, z);
  }
  return xyz;
}

// how shifted the disk's light is when it reaches the camera, above 1 bluer and brighter.
// climbing out of the well, the gas's slow clock and its push on the light as it orbits at r^-1.5,
// the sqrt(1 - 2M / r) from the climb and the clock cancel
float disk_g(float r, float L, float rCam) {
  return sqrt(1.0 - 3.0 / r) / sqrt(1.0 - 2.0 / rCam) / (1.0 + pow(r, -1.5) * L);
}

// blackbody glow as plain linear light, the hottest ring with no shifts reads 1
vec3 disk_linear(float temp) {
  vec3 c = blackbody_xyz(temp) / blackbody_xyz(peak).y;
  // cie xyz to linear srgb
  return max(vec3(3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z,
                  -0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z,
                  0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z), 0.0);
}

// cheap random numbers from a position, no sin so it acts the same on every gpu
vec3 hash33(vec3 p) {
  p = fract(p * vec3(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return fract((p.xxy + p.yxx) * p.zyx);
}

// made up faint stars, only for light that comes from off the edge of the picture
vec3 star_sky(vec3 dir) {
  vec3 cell = floor(dir * 150.0);
  vec3 h = hash33(cell);
  if (h.x > 0.05) return vec3(0.0);
  vec3 star = normalize(cell + 0.25 + 0.5 * hash33(cell + 13.0)) * 150.0;
  float d = length(dir * 150.0 - star);
  return exp(-d * d * 40.0) * vec3(0.18, 0.2, 0.25);
}

// smooth random bumps in 3d, blends the random values at the 8 corners of each cell
float noise3(vec3 p) {
  vec3 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = mix(hash33(i).x, hash33(i + vec3(1, 0, 0)).x, f.x);
  float b = mix(hash33(i + vec3(0, 1, 0)).x, hash33(i + vec3(1, 1, 0)).x, f.x);
  float c = mix(hash33(i + vec3(0, 0, 1)).x, hash33(i + vec3(1, 0, 1)).x, f.x);
  float d = mix(hash33(i + vec3(0, 1, 1)).x, hash33(i + vec3(1, 1, 1)).x, f.x);
  return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}

// stacks 5 sizes of bumps, big soft ones plus finer and finer detail
float fbm(vec3 p) {
  float sum = 0.0, amp = 0.5;
  for (int i = 0; i < 5; ++i) {
    sum += amp * noise3(p);
    p = p * 2.03 + 17.1;
    amp *= 0.5;
  }
  return sum;
}

// how thick the gas is at this spot on the disk, 0 is a gap and 1 is solid
float gas(vec3 hit, float r) {
  // each ring has turned by its own orbit speed r^-1.5, so inner gas winds ahead and the clumps shear into spirals
  float a = atan(hit.x, hit.z) - pow(r, -1.5) * time;
  // laid out by angle and log radius, long streaks along the orbit and thin lanes across it
  vec3 p = vec3(cos(a) * 2.5, sin(a) * 2.5, log(r) * 9.0);
  float n = fbm(p + vec3(0.0, 0.0, fbm(p * 0.7) * 2.0));
  // fades in past the isco and out toward the outer edge so the disk has no hard rim
  float edge = smoothstep(6.0, 7.5, r) * smoothstep(outer, outer * 0.55, r);
  return edge * smoothstep(0.25, 0.75, n);
}

// film style tone curve (narkowicz's aces fit) for the gas, bright light rolls toward white instead of clipping.
// no srgb step here, the output pass at the end does that for the whole picture
vec3 aces(vec3 c) {
  return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

// the tracer keeps the disk in its own y = 0 plane, the scene's orbits lie in the world's z = 0 plane
vec3 toTrace(vec3 w) { return vec3(w.x, w.z, -w.y); }
vec3 toWorld(vec3 t) { return vec3(t.x, -t.z, t.y); }

// what the camera sees looking along a world direction: the finished picture, or faint stars if that's off screen.
// treats everything as far behind the hole, which is the approximation this whole pass makes
vec3 seen(vec3 dir) {
  vec4 clip = viewProj * vec4(dir, 0.0);
  vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
  bool onScreen = clip.w > 0.0 && all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)));
  return onScreen ? texture2D(tDiffuse, clamp(uv, 0.0, 1.0)).rgb : star_sky(dir);
}

void main() {
  // this pixel's direction in the world
  vec3 d = normalize(camRot * vec3((vUv * 2.0 - 1.0) * tanHalf, -1.0));
  // a camera holding still near the hole sees the sky squeezed sideways, so the sideways part gets stretched
  float rCam = length(camPos);
  vec3 out_ = camPos / rCam;
  vec3 v = dot(d, out_) * out_ + (d - dot(d, out_) * out_) / sqrt(1.0 - 2.0 / rCam);
  // how close the ray would pass the hole, it never changes along the way
  float b = length(cross(camPos, v));

  // far out the bend is small and a formula is good to under a pixel: the pull of 3M b^2 / r^4 added up along a
  // straight line from here on, (2 - 3s + s^3) / b where s is how much the ray already points away from the hole.
  // from far away that's the full 4M / b. the ray gets turned toward the hole by it
  if (b > bTrace) {
    vec3 u = normalize(v);
    float s = dot(out_, u);
    float bend = (2.0 - 3.0 * s + s * s * s) / b;
    vec3 toward = normalize(dot(out_, u) * u - out_);
    gl_FragColor = vec4(seen(cos(bend) * u + sin(bend) * toward), 1.0);
    return;
  }

  // near the hole, follow the ray until it falls in, escapes or the gas in front of it is solid
  vec3 pos = toTrace(camPos), vel = toTrace(v);
  // angular momentum about the disk's axis never changes, so the disk's doppler shift can use it
  float L = cross(pos, vel).y;
  float leftover_bend = 0.75 * b * b * b;
  vec3 color = vec3(0.0);
  vec3 glow = vec3(0.0);
  // how much light from further along the ray still gets through the gas
  float through = 1.0;
  for (int i = 0; i < MAX_STEPS; ++i) {
    float r = length(pos);
    // inside the horizon, the ray is gone and adds nothing more
    if (r < 2.0) break;
    // heading out and far away, or past the disk with almost no bend left, so look up what it points at
    float r2 = r * r;
    if (dot(pos, vel) > 0.0 && (r > ESCAPE_R || (r > outer && leftover_bend / r2 < BEND_TOL * r2))) {
      color = through * seen(toWorld(normalize(vel)));
      break;
    }
    vec3 prev = pos;
    // 1% of r steps, bigger once the ray is past b and nearly straight, never over 10% of r
    rk4_step(pos, vel, min(STEP_FRAC * max(1.0, r / max(b, 1e-3)), MAX_STEP_FRAC) * r);
    // crossed the disk plane this step, the ray picks up the gas glow there and gets dimmer, then keeps going
    if (prev.y != 0.0 && (prev.y > 0.0) != (pos.y > 0.0)) {
      vec3 hit = mix(prev, pos, prev.y / (prev.y - pos.y));
      float hit_r = length(hit);
      if (hit_r >= 6.0 && hit_r <= outer) {
        float thick = gas(hit, hit_r);
        glow += through * thick * disk_linear(disk_g(hit_r, L, rCam) * disk_temperature(hit_r));
        through *= 1.0 - 0.85 * thick;
        // hardly anything gets through anymore, stop here
        if (through < 0.02) break;
      }
    }
  }
  // half brightness, since the scene's own glow pass comes after and brightens it again
  gl_FragColor = vec4(color + aces(0.5 * glow), 1.0);
}
