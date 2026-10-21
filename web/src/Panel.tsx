import { useEffect, useState, useSyncExternalStore } from 'react';
import { PRESETS, START_EFFECTS, START_PRESET, START_THETA, TOUCH, defaultN } from './presets';
import { getReadout, subscribe } from './store';

// what the panel can ask the sim to do, handed in by main.ts
export interface Actions {
  load(preset: number, n: number): void;
  setPaused(paused: boolean): void;
  setSpeed(speed: number): void;
  setTheta(theta: number): void;
  setGlow(on: boolean): void;
  setLensing(on: boolean): void;
  setTrails(on: boolean): void;
  setSpeedColors(on: boolean): void;
}

// the looks the panel can switch on and off, with the label each one shows
const EFFECTS = [
  { key: 'glow', label: 'glow' },
  { key: 'trails', label: 'trails' },
  { key: 'speedColors', label: 'color by speed' },
  { key: 'lensing', label: 'black hole lensing and disk' },
] as const;
type Effect = (typeof EFFECTS)[number]['key'];

// the speed slider steps through these, as multiples of normal speed
const SPEEDS = [0.25, 0.5, 1, 2, 4, 8];

// where the code lives, linked from the panel header
const SOURCE = 'https://github.com/joshuacortes195/gravity-sandbox';

// the live numbers, the only part that re-renders twice a second
function Readouts() {
  const r = useSyncExternalStore(subscribe, getReadout);
  return (
    <dl className="readouts">
      <dt>bodies</dt>
      <dd>{r.bodies}</dd>
      <dt>fps</dt>
      <dd>{r.fps.toFixed(0)}</dd>
      <dt>physics</dt>
      <dd>{r.msPerStep.toFixed(2)} ms/step</dd>
      <dt>build</dt>
      <dd>{r.build}</dd>
      <dt>threads</dt>
      <dd>{r.threads}</dd>
      <dt>steps/s</dt>
      <dd>{r.stepsPerSecond.toFixed(0)}</dd>
      <dt>sim time</dt>
      <dd>{r.simTime.toFixed(2)}</dd>
      <dt>energy drift</dt>
      <dd>{r.drift === null ? 'measuring' : r.drift.toExponential(1)}</dd>
    </dl>
  );
}

// the control panel in the corner: scene picker, sliders and the readouts
export function Panel({ actions }: { actions: Actions }) {
  const [preset, setPreset] = useState(START_PRESET);
  const [n, setN] = useState(defaultN(START_PRESET));
  const [paused, setPaused] = useState(false);
  // phones start with the panel folded up so the sim gets the whole screen
  const [open, setOpen] = useState(!TOUCH);
  const [speedIndex, setSpeedIndex] = useState(2);
  const [theta, setTheta] = useState(START_THETA);
  const [effects, setEffects] = useState(START_EFFECTS);
  const p = PRESETS[preset];

  // flips one look on or off in both the panel and the renderer
  function toggleEffect(key: Effect) {
    const on = !effects[key];
    setEffects({ ...effects, [key]: on });
    if (key === 'glow') actions.setGlow(on);
    else if (key === 'lensing') actions.setLensing(on);
    else if (key === 'trails') actions.setTrails(on);
    else actions.setSpeedColors(on);
  }

  // flips pause in both the panel and the sim
  function togglePause() {
    setPaused(!paused);
    actions.setPaused(!paused);
  }

  // space bar pauses and plays, unless a control has focus and wants the key itself (a checkbox ticks with space)
  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      const t = e.target;
      const wantsSpace = t instanceof HTMLSelectElement || t instanceof HTMLButtonElement || (t instanceof HTMLInputElement && t.type === 'checkbox');
      if (e.code !== 'Space' || wantsSpace) return;
      e.preventDefault();
      togglePause();
    }
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  // switching scenes puts the body count back to that scene's default
  function pickPreset(i: number) {
    const count = defaultN(i);
    setPreset(i);
    setN(count);
    actions.load(i, count);
  }

  // a new body count rebuilds the scene from scratch
  function pickN(count: number) {
    setN(count);
    actions.load(preset, count);
  }

  return (
    <div className="panel">
      {/* title, a link to the code and the button that folds the panel away */}
      <header>
        <h1>Gravity Sandbox</h1>
        <a href={SOURCE} target="_blank" rel="noreferrer">
          code
        </a>
        <button onClick={() => setOpen(!open)} aria-expanded={open}>
          {open ? 'hide' : 'show'}
        </button>
      </header>

      {/* everything under the header, left out while the panel is folded */}
      {open ? (
        <>
          {/* scene picker */}
          <label>
            scene
            <select value={preset} onChange={(e) => pickPreset(Number(e.target.value))}>
              {PRESETS.map((s, i) => (
                <option key={s.name} value={i}>
                  {s.name}
                </option>
              ))}
            </select>
          </label>

          {/* pause and restart */}
          <div className="buttons">
            <button onClick={togglePause}>{paused ? 'play' : 'pause'}</button>
            <button onClick={() => actions.load(preset, n)}>restart</button>
          </div>

          {/* how many physics steps run per second, the timestep itself never changes */}
          <label>
            speed {SPEEDS[speedIndex]}x
            <input
              type="range"
              min={0}
              max={SPEEDS.length - 1}
              value={speedIndex}
              onChange={(e) => {
                const i = Number(e.target.value);
                setSpeedIndex(i);
                actions.setSpeed(SPEEDS[i]);
              }}
            />
          </label>

          {/* tree accuracy, smaller is more exact and slower, bigger is faster and rougher */}
          {/* stops at 0.2 because 0 is brute force, and one step of that at 10,000 stars froze the page for seconds */}
          <label>
            theta {theta.toFixed(2)}
            <input
              type="range"
              min={0.2}
              max={1.2}
              step={0.05}
              value={theta}
              onChange={(e) => {
                const t = Number(e.target.value);
                setTheta(t);
                actions.setTheta(t);
              }}
            />
          </label>

          {/* body count, only for scenes that are made of many stars */}
          {p.n !== null ? (
            <label>
              {preset === 2 ? 'stars per galaxy' : 'stars'} {n}
              <input type="range" min={100} max={p.nMax} step={100} value={n} onChange={(e) => pickN(Number(e.target.value))} />
            </label>
          ) : null}

          {/* switches for the looks, none of them touch the physics */}
          <div className="effects">
            {/* lensing only means something on a scene with a black hole */}
            {EFFECTS.filter((e) => e.key !== 'lensing' || p.lightSpeed !== undefined).map((e) => (
              <label key={e.key}>
                <input type="checkbox" checked={effects[e.key]} onChange={() => toggleEffect(e.key)} />
                {e.label}
              </label>
            ))}
          </div>

          <Readouts />
          <p className="hint">
            {TOUCH ? 'drag to turn, pinch to zoom, tap to drop in a body' : 'drag to turn, scroll to zoom, click to drop in a body, space to pause'}
          </p>
        </>
      ) : TOUCH ? (
        // phones start folded, so the one thing worth knowing stays visible under the header
        <p className="hint">tap to drop in a body</p>
      ) : null}
    </div>
  );
}
