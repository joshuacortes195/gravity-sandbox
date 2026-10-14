// what the compiled physics module hands back to the page
export interface Sim {
  HEAPF32: Float32Array;
  _load_disk(n: number, seed: number, eps: number, theta: number): void;
  _advance(steps: number, dt: number, eps: number, theta: number): void;
  _body_count(): number;
  _pos_x(): number;
  _pos_y(): number;
  _pos_z(): number;
}

// loads the .wasm file and resolves once the module is ready
export default function createSim(): Promise<Sim>;
