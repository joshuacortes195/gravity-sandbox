// what the compiled physics module hands back to the page
export interface Sim {
  HEAPF32: Float32Array;
  _load_scene(which: number, n: number, seed: number, eps: number, theta: number): void;
  _spawn(x: number, y: number, z: number, vx: number, vy: number, vz: number, m: number, eps: number, theta: number): void;
  _advance(steps: number, dt: number, eps: number, theta: number): void;
  _body_count(): number;
  _pos_x(): number;
  _pos_y(): number;
  _pos_z(): number;
  _mass(): number;
  _energy_begin(): void;
  _energy_work(rows: number, eps: number): number;
  _energy_value(): number;
}

// loads the .wasm file and resolves once the module is ready
export default function createSim(): Promise<Sim>;
