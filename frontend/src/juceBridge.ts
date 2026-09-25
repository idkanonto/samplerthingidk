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
  sourceKey: number
  transpose: number
  fineTune: number
  gainDb: number
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
  outputMuted: boolean
  importMessage: string
  uiScale: number
  samples: SampleSummary[]
  spectralWidth: number
  spectralHeight: number
  spectralCanvas: number[]
  effectEnabled: boolean[]
}

export type VisualisationState = {
  outputPeak: number
  outputPeakLeft: number
  outputPeakRight: number
  voiceCount: number
  scramblePhase: number
  scrambleFlags: number
  meltStretch: number
  meltProgress: number
  meltFlags: number
  smearActivity: number
  smearGain: number
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
  { id: 'scrambleAmount', name: 'Scramble', label: '%', value: 42, defaultValue: 42,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'meltAmount', name: 'Melt', label: '%', value: 63, defaultValue: 63,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'smearAmount', name: 'Smear', label: '%', value: 28, defaultValue: 28,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'spectralDepth', name: 'Spectral Depth', label: '%', value: 71, defaultValue: 71,
    min: 0, max: 100, interval: 1, numSteps: 101, isDiscrete: false, isBoolean: false },
  { id: 'output', name: 'Vol', label: '%', value: 100, defaultValue: 100,
    min: 0, max: 125, interval: 0.1, numSteps: 1251, isDiscrete: false, isBoolean: false },
  { id: 'globalPitch', name: 'Global Pitch', label: 'st', value: 0, defaultValue: 0,
    min: -12, max: 12, interval: 1, numSteps: 25, isDiscrete: true, isBoolean: false },
  { id: 'midiPitch', name: 'Chords', label: '', value: 0, defaultValue: 0,
    min: 0, max: 1, interval: 1, numSteps: 2, isDiscrete: true, isBoolean: true },
  { id: 'voiceMode', name: 'Voice Mode', label: '', value: 0, defaultValue: 0,
    min: 0, max: 1, interval: 1, numSteps: 2, isDiscrete: true, isBoolean: true },
  { id: 'targetKey', name: 'Play In Key', label: '', value: 0, defaultValue: 0,
    min: 0, max: 12, interval: 1, numSteps: 13, isDiscrete: true, isBoolean: false }
]

const descriptors = new Map<string, ParameterDescriptor>(
  (window.__JUCE__?.initialisationData?.parameters?.[0] ?? fallbackParameters)
    .map((parameter) => [parameter.id, parameter])
)

const parameterSubscribers = new Map<string, Set<(value: number) => void>>()
let backendState: BackendState | null = null
let visualisationState: VisualisationState = {
  outputPeak: 0, outputPeakLeft: 0, outputPeakRight: 0, voiceCount: 0,
  scramblePhase: 0, scrambleFlags: 0, meltStretch: 0,
  meltProgress: 0, meltFlags: 0, smearActivity: 0, smearGain: 0, spectralScan: 0
}
const backendSubscribers = new Set<(state: BackendState) => void>()
const visualisationSubscribers = new Set<(state: VisualisationState) => void>()

const backend = window.__JUCE__?.backend

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
  backend?.emitEvent('backendCommand', { type, ...payload })
}

export function postDroppedFiles(files: FileList) {
  const webview = window.chrome?.webview
  if (!webview?.postMessageWithAdditionalObjects || files.length === 0) return false
  webview.postMessageWithAdditionalObjects('__recompilerFileDrop', files)
  return true
}
