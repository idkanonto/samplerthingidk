import { type CSSProperties, type MouseEvent, type PointerEvent as ReactPointerEvent, type ReactNode, type WheelEvent, useEffect, useMemo, useRef, useState } from 'react'
import {
  postDroppedFiles, sendPluginCommand, type SampleSummary, useBackendState,
  usePluginParameter, useVisualisationState
} from './juceBridge'
import { FaultWaves, NeuralBackground, SpectralDrawCanvas, StereoMeterCanvas, WaveformCanvas } from './VisualCanvases'
import productLogo from './assets/recompiler-logo.svg'
import damnnprodigyLogo from './assets/damnnprodigy-logo.svg'
import shadx2Logo from './assets/shadx2-logo.svg'
import closeIcon from './assets/close-icon.svg'

declare const __RECOMPILER_VERSION__: string
declare const __RECOMPILER_BUILD_ID__: string

const KNOB_TRAVEL_PX = 180
const FINE_DRAG_DIVISOR = 6

function HatchFill() { return <span className="hatch-fill" aria-hidden="true" /> }
function Divider() { return <span className="divider" aria-hidden="true" /> }

function PixelButton({ children, active = false, className = '', onClick, title, disabled = false }: {
  children: ReactNode, active?: boolean, className?: string, disabled?: boolean,
  onClick?: (event: MouseEvent<HTMLButtonElement>) => void, title?: string
}) {
  return <button type="button" disabled={disabled} title={title}
    className={`pixel-button ${active ? 'active' : ''} ${className}`} onClick={onClick}>{children}</button>
}

function PixelToggle({ id, left, right }: { id: string, left: string, right: string }) {
  const parameter = usePluginParameter(id)
  const set = (value: number) => { parameter.beginGesture(); parameter.setValue(value); parameter.endGesture() }
  return <div className="pixel-toggle" role="group" aria-label={parameter.descriptor?.name ?? id}>
    <PixelButton active={parameter.value < .5} onClick={() => set(0)}>{left}</PixelButton>
    <PixelButton active={parameter.value >= .5} onClick={() => set(1)}>{right}</PixelButton>
  </div>
}

function EditableValue({ label, value, min, max, step, digits = 0, suffix = '', disabled = false, format, onCommit }: {
  label: string, value: number, min: number, max: number, step: number, digits?: number, suffix?: string,
  disabled?: boolean, format?: (value: number) => string, onCommit: (value: number) => void
}) {
  const [editing, setEditing] = useState(false)
  const [draft, setDraft] = useState(value.toFixed(digits))
  const input = useRef<HTMLInputElement>(null)
  const skipNextBlur = useRef(false)
  useEffect(() => { if (!editing) setDraft(value.toFixed(digits)) }, [digits, editing, value])
  const clamp = (next: number) => Math.max(min, Math.min(max, Number(next.toFixed(Math.max(digits, 3)))))
  const begin = () => {
    if (disabled || editing) return
    setEditing(true); setDraft(value.toFixed(digits))
    requestAnimationFrame(() => input.current?.select())
  }
  const commit = (candidate = draft) => {
    const parsed = Number(candidate)
    const next = clamp(Number.isFinite(parsed) ? parsed : value)
    onCommit(next); setDraft(next.toFixed(digits)); setEditing(false)
  }
  const cancel = () => { skipNextBlur.current = true; setDraft(value.toFixed(digits)); setEditing(false); input.current?.blur() }
  const nudge = (direction: number, fine: boolean) => {
    const current = Number(editing ? draft : value)
    const increment = step * (fine ? .1 : 1)
    const next = clamp((Number.isFinite(current) ? current : value) + direction * increment)
    setEditing(true); setDraft(next.toFixed(digits))
  }
  return <span className={`editable-value ${editing ? 'editing' : ''} ${disabled ? 'disabled' : ''}`}>
    <input ref={input} aria-label={label} inputMode="decimal" disabled={disabled} readOnly={!editing}
      value={editing ? draft : (format?.(value) ?? value.toFixed(digits))}
      onClick={begin} onFocus={begin} onChange={(event) => setDraft(event.target.value)}
      onBlur={() => { if (skipNextBlur.current) skipNextBlur.current = false; else if (editing) commit() }} onKeyDown={(event) => {
        if (event.key === 'Enter') { event.preventDefault(); commit() }
        else if (event.key === 'Escape') { event.preventDefault(); cancel() }
        else if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
          event.preventDefault(); nudge(event.key === 'ArrowUp' ? 1 : -1, event.shiftKey)
        }
      }} />
    {suffix && <span className="value-unit">{suffix}</span>}
  </span>
}

function RotaryKnob({ label, value, min, max, step, defaultValue, disabled = false, onBegin, onSet, onEnd }: {
  label: string, value: number, min: number, max: number, step: number, defaultValue: number,
  disabled?: boolean, onBegin: () => void, onSet: (value: number) => void, onEnd: () => void
}) {
  const drag = useRef<{ pointerId: number, startY: number, startValue: number } | null>(null)
  const clamped = Math.max(min, Math.min(max, value))
  const percent = (clamped - min) / Math.max(.001, max - min)
  const quantise = (next: number) => {
    const snapped = step > 0 ? min + Math.round((next - min) / step) * step : next
    return Math.max(min, Math.min(max, Number(snapped.toFixed(6))))
  }
  const begin = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (disabled || event.button !== 0) return
    event.preventDefault(); event.currentTarget.setPointerCapture(event.pointerId)
    drag.current = { pointerId: event.pointerId, startY: event.clientY, startValue: clamped }
    onBegin()
  }
  const move = (event: ReactPointerEvent<HTMLDivElement>) => {
    const active = drag.current
    if (!active || active.pointerId !== event.pointerId) return
    const divisor = event.shiftKey ? FINE_DRAG_DIVISOR : 1
    onSet(quantise(active.startValue + (active.startY - event.clientY) * (max - min) / (KNOB_TRAVEL_PX * divisor)))
  }
  const finish = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!drag.current || drag.current.pointerId !== event.pointerId) return
    drag.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId)
    onEnd()
  }
  const wheel = (event: WheelEvent<HTMLDivElement>) => {
    if (disabled) return
    event.preventDefault()
    const increment = step > 0 ? step : (max - min) / 100
    onBegin(); onSet(quantise(clamped + (event.deltaY < 0 ? increment : -increment) * (event.shiftKey ? .2 : 1))); onEnd()
  }
  return <div className={`pixel-knob ${disabled ? 'disabled' : ''}`} role="presentation" onPointerDown={begin} onPointerMove={move}
      onPointerUp={finish} onPointerCancel={finish} onWheel={wheel}
      onDoubleClick={() => { if (!disabled) { onBegin(); onSet(defaultValue); onEnd() } }}>
      <span className="knob-pointer" style={{ '--knob-angle': `${-135 + percent * 270}deg` } as CSSProperties} />
      <input aria-label={label} type="range" min={min} max={max} step={step || .1} value={clamped} disabled={disabled}
        onKeyDown={onBegin} onKeyUp={onEnd} onChange={(event) => onSet(Number(event.target.value))} />
  </div>
}

function PressureControl({ id, accessibleLabel, label = 'PRESSURE' }: { id: string, accessibleLabel: string, label?: string }) {
  const parameter = usePluginParameter(id)
  const descriptor = parameter.descriptor
  const min = descriptor?.min ?? 0; const max = descriptor?.max ?? 100; const step = descriptor?.interval || 1
  const commit = (value: number) => { parameter.beginGesture(); parameter.setValue(value); parameter.endGesture() }
  return <div className="pressure-control"><b className="control-label">{label}</b>
    <RotaryKnob label={accessibleLabel} value={parameter.value} min={min} max={max} step={step}
      defaultValue={descriptor?.defaultValue ?? 0} onBegin={parameter.beginGesture} onSet={parameter.setValue} onEnd={parameter.endGesture} />
    <EditableValue label={`${accessibleLabel} exact value`} value={parameter.value} min={min} max={max} step={step}
      onCommit={commit} />
  </div>
}

function ModuleHeader({ children, action }: { children: ReactNode, action?: ReactNode }) {
  return <div className="module-header"><strong>{children}</strong><HatchFill />{action}</div>
}

function RecompilerPanel({ title, children, className = '', headerAction }: {
  title?: string, children: ReactNode, className?: string, headerAction?: ReactNode
}) {
  return <section className={`recompiler-panel ${className}`}>{title && <ModuleHeader action={headerAction}>{title}</ModuleHeader>}{children}</section>
}

function EffectPower({ effect, enabled }: { effect: number, enabled: boolean }) {
  return <button type="button" className={`effect-power ${enabled ? 'active' : ''}`}
    aria-label={`${enabled ? 'Disable' : 'Enable'} effect`} aria-pressed={enabled}
    onClick={() => sendPluginCommand('setEffectEnabled', { effect, enabled: !enabled })} />
}

function ActionButton({ children, className = '', onClick }: { children: ReactNode, className?: string, onClick: (event: MouseEvent<HTMLButtonElement>) => void }) {
  return <button type="button" className={`action-button ${className}`} onClick={onClick}>{children}</button>
}

function PixelDisplay({ children, className = '' }: { children: ReactNode, className?: string }) {
  return <div className={`pixel-display crt-window ${className}`}>{children}</div>
}

function BypassOverlay({ enabled }: { enabled: boolean }) {
  return enabled ? null : <div className="bypass-overlay" role="status">BYPASSED</div>
}

function SampleRow({ sample, selected }: { sample: SampleSummary, selected: boolean }) {
  const select = () => sendPluginCommand('selectSample', { id: sample.id })
  return <div className={`sample-row ${selected ? 'selected' : ''} ${sample.enabled ? '' : 'sample-disabled'}`} role="button" aria-pressed={selected} tabIndex={0} onClick={select}
    onKeyDown={(event) => { if (event.target === event.currentTarget && (event.key === 'Enter' || event.key === ' ')) { event.preventDefault(); select() } }}>
    <button className={`pixel-check ${sample.enabled ? 'checked' : ''}`} aria-pressed={sample.enabled} aria-label={`${sample.enabled ? 'Disable' : 'Enable'} ${sample.name}`}
      onClick={(event) => { event.stopPropagation(); sendPluginCommand('setSampleEnabled', { id: sample.id, enabled: !sample.enabled }) }}>
      <span aria-hidden="true" />
    </button>
    <span className={sample.missing ? 'missing' : ''}>{sample.name}</span>
    <button type="button" className="remove-button" onClick={(event) => { event.stopPropagation(); sendPluginCommand('removeSample', { id: sample.id }) }}>REMOVE</button>
  </div>
}

function Waveform({ sample }: { sample?: SampleSummary }) {
  const [region, setRegion] = useState({ start: sample?.start ?? 0, end: sample?.end ?? 1 })
  const dragging = useRef<'start' | 'end' | null>(null)
  const minimumRegion = .002
  useEffect(() => { setRegion({ start: sample?.start ?? 0, end: sample?.end ?? 1 }); dragging.current = null }, [sample?.id, sample?.start, sample?.end])
  const position = (event: ReactPointerEvent<HTMLDivElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect()
    return Math.max(0, Math.min(1, (event.clientX - bounds.left) / Math.max(1, bounds.width)))
  }
  const move = (next: number, handle: 'start' | 'end') => setRegion((current) => handle === 'start'
    ? { ...current, start: Math.min(next, current.end - minimumRegion) }
    : { ...current, end: Math.max(next, current.start + minimumRegion) })
  const begin = (event: ReactPointerEvent<HTMLDivElement>) => {
    const next = position(event); dragging.current = Math.abs(next - region.start) <= Math.abs(next - region.end) ? 'start' : 'end'
    event.currentTarget.setPointerCapture(event.pointerId); move(next, dragging.current)
  }
  const finish = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragging.current || !sample) return
    const next = position(event)
    const finalRegion = dragging.current === 'start' ? { ...region, start: Math.min(next, region.end - minimumRegion) } : { ...region, end: Math.max(next, region.start + minimumRegion) }
    setRegion(finalRegion); sendPluginCommand('setSampleRegion', { id: sample.id, ...finalRegion }); dragging.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId)
  }
  const nudge = (event: React.KeyboardEvent<HTMLDivElement>, handle: 'start' | 'end') => {
    if (!sample || !['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return
    event.preventDefault(); const delta = event.shiftKey ? .025 : .005
    const next = event.key === 'Home' ? 0 : event.key === 'End' ? 1 : region[handle] + (event.key === 'ArrowLeft' ? -delta : delta)
    const finalRegion = handle === 'start' ? { ...region, start: Math.max(0, Math.min(next, region.end - minimumRegion)) } : { ...region, end: Math.min(1, Math.max(next, region.start + minimumRegion)) }
    setRegion(finalRegion); sendPluginCommand('setSampleRegion', { id: sample.id, ...finalRegion })
  }
  return <PixelDisplay className="waveform-display"><div className="waveform-track">
    <WaveformCanvas waveform={sample?.waveform} />
    {sample && <>
      <div className="region-shade left" style={{ width: `${region.start * 100}%` }} /><div className="region-shade right" style={{ width: `${(1 - region.end) * 100}%` }} />
      {(['start', 'end'] as const).map((handle) => <div key={handle} className={`marker ${handle}`} role="slider" tabIndex={0}
        aria-label={`Sample ${handle}`} aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(region[handle] * 100)}
        style={{ left: `${region[handle] * 100}%` }} onKeyDown={(event) => nudge(event, handle)}><b>{handle.toUpperCase()}</b><i /></div>)}
      <div className="waveform-interaction" role="group" aria-label="Sample playback region" onPointerDown={begin}
        onPointerMove={(event) => { if (dragging.current) move(position(event), dragging.current) }} onPointerUp={finish}
        onPointerCancel={(event) => { dragging.current = null; setRegion({ start: sample.start, end: sample.end }); if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId) }} />
    </>}
  </div></PixelDisplay>
}

function SampleNumber({ sample, property, value, min, max, step, suffix, values, dragPixelsPerStep = 4 }: {
  sample?: SampleSummary, property: string, value: number, min: number, max: number, step: number, suffix: string,
  values?: readonly number[], dragPixelsPerStep?: number
}) {
  const drag = useRef<{ pointerId: number, startY: number, startValue: number, nextValue: number } | null>(null)
  const dragCleanup = useRef<(() => void) | null>(null)
  const [dragValue, setDragValue] = useState<number | null>(null)
  const quantise = (next: number) => {
    const bounded = Math.max(min, Math.min(max, Number.isFinite(next) ? next : value))
    if (values?.length) return values.reduce((closest, candidate) =>
      Math.abs(candidate - bounded) < Math.abs(closest - bounded) ? candidate : closest, values[0])
    return Math.max(min, Math.min(max, min + Math.round((bounded - min) / step) * step))
  }
  const shownValue = dragValue ?? quantise(value)
  const set = (next: number) => sample && sendPluginCommand('setSampleProperty', {
    id: sample.id, property, value: quantise(next)
  })
  useEffect(() => () => dragCleanup.current?.(), [])
  const beginDrag = (event: ReactPointerEvent<HTMLInputElement>) => {
    if (!sample || event.button !== 0) return
    dragCleanup.current?.()
    const active = { pointerId: event.pointerId, startY: event.clientY, startValue: shownValue, nextValue: shownValue }
    drag.current = active
    setDragValue(shownValue)
    const move = (pointer: PointerEvent) => {
      if (pointer.pointerId !== active.pointerId) return
      pointer.preventDefault()
      const offset = Math.round((active.startY - pointer.clientY) / dragPixelsPerStep)
      if (values?.length) {
        const startIndex = values.indexOf(quantise(active.startValue))
        active.nextValue = values[Math.max(0, Math.min(values.length - 1, startIndex + offset))]
      } else active.nextValue = quantise(active.startValue + offset * step)
      setDragValue(active.nextValue)
    }
    const finish = (pointer: PointerEvent) => {
      if (pointer.pointerId !== active.pointerId) return
      dragCleanup.current?.()
      drag.current = null
      set(active.nextValue)
      setDragValue(null)
    }
    const cleanup = () => {
      window.removeEventListener('pointermove', move, true)
      window.removeEventListener('pointerup', finish, true)
      window.removeEventListener('pointercancel', finish, true)
      if (dragCleanup.current === cleanup) dragCleanup.current = null
    }
    dragCleanup.current = cleanup
    window.addEventListener('pointermove', move, { capture: true, passive: false })
    window.addEventListener('pointerup', finish, true)
    window.addEventListener('pointercancel', finish, true)
  }
  return <div className="stepper-shell"><div className="stepper"><input type="number" min={min} max={max} step={step}
    value={shownValue} aria-label={property} title="Double-click and drag vertically to adjust" disabled={!sample}
    onChange={(event) => set(Number(event.target.value))} onPointerDown={beginDrag}
    onDoubleClick={(event) => event.preventDefault()} />
    <div><button disabled={!sample} onClick={() => set(shownValue + step)}>+</button><button disabled={!sample}
      onClick={() => set(shownValue - step)}>−</button></div></div><span className="unit-line">{suffix}</span></div>
}

function SourceTrim({ sample }: { sample?: SampleSummary }) {
  const set = (value: number) => sample && sendPluginCommand('setSampleProperty', { id: sample.id, property: 'gainDb', value })
  const value = sample?.gainDb ?? 0
  return <div className={`source-trim ${sample ? '' : 'disabled'}`}><input type="range" aria-label="Sample trim" min={-60} max={12} step={.1}
    value={value} disabled={!sample} onChange={(event) => set(Number(event.target.value))}
    onDoubleClick={() => set(0)} />
    <EditableValue label="Sample trim exact value" value={value} min={-60} max={12} step={.1} digits={1}
      suffix="dB" disabled={!sample} onCommit={set} />
  </div>
}

function SourceControls({ sample }: { sample?: SampleSummary }) {
  const stretchValues = [.25, .5, .75, 1, 1.25, 1.5, 1.75, 2] as const
  return <div className="source-controls">
    <label><b>TUNE</b><SampleNumber sample={sample} property="transpose" value={sample?.transpose ?? 0} min={-24} max={24} step={1} suffix="st" dragPixelsPerStep={4} /></label>
    <label><b>DRIFT</b><SampleNumber sample={sample} property="fineTune" value={sample?.fineTune ?? 0} min={-100} max={100} step={1} suffix="ct" dragPixelsPerStep={2} /></label>
    <label><b>STRETCH{sample?.stretchPending ? '…' : ''}</b><SampleNumber sample={sample} property="stretch" value={sample?.stretch ?? 1} min={.25} max={2} step={.25} suffix="×" values={stretchValues} dragPixelsPerStep={14} /></label>
    <label><b>TRIM</b><SourceTrim sample={sample} /></label>
  </div>
}

function BleedModule({ enabled }: { enabled: boolean }) {
  const pressure = usePluginParameter('smearAmount'); const visualisation = useVisualisationState()
  const mix = usePluginParameter('bleedMix')
  const shape = usePluginParameter('bleedShape')
  const commitShape = (value: number) => { shape.beginGesture(); shape.setValue(value); shape.endGesture() }
  return <RecompilerPanel title="BLEED" className={`effect-module bleed-module ${enabled ? '' : 'bypassed'}`} headerAction={<EffectPower effect={1} enabled={enabled} />}>
    <div className="bleed-controls">
      <PressureControl id="smearAmount" accessibleLabel="Bleed pressure" />
      <PressureControl id="bleedMix" accessibleLabel="Bleed mix" label="MIX" />
    </div>
    <div className="bleed-shape">
      <label htmlFor="bleed-shape">SHAPE</label>
      <input id="bleed-shape" aria-label="Bleed grain shape" type="range" min={-100} max={100} step={0.1} value={shape.value}
        aria-valuetext={shape.value < -2 ? 'Bubbly' : shape.value > 2 ? 'Piercing' : 'Balanced'}
        onPointerDown={shape.beginGesture} onPointerUp={shape.endGesture} onPointerCancel={shape.endGesture}
        onKeyDown={shape.beginGesture} onKeyUp={shape.endGesture} onChange={(event) => shape.setValue(Number(event.target.value))}
        onDoubleClick={() => commitShape(0)} />
      <div className="bleed-shape-values"><span>BUBBLY</span><span>PIERCING</span></div>
    </div>
    <PixelDisplay className="effect-display bleed-display"><NeuralBackground className="bleed-neural-root" audioLevel={visualisation.audioLevel}
      pressure={pressure.value / 100} mix={mix.value / 100} shape={shape.value} active={enabled} /></PixelDisplay>
    <BypassOverlay enabled={enabled} />
  </RecompilerPanel>
}

function FaultModule({ mutations, enabled }: { mutations: number, enabled: boolean }) {
  const visualisation = useVisualisationState()
  const choices = [{ label: 'FLIP', bit: 4 }, { label: 'DUST', bit: 2 }, { label: 'WARP', bit: 1 }]
  return <RecompilerPanel title="FAULT" className={`effect-module fault-module ${enabled ? '' : 'bypassed'}`} headerAction={<EffectPower effect={0} enabled={enabled} />}>
    <div className="creative-controls fault-controls"><PressureControl id="faultPressure" accessibleLabel="Fault pressure" />
      <div className="mutation-select" role="group" aria-label="Enabled fault mutations">
        {choices.map(({ label, bit }) => <PixelButton key={label} active={(mutations & bit) !== 0}
          onClick={() => sendPluginCommand('setFaultMutations', { mutations: mutations ^ bit })}>{label}</PixelButton>)}
      </div></div>
    <PixelDisplay className="fault-display"><FaultWaves eventSerial={visualisation.faultEventSerial}
      audioLevel={visualisation.audioLevel} active={enabled} /></PixelDisplay>
    <BypassOverlay enabled={enabled} />
  </RecompilerPanel>
}

function SpectralModule({ values, width, height, enabled, onInfo }: { values: number[], width: number, height: number, enabled: boolean, onInfo: () => void }) {
  const [resetSignal, setResetSignal] = useState(0)
  const visualisation = useVisualisationState()
  const reset = () => { setResetSignal((current) => current + 1); sendPluginCommand('resetSpectral') }
  return <RecompilerPanel title="ETCH" className={`effect-module spectral-module ${enabled ? '' : 'bypassed'}`} headerAction={<><ActionButton className="spectral-reset" onClick={reset}>CLEAR</ActionButton><EffectPower effect={2} enabled={enabled} /></>}>
    <PixelDisplay className="effect-display"><SpectralDrawCanvas values={values} width={width} height={height}
      resetSignal={resetSignal} scanPosition={visualisation.spectralScan} active={enabled} /></PixelDisplay>
    <div className="etch-footer"><button type="button" className="etch-brand-button" aria-label="Open About" onClick={onInfo}>
      <img src={productLogo} alt="RECOMPILER" /></button>
      <div className="creative-controls"><PressureControl id="spectralDepth" accessibleLabel="Etch pressure" /></div></div>
    <BypassOverlay enabled={enabled} />
  </RecompilerPanel>
}

function OutputFader({ id, label, format, reference }: { id: string, label: string, format: (value: number) => string, reference: number }) {
  const parameter = usePluginParameter(id); const min = parameter.descriptor?.min ?? 0; const max = parameter.descriptor?.max ?? 100
  const shown = Math.max(min, Math.min(max, parameter.value)); const position = (shown - min) / Math.max(.001, max - min)
  const referencePosition = (reference - min) / Math.max(.001, max - min)
  const commit = (value: number) => { parameter.beginGesture(); parameter.setValue(value); parameter.endGesture() }
  return <div className="output-fader"><b>{label}</b>
    <div className="fader-track" style={{ '--fader-position': `${position * 100}%`, '--reference-position': `${referencePosition * 100}%` } as CSSProperties}><i />
      <input aria-label={parameter.descriptor?.name ?? label} type="range" min={min} max={max} step={parameter.descriptor?.interval || 1} value={shown}
        onPointerDown={parameter.beginGesture} onPointerUp={parameter.endGesture} onPointerCancel={parameter.endGesture}
        onKeyDown={parameter.beginGesture} onKeyUp={parameter.endGesture} onChange={(event) => parameter.setValue(Number(event.target.value))} />
    </div><EditableValue label={`${label} exact value`} value={shown} min={min} max={max}
      step={parameter.descriptor?.interval || 1} digits={id === 'output' ? 1 : 0} format={format}
      suffix={id === 'output' && shown > 0 ? '%' : ''} onCommit={commit} />
  </div>
}

function OutputModule() {
  const visualisation = useVisualisationState()
  return <RecompilerPanel title="MASTER" className="output-module"><div className="output-body">
    <div className="pixel-display stereo-meter"><StereoMeterCanvas left={visualisation.outputPeakLeft ?? visualisation.outputPeak} right={visualisation.outputPeakRight ?? visualisation.outputPeak} /></div>
    <div className="meter-ticks"><span>+6</span><span>0</span><span>−6</span><span>−12</span><span>−24</span><span>−36</span><span>dB</span></div>
    <OutputFader id="output" label="VOL" reference={100} format={(value) => value <= 0 ? '−∞' : `${Math.round(value)}`} />
    <OutputFader id="globalPitch" label="PITCH" reference={0} format={(value) => `${value > 0 ? '+' : ''}${Math.round(value)}`} />
  </div></RecompilerPanel>
}

const aboutCopy = `ABOUT

RECOMPILER is a creative sampler instrument built around a pool of audio sources, playable randomness, source shaping and destructive-but-controlled processing.

Load multiple sounds into POOL. Trigger them through MIDI. Shape each source. Then push the result through FAULT, BLEED and ETCH.

You choose the material and the boundaries. RECOMPILER turns that material into something playable, unpredictable and repeatable enough to perform with.

GET STARTED

01 / LOAD AUDIO
Drop supported WAV, AIF, AIFF, MP3 or FLAC files into POOL. Use drag-and-drop or click the import area to browse. Each enabled source becomes part of the playable pool.

02 / PLAY
Send MIDI into RECOMPILER. Each trigger selects from the enabled sources. Use VOICES to choose between POLY and MONO operation.

03 / SHAPE THE SOURCE
Select a source in POOL. START / END set its playable region. TUNE transposes in semitones. DRIFT fine-tunes in cents. STRETCH changes timing while preserving pitch. TRIM adjusts source level.

04 / STACK
STACK controls whether incoming MIDI note pitch affects playback. OFF triggers without following note pitch. ON transposes playback relative to MIDI note 72.

05 / FAULT
FAULT introduces tempo-synced mutations into the combined sampler output. PRESSURE controls how often mutations occur and how small the possible slices become. LOW means fewer mutations, larger slices and more untouched signal. HIGH means more mutations, smaller slices and more rhythmic disruption.

FAULT uses 1/2, 1/4, 1/8 and 1/16 divisions. FLIP plays a slice backward. DUST reduces digital resolution and sample rate. WARP replays a slice one octave up or down using varispeed, changing speed and pitch together. Enable any combination; FAULT chooses once per event.

06 / BLEED
BLEED PRESSURE sets grain density. MIX blends the dry source with the processed grains. SHAPE is centered by default: left is bubbly; right is piercing. Shape does not change pitch; BLEED's processed grains stay fixed one octave above the source.

07 / ETCH
ETCH lets you draw directly into the spectral content. ETCH PRESSURE controls the depth of spectral removal. Frequency runs vertically and time horizontally. Draw to remove spectral energy as the scanner passes through the mask. CLEAR removes the drawing.

08 / MASTER
VOL controls final output: 0% is silence, 100% is unity and 125% is approximately +5.6 dB. PITCH globally transposes the instrument from −12 to +12 semitones. The L / R meter displays final stereo output activity.

CONTROLS
DRAG UP ........ INCREASE
DRAG DOWN ...... DECREASE
SHIFT + DRAG ... FINE ADJUST
DOUBLE CLICK ... RESET
MOUSE WHEEL .... ADJUST

PROJECTS
RECOMPILER stores current parameter values, source configuration, FAULT setup, ETCH drawing and other instrument settings with the host project. Imported audio remains external to plugin state. Keep project samples in a stable location so RECOMPILER can find them when reopened.

QUICK TIPS
Use a wide START / END region for more variation and a narrow region for predictable fragments. Balance sources with TRIM before pushing MASTER VOL. Low FAULT PRESSURE adds occasional variation; high pressure creates dense rhythmic corruption. BLEED PRESSURE and ETCH PRESSURE can be combined for more extreme textures.

VERSION
RECOMPILER
VERSION: ${__RECOMPILER_VERSION__}
FORMAT: VST3 / STANDALONE
PLATFORM: WINDOWS x64
BUILD: ${__RECOMPILER_BUILD_ID__}

THIRD-PARTY SOFTWARE
JUCE 8.0.13 — AGPLv3 or commercial JUCE license
Signalsmith Stretch — MIT
Signalsmith Linear — MIT
React 18.3.1 / React DOM 18.3.1 — MIT
Simplex Noise 4.0.3 — MIT
Microsoft WebView2 — Microsoft software license terms
Spleen 2.2.0 — BSD 2-Clause
Cozette 1.30.0 — MIT
Complete notices ship in the release resources.

BUILT BY

INTERFACE SCALE`

const builtByRevealIndex = aboutCopy.indexOf('BUILT BY') + 'BUILT BY'.length

function AboutPage({ uiScale, onClose }: { uiScale: number, onClose: () => void }) {
  const pageRef = useRef<HTMLDivElement>(null)
  const frameRef = useRef<number | null>(null)
  const delayRef = useRef<number | null>(null)
  const skippedRef = useRef(false)
  const [visible, setVisible] = useState(0)
  const [skipped, setSkipped] = useState(false)
  const complete = skipped || visible >= aboutCopy.length
  const revealAll = () => {
    skippedRef.current = true
    setSkipped(true)
    if (delayRef.current !== null) window.clearTimeout(delayRef.current)
    delayRef.current = null
    if (frameRef.current !== null) cancelAnimationFrame(frameRef.current)
    frameRef.current = null
    setVisible(aboutCopy.length)
  }
  useEffect(() => { pageRef.current?.focus() }, [])
  useEffect(() => {
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) { setVisible(aboutCopy.length); return }
    const begin = () => {
      if (skippedRef.current) return
      const started = performance.now(); const draw = (time: number) => {
        if (skippedRef.current) return
        const next = Math.min(aboutCopy.length, Math.floor((time - started) * .46)); setVisible(next)
        frameRef.current = next < aboutCopy.length ? requestAnimationFrame(draw) : null
      }
      frameRef.current = requestAnimationFrame(draw)
    }
    delayRef.current = window.setTimeout(begin, 2000); return () => {
      if (delayRef.current !== null) window.clearTimeout(delayRef.current)
      delayRef.current = null
      if (frameRef.current !== null) cancelAnimationFrame(frameRef.current)
      frameRef.current = null
    }
  }, [])
  useEffect(() => {
    const skip = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null
      if ((event.code !== 'Space' && event.key !== ' ' && event.key !== 'Space') || target?.matches('input, select, textarea, [contenteditable="true"]')) return
      event.preventDefault(); revealAll()
    }; document.addEventListener('keydown', skip, { capture: true }); return () => document.removeEventListener('keydown', skip, { capture: true })
  }, [])
  const beforeCredits = aboutCopy.slice(0, Math.min(visible, builtByRevealIndex))
  const afterCredits = visible > builtByRevealIndex ? aboutCopy.slice(builtByRevealIndex, visible) : ''
  return <div ref={pageRef} className="about-page" style={{ position: 'relative' }} tabIndex={-1}>
    <button type="button" className="about-close" aria-label="Close About" onClick={onClose}><img src={closeIcon} alt="" /></button>
    <section className="about-terminal" aria-label="About and getting started manual"><div className="terminal-document">
      <pre>{beforeCredits}</pre>
      {visible >= builtByRevealIndex && <div className="terminal-credit-marks">
        <div className="creator-mark creator-mark-damnnprodigy"><img src={damnnprodigyLogo} alt="damnnprodigy" /></div>
        <div className="creator-mark creator-mark-shadx2"><img src={shadx2Logo} alt="shadx2" /></div>
      </div>}
      <pre>{afterCredits}{!complete && <span className="terminal-cursor" aria-hidden="true">█</span>}</pre>
      {complete && <div className="terminal-scale">{['75%', '100%', '125%', '150%'].map((label, index) => <PixelButton key={label} active={uiScale === index} onClick={() => sendPluginCommand('setUiScale', { index })}>{label}</PixelButton>)}</div>}
    </div>
    </section>
    {!complete && <div className="skip-typing" role="status" aria-live="polite">PRESS SPACE TO SKIP</div>}
  </div>
}

export default function App() {
  const backendState = useBackendState(); const [page, setPage] = useState<'main' | 'about'>('main'); const [draggingFiles, setDraggingFiles] = useState(false)
  const samples = backendState?.samples ?? []
  const selectedSample = useMemo(() => samples.find((sample) => sample.id === backendState?.selectedSampleId), [samples, backendState?.selectedSampleId])
  useEffect(() => {
    const prevent = (event: DragEvent) => { if (event.dataTransfer?.types.includes('Files')) event.preventDefault() }
    document.addEventListener('dragover', prevent); document.addEventListener('drop', prevent)
    return () => { document.removeEventListener('dragover', prevent); document.removeEventListener('drop', prevent) }
  }, [])
  const enabled = backendState?.effectEnabled ?? [true, true, true]
  return <main className="recompiler-shell">
    {page === 'about' ? <AboutPage uiScale={backendState?.uiScale ?? 1} onClose={() => setPage('main')} /> : <>
      <div className="source-zone">
        <div className={`pool-drop-target ${draggingFiles ? 'drag-active' : ''}`}
          onDragEnter={(event) => { event.preventDefault(); event.stopPropagation(); setDraggingFiles(true) }}
          onDragOver={(event) => { event.preventDefault(); event.stopPropagation(); setDraggingFiles(true) }}
          onDragLeave={(event) => { if (!event.currentTarget.contains(event.relatedTarget as Node)) setDraggingFiles(false) }}
          onDrop={(event) => { event.preventDefault(); event.stopPropagation(); setDraggingFiles(false); postDroppedFiles(event.dataTransfer.files) }}>
          <RecompilerPanel title="POOL" className={`samples-panel ${samples.length === 0 ? 'is-empty' : ''}`}>{samples.length === 0
            ? <button type="button" className={`pool-empty-action ${draggingFiles ? 'drag-active' : ''}`} onClick={() => sendPluginCommand('importSamples')}>{draggingFiles ? 'RELEASE TO IMPORT' : 'DROP / CLICK TO IMPORT AUDIO'}</button>
            : <><div className="sample-list">{samples.map((sample) => <SampleRow key={sample.id} sample={sample} selected={sample.id === selectedSample?.id} />)}</div>
              <button className={`pool-loaded-action ${draggingFiles ? 'drag-active' : ''}`} onClick={() => sendPluginCommand('importSamples')}>{draggingFiles ? 'RELEASE TO IMPORT' : 'DROP / CLICK TO IMPORT AUDIO'}</button></>}
          </RecompilerPanel>
        </div>
        <RecompilerPanel title="SOURCE" className="selected-source"><div className="source-title"><strong>{selectedSample?.name ?? 'NO SOURCE SELECTED'}</strong><span>{selectedSample ? `${(selectedSample.sampleRate / 1000).toFixed(1)} kHz  ${selectedSample.bitDepth || '--'} bit  ${selectedSample.durationSeconds.toFixed(1)} s` : ''}</span></div><Waveform sample={selectedSample} /><SourceControls sample={selectedSample} /></RecompilerPanel>
      </div>
      <div className="lower-zone"><SpectralModule values={backendState?.spectralCanvas ?? []} width={backendState?.spectralWidth ?? 128} height={backendState?.spectralHeight ?? 64} enabled={enabled[2] !== false} onInfo={() => setPage('about')} />
        <div className="middle-rack"><div className="global-strip"><label>STACK <PixelToggle id="midiPitch" left="OFF" right="ON" /></label><Divider /><label>VOICES <PixelToggle id="voiceMode" left="POLY" right="MONO" /></label></div>
          <div className="effect-pair"><FaultModule mutations={backendState?.faultMutations ?? 7} enabled={enabled[0] !== false} /><BleedModule enabled={enabled[1] !== false} /></div></div>
        <OutputModule /></div>
    </>}
  </main>
}
