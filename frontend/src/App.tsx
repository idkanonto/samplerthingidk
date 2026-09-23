import { useEffect, useMemo, useState } from 'react'
import {
  isRunningInJuce,
  onCommandResult,
  sendPluginCommand,
  useBackendState,
  usePluginParameter,
  useVisualisationState
} from './juceBridge'

function ParameterSlider({ id }: { id: string }) {
  const parameter = usePluginParameter(id)
  const descriptor = parameter.descriptor
  if (!descriptor) return <p className="error">Parameter “{id}” was not reported by C++.</p>
  const finish = () => parameter.endGesture()
  return (
    <div className="control">
      <label htmlFor={id}>{descriptor.name}</label>
      <input id={id} type="range" min={descriptor.min} max={descriptor.max}
        step={descriptor.interval || 'any'} value={parameter.value}
        onPointerDown={parameter.beginGesture}
        onChange={(event) => parameter.setValue(Number(event.target.value))}
        onPointerUp={finish} onPointerCancel={finish} />
      <output>{parameter.value.toFixed(1)} {descriptor.label}</output>
    </div>
  )
}

function ParameterToggle({ id }: { id: string }) {
  const parameter = usePluginParameter(id)
  const descriptor = parameter.descriptor
  if (!descriptor) return <p className="error">Parameter “{id}” was not reported by C++.</p>
  const toggle = () => {
    parameter.beginGesture()
    parameter.setValue(parameter.value >= 0.5 ? 0 : 1)
    parameter.endGesture()
  }
  return (
    <button className={`toggle ${parameter.value >= 0.5 ? 'active' : ''}`} onClick={toggle}>
      {descriptor.name}: {parameter.value >= 0.5 ? 'ON' : 'OFF'}
    </button>
  )
}

export default function App() {
  const backendState = useBackendState()
  const visualisation = useVisualisationState()
  const [proofCount, setProofCount] = useState(0)
  useEffect(() => onCommandResult((result) => {
    if (result.type === 'proofAcknowledged') setProofCount(Number(result.count))
  }), [])
  const selectedSample = useMemo(() => backendState?.samples.find(
    (sample) => sample.id === backendState.selectedSampleId), [backendState])

  return (
    <main>
      <header>
        <div>
          <h1>recompiler.dll</h1>
          <p>WEBVIEW FOUNDATION TEST</p>
        </div>
        <span className={`status ${isRunningInJuce ? 'connected' : ''}`}>
          {isRunningInJuce ? 'JUCE CONNECTED' : 'BROWSER MOCK'}
        </span>
      </header>

      <section>
        <h2>GENERIC PARAMETER BRIDGE</h2>
        <ParameterSlider id="output" />
        <ParameterToggle id="midiPitch" />
      </section>

      <section className="two-column">
        <div>
          <h2>C++ STATE</h2>
          <dl>
            <dt>Selected sample</dt><dd>{selectedSample?.name ?? 'No sample selected'}</dd>
            <dt>Sample pool</dt><dd>{backendState?.sampleCount ?? 0} / {backendState?.maximumSampleCount ?? 20}</dd>
            <dt>Voice count</dt><dd>{visualisation.voiceCount}</dd>
          </dl>
        </div>
        <div>
          <h2>REALTIME SNAPSHOT</h2>
          <div className="meter"><i style={{ width: `${visualisation.outputPeak * 100}%` }} /></div>
          <p className="muted">Output peak: {(visualisation.outputPeak * 100).toFixed(1)}%</p>
        </div>
      </section>

      <section>
        <h2>STRUCTURED COMMAND BRIDGE</h2>
        <div className="button-row">
          <button onClick={() => sendPluginCommand('proof')}>SEND TEST COMMAND</button>
          <button onClick={() => sendPluginCommand('importSamples')}>IMPORT SAMPLE…</button>
          <span>Acknowledgements: {proofCount}</span>
        </div>
      </section>
    </main>
  )
}
