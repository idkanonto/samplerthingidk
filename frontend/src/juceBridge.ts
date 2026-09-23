import { useCallback, useEffect, useState } from 'react'

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
  samples: SampleSummary[]
  spectralWidth: number
  spectralHeight: number
  spectralCanvas: number[]
  scrambleFeatures: number
  meltFeatures: number
  smearFeatures: number
  effectEnabled: boolean[]
}

export type VisualisationState = {
  outputPeak: number
  voiceCount: number
  scramblePhase: number
  meltStretch: number
  meltProgress: number
  smearActivity: number
  smearGain: number
  spectralScan: number
  spectrum?: number[]
}

type JuceBackend = {
  emitEvent: (eventId: string, payload: unknown) => void
  addEventListener: (eventId: string, callback: (payload: any) => void) => string
  removeEventListener: (token: string) => void
}

declare global {
  interface Window {
    __JUCE__?: {
      backend: JuceBackend
      initialisationData?: { parameters?: ParameterDescriptor[][] }
    }
  }
}

const fallbackParameters: ParameterDescriptor[] = [
  { id: 'output', name: 'Output', label: 'dB', value: 0, defaultValue: 0,
    min: -60, max: 6, interval: 0.1, numSteps: 661, isDiscrete: false, isBoolean: false },
  { id: 'midiPitch', name: 'Chords', label: '', value: 0, defaultValue: 0,
    min: 0, max: 1, interval: 1, numSteps: 2, isDiscrete: true, isBoolean: true }
]

const descriptors = new Map<string, ParameterDescriptor>(
  (window.__JUCE__?.initialisationData?.parameters?.[0] ?? fallbackParameters)
    .map((parameter) => [parameter.id, parameter])
)

const parameterSubscribers = new Map<string, Set<(value: number) => void>>()
let backendState: BackendState | null = null
let visualisationState: VisualisationState = {
  outputPeak: 0, voiceCount: 0, scramblePhase: 0, meltStretch: 0,
  meltProgress: 0, smearActivity: 0, smearGain: 0, spectralScan: 0
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

  useEffect(() => {
    const listeners = parameterSubscribers.get(id) ?? new Set<(value: number) => void>()
    listeners.add(setLocalValue)
    parameterSubscribers.set(id, listeners)
    return () => { listeners.delete(setLocalValue) }
  }, [id])

  const beginGesture = useCallback(() => {
    backend?.emitEvent('parameterGesture', { id, phase: 'begin' })
  }, [id])
  const setValue = useCallback((nextValue: number) => {
    setLocalValue(nextValue)
    backend?.emitEvent('parameterValue', { id, value: nextValue })
  }, [id])
  const endGesture = useCallback(() => {
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

export function onCommandResult(callback: (payload: any) => void) {
  if (!backend) return () => undefined
  const token = backend.addEventListener('commandResult', callback)
  return () => backend.removeEventListener(token)
}

export const isRunningInJuce = Boolean(backend)
