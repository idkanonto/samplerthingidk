import { type CSSProperties, type MouseEvent, type PointerEvent as ReactPointerEvent, type ReactNode, useEffect, useMemo, useRef, useState } from 'react'
import {
  sendPluginCommand,
  type SampleSummary,
  useBackendState,
  usePluginParameter,
  useVisualisationState
} from './juceBridge'
import {
  EffectCanvas, SpectralDrawCanvas, StereoMeterCanvas, WaveformCanvas
} from './VisualCanvases'

const tonicNames = ['NONE', 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

function HatchFill() { return <span className="hatch-fill" aria-hidden="true" /> }
function Divider() { return <span className="divider" aria-hidden="true" /> }

function PixelButton({ children, active = false, className = '', onClick, title }: {
  children: ReactNode, active?: boolean, className?: string,
  onClick?: (event: MouseEvent<HTMLButtonElement>) => void, title?: string
}) {
  return <button type="button" title={title} className={`pixel-button ${active ? 'active' : ''} ${className}`} onClick={onClick}>{children}</button>
}

function PixelToggle({ id, left, right }: { id: string, left: string, right: string }) {
  const parameter = usePluginParameter(id)
  const set = (value: number) => {
    parameter.beginGesture(); parameter.setValue(value); parameter.endGesture()
  }
  return <div className="pixel-toggle" role="group" aria-label={parameter.descriptor?.name ?? id}>
    <PixelButton active={parameter.value < 0.5} onClick={() => set(0)}>{left}</PixelButton>
    <PixelButton active={parameter.value >= 0.5} onClick={() => set(1)}>{right}</PixelButton>
  </div>
}

function PixelSelect({ id }: { id: string }) {
  const parameter = usePluginParameter(id)
  const options = id === 'targetKey' ? tonicNames : (parameter.descriptor?.choices ?? [])
  return <select className="pixel-select" aria-label={parameter.descriptor?.name ?? id}
    value={Math.round(parameter.value)} onFocus={parameter.beginGesture} onBlur={parameter.endGesture}
    onChange={(event) => parameter.setValue(Number(event.target.value))}>
    {options.map((option, index) => <option key={option} value={index}>{option}</option>)}
  </select>
}

function NumericReadout({ value, digits = 0, suffix = '' }: { value: number, digits?: number, suffix?: string }) {
  return <output className="numeric-readout">{value.toFixed(digits)}{suffix}</output>
}

function PixelKnob({ id, label }: { id: string, label: string }) {
  const parameter = usePluginParameter(id)
  const descriptor = parameter.descriptor
  const min = descriptor?.min ?? 0
  const max = descriptor?.max ?? 100
  const percent = Math.max(0, Math.min(1, (parameter.value - min) / Math.max(0.001, max - min)))
  const finish = () => parameter.endGesture()
  return <div className="knob-control">
    <div className="pixel-knob" style={{ '--knob-angle': `${-138 + percent * 276}deg` } as CSSProperties}>
      <i />
      <input aria-label={label} type="range" min={min} max={max} step={descriptor?.interval || 0.1}
        value={parameter.value} onPointerDown={parameter.beginGesture}
        onChange={(event) => parameter.setValue(Number(event.target.value))}
        onPointerUp={finish} onPointerCancel={finish}
        onKeyDown={parameter.beginGesture} onKeyUp={finish} />
    </div>
    <div className="knob-scale"><span>{min}</span><span>{max}</span></div>
    <NumericReadout value={parameter.value} digits={0} />
  </div>
}

function ModuleHeader({ children }: { children: ReactNode }) {
  return <div className="module-header"><strong>{children}</strong><HatchFill /></div>
}

function RecompilerPanel({ title, children, className = '' }: { title?: string, children: ReactNode, className?: string }) {
  return <section className={`recompiler-panel ${className}`}>{title && <ModuleHeader>{title}</ModuleHeader>}{children}</section>
}

function PixelDisplay({ children, className = '' }: { children: ReactNode, className?: string }) {
  return <div className={`pixel-display ${className}`}>{children}</div>
}

function SampleRow({ sample, selected }: { sample: SampleSummary, selected: boolean }) {
  const select = () => sendPluginCommand('selectSample', { id: sample.id })
  return <div className={`sample-row ${selected ? 'selected' : ''}`} role="button" tabIndex={0} onClick={select}
    onKeyDown={(event) => { if (event.target === event.currentTarget && (event.key === 'Enter' || event.key === ' ')) { event.preventDefault(); select() } }}>
    <button className={`pixel-check ${sample.enabled ? 'checked' : ''}`} aria-label={`${sample.enabled ? 'Disable' : 'Enable'} ${sample.name}`}
      onClick={(event) => { event.stopPropagation(); sendPluginCommand('setSampleEnabled', { id: sample.id, enabled: !sample.enabled }) }}>
      {sample.enabled ? '✓' : ''}
    </button>
    <span className={sample.missing ? 'missing' : ''}>{sample.name}</span>
    <PixelButton className="remove-button" onClick={(event) => {
      event.stopPropagation(); sendPluginCommand('removeSample', { id: sample.id })
    }}>REMOVE</PixelButton>
  </div>
}

function Waveform({ sample }: { sample?: SampleSummary }) {
  const [region, setRegion] = useState({ start: sample?.start ?? 0, end: sample?.end ?? 1 })
  const dragging = useRef<'start' | 'end' | null>(null)
  const minimumRegion = 0.002

  useEffect(() => {
    setRegion({ start: sample?.start ?? 0, end: sample?.end ?? 1 })
    dragging.current = null
  }, [sample?.id, sample?.start, sample?.end])

  const pointerPosition = (event: ReactPointerEvent<HTMLDivElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect()
    return Math.max(0, Math.min(1, (event.clientX - bounds.left) / Math.max(1, bounds.width)))
  }
  const moveHandle = (position: number, handle: 'start' | 'end') => setRegion((current) => handle === 'start'
    ? { ...current, start: Math.min(position, current.end - minimumRegion) }
    : { ...current, end: Math.max(position, current.start + minimumRegion) })
  const beginDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    const position = pointerPosition(event)
    dragging.current = Math.abs(position - region.start) <= Math.abs(position - region.end) ? 'start' : 'end'
    event.currentTarget.setPointerCapture(event.pointerId)
    moveHandle(position, dragging.current)
  }
  const continueDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (dragging.current) moveHandle(pointerPosition(event), dragging.current)
  }
  const finishDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragging.current || !sample) return
    const position = pointerPosition(event)
    const finalRegion = dragging.current === 'start'
      ? { ...region, start: Math.min(position, region.end - minimumRegion) }
      : { ...region, end: Math.max(position, region.start + minimumRegion) }
    setRegion(finalRegion)
    sendPluginCommand('setSampleRegion', { id: sample.id, ...finalRegion })
    dragging.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId)
  }
  const cancelDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    dragging.current = null
    setRegion({ start: sample?.start ?? 0, end: sample?.end ?? 1 })
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId)
  }
  const nudgeHandle = (event: React.KeyboardEvent<HTMLDivElement>, handle: 'start' | 'end') => {
    if (!sample || !['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return
    event.preventDefault()
    const delta = event.shiftKey ? .025 : .005
    const position = event.key === 'Home' ? 0 : event.key === 'End' ? 1
      : region[handle] + (event.key === 'ArrowLeft' ? -delta : delta)
    const next = handle === 'start'
      ? { ...region, start: Math.max(0, Math.min(position, region.end - minimumRegion)) }
      : { ...region, end: Math.min(1, Math.max(position, region.start + minimumRegion)) }
    setRegion(next)
    sendPluginCommand('setSampleRegion', { id: sample.id, ...next })
  }
  return <PixelDisplay className="waveform-display">
    <WaveformCanvas waveform={sample?.waveform} />
    {!sample && <span className="waveform-empty">NO WAVEFORM DATA</span>}
    {sample && <>
      <div className="region-shade left" style={{ width: `${region.start * 100}%` }} />
      <div className="region-shade right" style={{ width: `${(1 - region.end) * 100}%` }} />
      <div className="marker start" role="slider" tabIndex={0} aria-label="Sample start" aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(region.start * 100)}
        style={{ left: `${region.start * 100}%` }} onKeyDown={(event) => nudgeHandle(event, 'start')}><b>START</b><i /></div>
      <div className="marker end" role="slider" tabIndex={0} aria-label="Sample end" aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(region.end * 100)}
        style={{ left: `${region.end * 100}%` }} onKeyDown={(event) => nudgeHandle(event, 'end')}><b>END</b><i /></div>
      <div className="waveform-interaction" role="group" aria-label="Sample playback region"
        onPointerDown={beginDrag} onPointerMove={continueDrag} onPointerUp={finishDrag} onPointerCancel={cancelDrag} />
    </>}
  </PixelDisplay>
}

function SampleNumber({ sample, property, value, min, max, step, suffix }: {
  sample?: SampleSummary, property: string, value: number, min: number, max: number, step: number, suffix: string
}) {
  const set = (next: number) => sample && sendPluginCommand('setSampleProperty', { id: sample.id, property, value: Math.max(min, Math.min(max, next)) })
  return <div className="stepper"><input type="number" min={min} max={max} step={step} value={value} aria-label={property}
    disabled={!sample} onChange={(event) => set(Number(event.target.value))} /><div><button disabled={!sample} onClick={() => set(value + step)}>▲</button><button disabled={!sample} onClick={() => set(value - step)}>▼</button></div><span>{suffix}</span></div>
}

function SourceGainKnob({ sample }: { sample?: SampleSummary }) {
  const min = -60
  const max = 12
  const value = sample?.gainDb ?? 0
  const percent = (value - min) / (max - min)
  return <div className="source-gain-control">
    <div className="pixel-knob source-gain-knob" style={{ '--knob-angle': `${-138 + percent * 276}deg` } as CSSProperties}>
      <i /><input aria-label="Sample gain" type="range" min={min} max={max} step="0.1" value={value} disabled={!sample}
        onChange={(event) => sample && sendPluginCommand('setSampleProperty', { id: sample.id, property: 'gainDb', value: Number(event.target.value) })} />
    </div>
    <NumericReadout value={value} digits={1} suffix=" dB" />
  </div>
}

function SourceControls({ sample }: { sample?: SampleSummary }) {
  return <div className="source-controls">
    <label><b>SOURCE KEY</b><select className="pixel-select" value={sample?.sourceKey ?? 1} disabled={!sample}
      onChange={(event) => sample && sendPluginCommand('setSampleProperty', { id: sample.id, property: 'sourceKey', value: Number(event.target.value) })}>
      {tonicNames.map((name, index) => <option key={name} value={index}>{name}</option>)}</select></label>
    <Divider />
    <label><b>TRANSPOSE</b><SampleNumber sample={sample} property="transpose" value={sample?.transpose ?? 0} min={-24} max={24} step={1} suffix="st" /></label>
    <Divider />
    <label><b>FINE TUNE</b><SampleNumber sample={sample} property="fineTune" value={sample?.fineTune ?? 0} min={-100} max={100} step={1} suffix="ct" /></label>
    <Divider />
    <label className="gain-source"><b>GAIN</b><SourceGainKnob sample={sample} /></label>
  </div>
}

function EffectActivityDisplay({ type, amount }: { type: 'scramble' | 'melt' | 'smear', amount: number }) {
  const visualisation = useVisualisationState()
  return <PixelDisplay className="effect-display"><EffectCanvas type={type} amount={amount} visualisation={visualisation} /></PixelDisplay>
}

function EffectModule({ title, id, type }: {
  title: string, id: string, type: 'scramble' | 'melt' | 'smear'
}) {
  const parameter = usePluginParameter(id)
  return <RecompilerPanel title={title} className="effect-module"><PixelKnob id={id} label={title} />
    <EffectActivityDisplay type={type} amount={parameter.value} />
  </RecompilerPanel>
}

function SpectralActivityDisplay({ values, width, height }: { values: number[], width: number, height: number }) {
  const visualisation = useVisualisationState()
  return <PixelDisplay className="effect-display"><SpectralDrawCanvas values={values} width={width} height={height}
    scan={visualisation.spectralScan} spectrum={visualisation.spectrum} /></PixelDisplay>
}

function SpectralModule({ values, width, height }: {
  values: number[], width: number, height: number
}) {
  return <RecompilerPanel title="SPECTRAL DRAW" className="effect-module spectral-module">
    <PixelKnob id="spectralDepth" label="Spectral Depth" />
    <SpectralActivityDisplay values={values} width={width} height={height} />
  </RecompilerPanel>
}

function StereoMeter() {
  const visualisation = useVisualisationState()
  return <PixelDisplay className="stereo-meter"><StereoMeterCanvas left={visualisation.outputPeakLeft ?? visualisation.outputPeak}
    right={visualisation.outputPeakRight ?? visualisation.outputPeak} /></PixelDisplay>
}

function OutputModule({ muted }: { muted: boolean }) {
  const output = usePluginParameter('output')
  const min = output.descriptor?.min ?? -60
  const max = output.descriptor?.max ?? 6
  const shown = Math.max(min, Math.min(max, output.value))
  const faderPosition = (shown - min) / Math.max(0.001, max - min)
  const finish = () => output.endGesture()
  return <RecompilerPanel title="OUTPUT" className="output-module"><div className="output-body">
    <StereoMeter />
    <div className="meter-ticks"><span>+{max}</span><span>0</span><span>-6</span><span>-12</span><span>-24</span><span>-36</span><span>dB</span></div>
    <div className="output-fader"><b>LEVEL</b><div className="fader-track" style={{ '--fader-position': `${faderPosition * 100}%` } as CSSProperties}><i /><input aria-label="Output level" type="range" min={min} max={max} step={output.descriptor?.interval || 0.1}
      value={shown} onPointerDown={output.beginGesture} onPointerUp={finish} onPointerCancel={finish}
      onKeyDown={output.beginGesture} onKeyUp={finish} onChange={(event) => output.setValue(Number(event.target.value))} /></div>
      <PixelButton className="mute-button" active={muted} onClick={() => sendPluginCommand('setOutputMuted', { enabled: !muted })}>{muted ? 'UNMUTE' : 'MUTE'}</PixelButton>
    </div>
  </div></RecompilerPanel>
}

function VoiceCounter({ maximum }: { maximum: number }) {
  const visualisation = useVisualisationState()
  return <div className="voice-counter">{String(visualisation.voiceCount).padStart(2, '0')} / {maximum} VOICES</div>
}

function Header({ page, setPage, maxSamples }: { page: string, setPage: (page: string) => void, maxSamples: number }) {
  return <header className="app-header">
    <div className="brand-reserve" aria-label="Reserved branding area"><i /><i /><i /></div>
    <div className="header-actions"><VoiceCounter maximum={maxSamples} />
      <nav><PixelButton active={page === 'main'} onClick={() => setPage('main')}>MAIN</PixelButton><PixelButton active={page === 'settings'} onClick={() => setPage('settings')}>SETTINGS</PixelButton></nav>
    </div>
  </header>
}

function SettingsPage({ sampleCount, maximumSampleCount, effectEnabled, uiScale }: {
  sampleCount: number, maximumSampleCount: number, effectEnabled: boolean[], uiScale: number
}) {
  const effects = ['SCRAMBLE', 'MELT', 'SMEAR', 'SPECTRAL DRAW']
  return <div className="settings-page">
    <RecompilerPanel title="ENGINE" className="settings-block"><dl><dt>AUDIO</dt><dd>NATIVE C++</dd><dt>EDITOR</dt><dd>EMBEDDED / OFFLINE</dd><dt>PROJECT STATE</dt><dd>AUTOMATIC</dd></dl></RecompilerPanel>
    <RecompilerPanel title="INTERFACE SCALE" className="settings-block scale-settings"><p>FIXED LOGICAL CANVAS / UNIFORM SCALE</p><div>{['75%', '100%', '125%', '150%'].map((label, index) => <PixelButton key={label} active={uiScale === index} onClick={() => sendPluginCommand('setUiScale', { index })}>{label}</PixelButton>)}</div></RecompilerPanel>
    <RecompilerPanel title="SAMPLE LIBRARY" className="settings-block"><dl><dt>LOADED</dt><dd>{sampleCount} / {maximumSampleCount}</dd><dt>FORMATS</dt><dd>WAV / AIFF / MP3 / FLAC</dd></dl><PixelButton onClick={() => sendPluginCommand('importSamples')}>ADD SAMPLES…</PixelButton></RecompilerPanel>
    <RecompilerPanel title="EFFECT ENGINES" className="settings-block effect-settings">
      {effects.map((name, effect) => <PixelButton key={name} active={effectEnabled[effect] !== false}
        onClick={() => sendPluginCommand('setEffectEnabled', { effect, enabled: effectEnabled[effect] === false })}>{name}</PixelButton>)}
      <PixelButton onClick={() => sendPluginCommand('regenerateSeed')}>NEW RANDOM SEED</PixelButton>
      <PixelButton onClick={() => sendPluginCommand('resetSpectral')}>CLEAR SPECTRAL MASK</PixelButton>
    </RecompilerPanel>
  </div>
}

export default function App() {
  const backendState = useBackendState()
  const [page, setPage] = useState('main')
  const samples = backendState?.samples ?? []
  const selectedSample = useMemo(() => samples.find((sample) => sample.id === backendState?.selectedSampleId), [samples, backendState?.selectedSampleId])
  return <main className="recompiler-shell">
    <Header page={page} setPage={setPage} maxSamples={backendState?.maximumSampleCount ?? 20} />
    {page === 'settings' ? <SettingsPage sampleCount={backendState?.sampleCount ?? 0} maximumSampleCount={backendState?.maximumSampleCount ?? 20} effectEnabled={backendState?.effectEnabled ?? [true, true, true, true]} uiScale={backendState?.uiScale ?? 1} /> : <>
      <div className="source-zone">
        <RecompilerPanel title="SAMPLES" className="samples-panel">
          <div className="sample-list">{samples.length ? samples.map((sample) => <SampleRow key={sample.id} sample={sample} selected={sample.id === selectedSample?.id} />) : <p className="empty-samples">NO SAMPLES LOADED</p>}</div>
          <button className="drop-zone" onClick={() => sendPluginCommand('importSamples')} onDragOver={(event) => event.preventDefault()} onDrop={(event) => { event.preventDefault(); sendPluginCommand('importSamples') }}>DROP WAV, AIFF, MP3 OR FLAC</button>
        </RecompilerPanel>
        <RecompilerPanel className="selected-source">
          <div className="source-title"><strong>{selectedSample?.name ?? 'NO SAMPLE SELECTED'}</strong><span>{selectedSample ? `${(selectedSample.sampleRate / 1000).toFixed(1)} kHz   ${selectedSample.bitDepth || '--'} bit   ${selectedSample.durationSeconds.toFixed(1)} s` : '--.- kHz   -- bit   --.- s'}</span></div>
          <Waveform sample={selectedSample} /><SourceControls sample={selectedSample} />
        </RecompilerPanel>
      </div>
      <div className="global-strip"><strong>GLOBAL</strong><label>PLAY IN KEY <PixelSelect id="targetKey" /></label><Divider /><label>CHORDS <PixelToggle id="midiPitch" left="OFF" right="ON" /></label><Divider /><label>POLY / MONO <PixelToggle id="voiceMode" left="POLY" right="MONO" /></label></div>
      <div className="effects-zone">
        <EffectModule title="SCRAMBLE" id="scrambleAmount" type="scramble" />
        <EffectModule title="MELT" id="meltAmount" type="melt" />
        <EffectModule title="SMEAR" id="smearAmount" type="smear" />
        <SpectralModule values={backendState?.spectralCanvas ?? []} width={backendState?.spectralWidth ?? 128} height={backendState?.spectralHeight ?? 64} />
        <OutputModule muted={backendState?.outputMuted ?? false} />
      </div>
    </>}
  </main>
}
