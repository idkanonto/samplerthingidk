import { useCallback, useEffect, useRef, useState } from 'react'

export type ParameterDescriptor = {
  id: string
  name: string
  label: string
  value: number
  defaultValue: number
  min: number
  max: number
  interval: number
  numSteps: number
  isDiscrete: boolean
  isBoolean: boolean
  choices?: string[]
}

export type SampleSummary = {
  id: string
  name: string
  enabled: boolean
  missing: boolean
  start: number
  end: number
  transpose: number
  fineTune: number
  gainDb: number
  stretch: number
  stretchPending: boolean
  sampleRate: number
  bitDepth: number
  durationSeconds: number
  waveform?: [number, number][]
}

export type BackendState = {
  selectedSampleId: string
  sampleCount: number
  maximumSampleCount: number
  voiceCount: number
  importMessage: string
  uiScale: number
  samples: SampleSummary[]
  spectralWidth: number
  spectralHeight: number
  spectralCanvas: number[]
  effectEnabled: boolean[]
  faultMutations: number
}

export type VisualisationState = {
  outputPeak: number
  outputPeakLeft: number
  outputPeakRight: number
  voiceCount: number
  faultMutation: number
  faultDivision: number
  faultProgress: number
  faultResampleSemitones: number
  smearActivity: number
  smearGain: number
  bleedScopeLeft?: number[]
  bleedScopeRight?: number[]
  spectralScan: number
  spectrum?: number[]
}

type JuceBackend = {
  emitEvent: (eventId: string, payload: unknown) => void
  addEventListener: (eventId: string, callback: (payload: any) => void) => string
}

declare global {
  interface Window {
    __JUCE__?: {
      backend: JuceBackend
      initialisationData?: { parameters?: ParameterDescriptor[][] }
    }
    chrome?: { webview?: {
      postMessageWithAdditionalObjects?: (message: string, objects: FileList) => void
    } }
  }
}

const fallbackParameters: ParameterDescriptor[] = [
  { id: 'faultPressure', name: 'Fault Pressure', label: '%', value: 0, defaultValue: 0,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'smearAmount', name: 'Bleed Pressure', label: '%', value: 0, defaultValue: 0,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'bleedMix', name: 'Bleed Mix', label: '%', value: 50, defaultValue: 50,
    min: 0, max: 100, interval: 0.1, numSteps: 1001, isDiscrete: false, isBoolean: false },
  { id: 'bleedGrainSize', name: 'Bleed Grain Size', label: 'ms', value: 40, defaultValue: 40,
    min: 8, max: 120, interval: 0.1, numSteps: 1121, isDiscrete: false, isBoolean: false },
  { id: 'bleedGrainPitch', name: 'Bleed Grain Pitch', label: '%', value: 0, defaultValue: 0,
    min: -100, max: 100, interval: 0.1, numSteps: 2001, isDiscrete: false, isBoolean: false },
  { id: 'spectralDepth', name: 'Etch Pressure', label: '%', value: 0, defaultValue: 0,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'output', name: 'Vol', label: '%', value: 100, defaultValue: 100,
    min: 0, max: 125, interval: 0.1, numSteps: 1251, isDiscrete: false, isBoolean: false },
  { id: 'globalPitch', name: 'Global Pitch', label: 'st', value: 0, defaultValue: 0,
    min: -12, max: 12, interval: 1, numSteps: 25, isDiscrete: true, isBoolean: false },
  { id: 'midiPitch', name: 'Stack', label: '', value: 0, defaultValue: 0,
    min: 0, max: 1, interval: 1, numSteps: 2, isDiscrete: true, isBoolean: true },
  { id: 'voiceMode', name: 'Voice Mode', label: '', value: 0, defaultValue: 0,
    min: 0, max: 1, interval: 1, numSteps: 2, isDiscrete: true, isBoolean: true }
]

const descriptors = new Map<string, ParameterDescriptor>(
  (window.__JUCE__?.initialisationData?.parameters?.[0] ?? fallbackParameters)
    .map((parameter) => [parameter.id, parameter])
)

const parameterSubscribers = new Map<string, Set<(value: number) => void>>()
const previewWaveform = Array.from({ length: 128 }, (_, index): [number, number] => {
  const envelope = .22 + .68 * Math.sin(index / 127 * Math.PI)
  const sample = Math.sin(index * .34) * envelope
  return [Math.min(0, sample), Math.max(0, sample)]
})
const previewSample = (): SampleSummary => ({
  id: 'preview-sample', name: 'approved_loop.wav', enabled: true, missing: false,
  start: 0, end: 1, transpose: 0, fineTune: 0, gainDb: 0, stretch: 1,
  stretchPending: false, sampleRate: 48000, bitDepth: 24, durationSeconds: 3.2,
  waveform: previewWaveform
})
const previewBackendState = (): BackendState => ({
  selectedSampleId: '', sampleCount: 0, maximumSampleCount: 20, voiceCount: 0,
  importMessage: '', uiScale: 1, samples: [], spectralWidth: 128, spectralHeight: 64,
  spectralCanvas: [], effectEnabled: [true, true, true], faultMutations: 7
})
let backendState: BackendState | null = window.__JUCE__?.backend ? null : previewBackendState()
let visualisationState: VisualisationState = {
  outputPeak: 0, outputPeakLeft: 0, outputPeakRight: 0, voiceCount: 0,
  faultMutation: 0, faultDivision: 16, faultProgress: 0, faultResampleSemitones: 0,
  smearActivity: 0, smearGain: 0, bleedScopeLeft: [], bleedScopeRight: [], spectralScan: 0
}
const backendSubscribers = new Set<(state: BackendState) => void>()
const visualisationSubscribers = new Set<(state: VisualisationState) => void>()

const backend = window.__JUCE__?.backend

const publishPreviewState = (next: BackendState) => {
  backendState = next
  backendSubscribers.forEach((listener) => listener(next))
}

backend?.addEventListener('parameterChanged', (update) => {
  const descriptor = descriptors.get(update.id)
  if (descriptor) descriptor.value = Number(update.value)
  parameterSubscribers.get(update.id)?.forEach((listener) => listener(Number(update.value)))
})
backend?.addEventListener('backendState', (state: BackendState) => {
  backendState = state
  backendSubscribers.forEach((listener) => listener(state))
})
backend?.addEventListener('visualisationState', (state: VisualisationState) => {
  visualisationState = state
  visualisationSubscribers.forEach((listener) => listener(state))
})

export function usePluginParameter(id: string) {
  const descriptor = descriptors.get(id)
  const [value, setLocalValue] = useState(descriptor?.value ?? 0)
  const gestureActive = useRef(false)

  useEffect(() => {
    const listeners = parameterSubscribers.get(id) ?? new Set<(value: number) => void>()
    listeners.add(setLocalValue)
    parameterSubscribers.set(id, listeners)
    return () => {
      listeners.delete(setLocalValue)
      if (gestureActive.current) {
        gestureActive.current = false
        backend?.emitEvent('parameterGesture', { id, phase: 'end' })
      }
    }
  }, [id])

  const beginGesture = useCallback(() => {
    if (gestureActive.current) return
    gestureActive.current = true
    backend?.emitEvent('parameterGesture', { id, phase: 'begin' })
  }, [id])
  const setValue = useCallback((nextValue: number) => {
    const currentDescriptor = descriptors.get(id)
    const finiteValue = Number.isFinite(nextValue) ? nextValue
      : (currentDescriptor?.defaultValue ?? 0)
    const safeValue = currentDescriptor
      ? Math.max(currentDescriptor.min, Math.min(currentDescriptor.max, finiteValue))
      : finiteValue
    if (currentDescriptor) currentDescriptor.value = safeValue
    setLocalValue(safeValue)
    backend?.emitEvent('parameterValue', { id, value: safeValue })
  }, [id])
  const endGesture = useCallback(() => {
    if (!gestureActive.current) return
    gestureActive.current = false
    backend?.emitEvent('parameterGesture', { id, phase: 'end' })
  }, [id])

  return { value, descriptor, beginGesture, setValue, endGesture }
}

export function useBackendState() {
  const [state, setState] = useState(backendState)
  useEffect(() => {
    backendSubscribers.add(setState)
    backend?.emitEvent('backendCommand', { type: 'requestState' })
    return () => { backendSubscribers.delete(setState) }
  }, [])
  return state
}

export function useVisualisationState() {
  const [state, setState] = useState(visualisationState)
  useEffect(() => {
    visualisationSubscribers.add(setState)
    return () => { visualisationSubscribers.delete(setState) }
  }, [])
  return state
}

export function sendPluginCommand(type: string, payload: Record<string, unknown> = {}) {
  if (backend) { backend.emitEvent('backendCommand', { type, ...payload }); return }
  if (!backendState) return
  if (type === 'importSamples') {
    const sample = previewSample()
    publishPreviewState({ ...backendState, selectedSampleId: sample.id, sampleCount: 1, samples: [sample] })
  } else if (type === 'removeSample') {
    const samples = backendState.samples.filter((sample) => sample.id !== payload.id)
    publishPreviewState({ ...backendState, samples, sampleCount: samples.length,
      selectedSampleId: samples[0]?.id ?? '' })
  } else if (type === 'selectSample') {
    publishPreviewState({ ...backendState, selectedSampleId: String(payload.id ?? '') })
  } else if (type === 'setSampleEnabled') {
    publishPreviewState({ ...backendState, samples: backendState.samples.map((sample) => sample.id === payload.id
      ? { ...sample, enabled: Boolean(payload.enabled) } : sample) })
  } else if (type === 'setSampleRegion') {
    publishPreviewState({ ...backendState, samples: backendState.samples.map((sample) => sample.id === payload.id
      ? { ...sample, start: Number(payload.start), end: Number(payload.end) } : sample) })
  } else if (type === 'setSampleProperty') {
    publishPreviewState({ ...backendState, samples: backendState.samples.map((sample) => sample.id === payload.id
      ? { ...sample, [String(payload.property)]: Number(payload.value) } : sample) })
  } else if (type === 'setEffectEnabled') {
    const effectEnabled = [...backendState.effectEnabled]
    effectEnabled[Number(payload.effect)] = Boolean(payload.enabled)
    publishPreviewState({ ...backendState, effectEnabled })
  } else if (type === 'setFaultMutations') {
    publishPreviewState({ ...backendState, faultMutations: Number(payload.mutations) })
  }
}

export function postDroppedFiles(files: FileList) {
  const webview = window.chrome?.webview
  if (files.length === 0) return false
  if (!webview?.postMessageWithAdditionalObjects) { sendPluginCommand('importSamples'); return true }
  webview.postMessageWithAdditionalObjects('__recompilerFileDrop', files)
  return true
}
