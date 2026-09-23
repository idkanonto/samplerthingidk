import { type CSSProperties, type MouseEvent, type ReactNode, useMemo, useState } from 'react'
import {
  isRunningInJuce,
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
        onPointerUp={finish} onPointerCancel={finish} />
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
  return <div className={`sample-row ${selected ? 'selected' : ''}`} onClick={() => sendPluginCommand('selectSample', { id: sample.id })}>
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
  const start = sample?.start ?? 0
  const end = sample?.end ?? 1
  const update = (nextStart: number, nextEnd: number) => sample && sendPluginCommand('setSampleRegion', {
    id: sample.id, start: Math.min(nextStart, nextEnd - 0.01), end: Math.max(nextEnd, nextStart + 0.01)
  })
  return <PixelDisplay className="waveform-display">
    <WaveformCanvas waveform={sample?.waveform} />
    {!sample && <span className="waveform-empty">NO WAVEFORM DATA</span>}
    {sample && <>
      <div className="region-shade left" style={{ width: `${start * 100}%` }} />
      <div className="region-shade right" style={{ width: `${(1 - end) * 100}%` }} />
      <div className="marker start" style={{ left: `${start * 100}%` }}><b>START</b><i /></div>
      <div className="marker end" style={{ left: `${end * 100}%` }}><b>END</b><i /></div>
      <input className="region-input start-input" aria-label="Sample start" type="range" min="0" max="1" step="0.001" value={start} onChange={(event) => update(Number(event.target.value), end)} />
      <input className="region-input end-input" aria-label="Sample end" type="range" min="0" max="1" step="0.001" value={end} onChange={(event) => update(start, Number(event.target.value))} />
    </>}
  </PixelDisplay>
}

function SampleNumber({ sample, property, value, min, max, step, suffix }: {
  sample: SampleSummary, property: string, value: number, min: number, max: number, step: number, suffix: string
}) {
  const set = (next: number) => sendPluginCommand('setSampleProperty', { id: sample.id, property, value: Math.max(min, Math.min(max, next)) })
  return <div className="stepper"><input type="number" min={min} max={max} step={step} value={value} aria-label={property}
    onChange={(event) => set(Number(event.target.value))} /><div><button onClick={() => set(value + step)}>▲</button><button onClick={() => set(value - step)}>▼</button></div><span>{suffix}</span></div>
}

function SourceControls({ sample }: { sample?: SampleSummary }) {
  if (!sample) return <div className="source-controls empty">IMPORT A SAMPLE TO EDIT ITS SOURCE SETTINGS</div>
  return <div className="source-controls">
    <label><b>SOURCE KEY</b><select className="pixel-select" value={sample.sourceKey}
      onChange={(event) => sendPluginCommand('setSampleProperty', { id: sample.id, property: 'sourceKey', value: Number(event.target.value) })}>
      {tonicNames.map((name, index) => <option key={name} value={index}>{name}</option>)}</select></label>
    <Divider />
    <label><b>TRANSPOSE</b><SampleNumber sample={sample} property="transpose" value={sample.transpose} min={-24} max={24} step={1} suffix="st" /></label>
    <Divider />
    <label><b>FINE TUNE</b><SampleNumber sample={sample} property="fineTune" value={sample.fineTune} min={-100} max={100} step={1} suffix="ct" /></label>
    <Divider />
    <label className="gain-source"><b>GAIN</b><input type="range" min="-60" max="12" step="0.1" value={sample.gainDb}
      onChange={(event) => sendPluginCommand('setSampleProperty', { id: sample.id, property: 'gainDb', value: Number(event.target.value) })} />
      <NumericReadout value={sample.gainDb} digits={1} suffix=" dB" /></label>
  </div>
}

function EffectModule({ title, id, type, visualisation }: {
  title: string, id: string, type: 'scramble' | 'melt' | 'smear', visualisation: ReturnType<typeof useVisualisationState>
}) {
  const parameter = usePluginParameter(id)
  return <RecompilerPanel title={title} className="effect-module"><PixelKnob id={id} label={title} />
    <PixelDisplay className="effect-display"><EffectCanvas type={type} amount={parameter.value} visualisation={visualisation} /></PixelDisplay>
  </RecompilerPanel>
}

function SpectralModule({ values, width, height, scan, spectrum }: {
  values: number[], width: number, height: number, scan: number, spectrum?: number[]
}) {
  return <RecompilerPanel title="SPECTRAL DRAW" className="effect-module spectral-module">
    <PixelKnob id="spectralDepth" label="Spectral Depth" />
    <PixelDisplay className="effect-display"><SpectralDrawCanvas values={values} width={width} height={height} scan={scan} spectrum={spectrum} /></PixelDisplay>
  </RecompilerPanel>
}

function OutputModule({ leftPeak, rightPeak, muted }: { leftPeak: number, rightPeak: number, muted: boolean }) {
  const output = usePluginParameter('output')
  const min = output.descriptor?.min ?? -60
  const max = 4
  const shown = Math.max(min, Math.min(max, output.value))
  const finish = () => output.endGesture()
  return <RecompilerPanel title="OUTPUT" className="output-module"><div className="output-body">
    <PixelDisplay className="stereo-meter"><StereoMeterCanvas left={leftPeak} right={rightPeak} /></PixelDisplay>
    <div className="meter-ticks"><span>+4</span><span>0</span><span>-6</span><span>-12</span><span>-24</span><span>-36</span><span>dB</span></div>
    <div className="output-fader"><b>LEVEL</b><div className="fader-track"><input aria-label="Output level" type="range" min={min} max={max} step={output.descriptor?.interval || 0.1}
      value={shown} onPointerDown={output.beginGesture} onPointerUp={finish} onPointerCancel={finish} onChange={(event) => output.setValue(Number(event.target.value))} /></div>
      <PixelButton className="mute-button" active={muted} onClick={() => sendPluginCommand('setOutputMuted', { enabled: !muted })}>{muted ? 'UNMUTE' : 'MUTE'}</PixelButton>
    </div>
  </div></RecompilerPanel>
}

function Header({ page, setPage, sampleCount, voices, maxSamples }: { page: string, setPage: (page: string) => void, sampleCount: number, voices: number, maxSamples: number }) {
  return <header className="app-header">
    <div className="brand"><div className="logo-mark">⌁╳</div><h1>recompiler.dll</h1><Divider /><span>random sample instrument</span></div>
    <div className="header-actions"><div className="voice-counter">{String(Math.max(sampleCount, voices)).padStart(2, '0')} / {maxSamples} VOICES</div>
      <nav><PixelButton active={page === 'main'} onClick={() => setPage('main')}>MAIN</PixelButton><PixelButton active={page === 'settings'} onClick={() => setPage('settings')}>SETTINGS</PixelButton></nav>
    </div>
  </header>
}

function SettingsPage({ sampleCount, maximumSampleCount, effectEnabled }: {
  sampleCount: number, maximumSampleCount: number, effectEnabled: boolean[]
}) {
  const effects = ['SCRAMBLE', 'MELT', 'SMEAR', 'SPECTRAL DRAW']
  return <div className="settings-page">
    <RecompilerPanel title="RUNTIME" className="settings-block"><dl><dt>EDITOR</dt><dd>{isRunningInJuce ? 'WEBVIEW2 CONNECTED' : 'BROWSER PREVIEW'}</dd><dt>VISUAL RATE</dt><dd>30 FPS</dd><dt>SPECTRAL MASK</dt><dd>128 × 64 CELLS</dd></dl></RecompilerPanel>
    <RecompilerPanel title="SAMPLE LIBRARY" className="settings-block"><dl><dt>LOADED</dt><dd>{sampleCount} / {maximumSampleCount}</dd><dt>FORMATS</dt><dd>WAV / AIFF / MP3 / FLAC</dd></dl><PixelButton onClick={() => sendPluginCommand('importSamples')}>ADD SAMPLES…</PixelButton></RecompilerPanel>
    <RecompilerPanel title="EFFECT ENGINES" className="settings-block effect-settings">
      {effects.map((name, effect) => <PixelButton key={name} active={effectEnabled[effect] !== false}
        onClick={() => sendPluginCommand('setEffectEnabled', { effect, enabled: effectEnabled[effect] === false })}>{name}</PixelButton>)}
      <PixelButton onClick={() => sendPluginCommand('regenerateSeed')}>NEW RANDOM SEED</PixelButton>
      <PixelButton onClick={() => sendPluginCommand('resetSpectral')}>CLEAR SPECTRAL MASK</PixelButton>
    </RecompilerPanel>
    <RecompilerPanel title="ABOUT" className="settings-block settings-about"><h2>RECOMPILER.DLL</h2><p>RANDOM SAMPLE INSTRUMENT</p><p>The C++ engine remains authoritative for audio, automation, and project state.</p></RecompilerPanel>
  </div>
}

export default function App() {
  const backendState = useBackendState()
  const visualisation = useVisualisationState()
  const [page, setPage] = useState('main')
  const samples = backendState?.samples ?? []
  const selectedSample = useMemo(() => samples.find((sample) => sample.id === backendState?.selectedSampleId), [samples, backendState?.selectedSampleId])
  return <main className="recompiler-shell">
    <Header page={page} setPage={setPage} sampleCount={backendState?.sampleCount ?? 0} voices={visualisation.voiceCount} maxSamples={backendState?.maximumSampleCount ?? 20} />
    {page === 'settings' ? <SettingsPage sampleCount={backendState?.sampleCount ?? 0} maximumSampleCount={backendState?.maximumSampleCount ?? 20} effectEnabled={backendState?.effectEnabled ?? [true, true, true, true]} /> : <>
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
        <EffectModule title="SCRAMBLE" id="scrambleAmount" type="scramble" visualisation={visualisation} />
        <EffectModule title="MELT" id="meltAmount" type="melt" visualisation={visualisation} />
        <EffectModule title="SMEAR" id="smearAmount" type="smear" visualisation={visualisation} />
        <SpectralModule values={backendState?.spectralCanvas ?? []} width={backendState?.spectralWidth ?? 128} height={backendState?.spectralHeight ?? 64} scan={visualisation.spectralScan} spectrum={visualisation.spectrum} />
        <OutputModule leftPeak={visualisation.outputPeakLeft ?? visualisation.outputPeak} rightPeak={visualisation.outputPeakRight ?? visualisation.outputPeak} muted={backendState?.outputMuted ?? false} />
      </div>
    </>}
  </main>
}
