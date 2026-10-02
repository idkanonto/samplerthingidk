import { useCallback, useEffect, useRef, useState } from 'react'
import { sendPluginCommand, type VisualisationState } from './juceBridge'

const setup = (canvas: HTMLCanvasElement, width: number, height: number, clear = true) => {
  const bounds = canvas.getBoundingClientRect()
  const cssWidth = Math.max(1, bounds.width || width)
  const cssHeight = Math.max(1, bounds.height || height)
  const pixelRatio = Math.max(1, window.devicePixelRatio || 1)
  const backingWidth = Math.max(1, Math.round(cssWidth * pixelRatio))
  const backingHeight = Math.max(1, Math.round(cssHeight * pixelRatio))
  const resized = canvas.width !== backingWidth || canvas.height !== backingHeight
  if (canvas.width !== backingWidth) canvas.width = backingWidth
  if (canvas.height !== backingHeight) canvas.height = backingHeight
  const context = canvas.getContext('2d')
  if (!context) return null
  context.setTransform(pixelRatio * cssWidth / width, 0, 0,
    pixelRatio * cssHeight / height, 0, 0)
  context.imageSmoothingEnabled = false
  if (clear || resized) {
    context.fillStyle = '#101010'
    context.fillRect(0, 0, width, height)
  }
  return context
}

export function WaveformCanvas({ waveform }: { waveform?: [number, number][] }) {
  const ref = useRef<HTMLCanvasElement>(null)
  useEffect(() => {
    const canvas = ref.current
    if (!canvas) return
    const context = setup(canvas, 256, 96)
    if (!context) return
    if (!waveform?.length) return
    context.fillStyle = '#eeeeee'
    waveform.forEach((pair, index) => {
      const x0 = Math.floor(index * 256 / waveform.length)
      const x1 = Math.max(x0 + 1, Math.ceil((index + 1) * 256 / waveform.length))
      const top = Math.floor(48 - Math.max(-1, Math.min(1, pair[1])) * 43)
      const bottom = Math.ceil(48 - Math.max(-1, Math.min(1, pair[0])) * 43)
      context.fillRect(x0, top, Math.max(1, x1 - x0), Math.max(1, bottom - top))
    })
  }, [waveform])
  return <canvas ref={ref} className="pixel-canvas waveform-canvas" aria-label="Selected sample waveform" />
}

export function FaultCanvas({ pressure, visualisation, active }: {
  pressure: number, visualisation: VisualisationState, active: boolean
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const data = useRef({ pressure, visualisation })
  data.current = { pressure, visualisation }
  useEffect(() => {
    const canvas = ref.current
    if (!canvas || !active) return
    type Mark = { x: number, y: number, mutation: number, direction: number, born: number }
    let marks: Mark[] = []
    let frame = 0
    let previousFrame = 0
    let previousProgress = -1
    let previousMutation = -1
    const draw = (time: number) => {
      frame = requestAnimationFrame(draw)
      if (time - previousFrame < 33) return
      previousFrame = time
      const current = data.current
      const telemetry = current.visualisation
      const mutation = telemetry.faultMutation || 0
      const progress = Math.max(0, Math.min(1, telemetry.faultProgress || 0))
      const changed = mutation !== previousMutation || Math.abs(progress - previousProgress) > .008
        || progress < previousProgress
      if (changed && mutation > 0 && current.pressure > 0) {
        const x = 8 + progress * 240
        const divisionLift = Math.max(0, Math.min(24, (telemetry.faultDivision || 16) * 1.5))
        const phase = progress * Math.PI * 2
        const y = mutation === 1
          ? 70 - Math.sin(phase * (telemetry.faultResampleSemitones > 0 ? 2 : 1)) * (24 + divisionLift)
          : mutation === 2 ? 24 + ((Math.floor(progress * 64) * 17 + telemetry.faultDivision) % 88)
            : 70 + Math.sin(phase * 3) * (18 + divisionLift)
        marks.push({ x, y, mutation, direction: telemetry.faultResampleSemitones, born: time })
        marks = marks.slice(-96)
      }
      previousProgress = progress
      previousMutation = mutation

      const context = setup(canvas, 256, 140, false)
      if (!context) return
      context.fillStyle = 'rgba(16,16,16,.18)'
      context.fillRect(0, 0, 256, 140)
      marks = marks.filter((mark) => time - mark.born < 1300)
      for (const mark of marks) {
        const age = (time - mark.born) / 1300
        context.globalAlpha = Math.max(0, 1 - age) * .92
        context.strokeStyle = '#eeeeee'
        context.fillStyle = '#eeeeee'
        if (mark.mutation === 1) {
          context.beginPath()
          context.moveTo(mark.x - 9, mark.y + (mark.direction > 0 ? 5 : -5))
          context.lineTo(mark.x, mark.y)
          context.lineTo(mark.x + 9, mark.y + (mark.direction > 0 ? -5 : 5))
          context.stroke()
        } else if (mark.mutation === 2) {
          context.fillRect(Math.round(mark.x) - 2, Math.round(mark.y) - 2, 4, 4)
          context.globalAlpha *= .55
          context.fillRect(Math.round(mark.x) + 4, 132 - Math.round(mark.y), 2, 2)
        } else {
          context.beginPath()
          context.moveTo(mark.x - 8, 140 - mark.y)
          context.lineTo(mark.x + 8, mark.y)
          context.stroke()
        }
      }
      context.globalAlpha = 1
      if (mutation > 0) {
        context.fillStyle = '#eeeeee'
        context.fillRect(Math.round(7 + progress * 240), 18, 1, 112)
      }
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [active])
  return <canvas ref={ref} className="pixel-canvas fault-canvas"
    aria-label="Live Fault segment activity, mutation type, division, and progress" />
}

export function EffectCanvas({ amount, visualisation, active }: {
  amount: number, visualisation: VisualisationState, active: boolean
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const data = useRef({ amount, visualisation })
  data.current = { amount, visualisation }
  useEffect(() => {
    const canvas = ref.current
    if (!canvas || !active) return
    let frame = 0
    let previous = 0
    const draw = (time: number) => {
      frame = requestAnimationFrame(draw)
      if (time - previous < 33) return
      previous = time
      const context = setup(canvas, 128, 68, false)
      if (!context) return
      const current = data.current
      const telemetry = current.visualisation
      const left = telemetry.bleedScopeLeft ?? []
      const right = telemetry.bleedScopeRight ?? []
      const count = Math.min(left.length, right.length)
      const activity = Math.max(0, Math.min(1, telemetry.smearActivity || 0))
      const gain = Math.max(0, Math.min(1, telemetry.smearGain || 0))
      context.fillStyle = '#101010'
      context.fillRect(0, 0, 128, 68)
      if (count > 1 && current.amount > 0 && activity > 0) {
        // Audio amplitude and stereo difference shape thin facets; silence draws nothing.
        context.strokeStyle = '#eeeeee'
        context.lineWidth = .75
        for (let facet = 0; facet < 8; facet += 1) {
          const index = Math.min(count - 1, Math.floor(facet * count / 8))
          const amplitude = Math.min(1, Math.abs(left[index]) + Math.abs(right[index]))
          if (amplitude < .002) continue
          const difference = Math.max(-1, Math.min(1, left[index] - right[index]))
          const x = 10 + facet * 15
          const y = 34 + difference * 18
          const reach = 2 + amplitude * 25
          context.globalAlpha = Math.min(1, .25 + amplitude * .7 + gain * .1)
          context.beginPath()
          context.moveTo(x - 3, y + reach * .5)
          context.lineTo(x + difference * 8, y - reach)
          context.lineTo(x + 4, y + reach * .2)
          context.closePath()
          context.stroke()
        }
      }
      context.globalAlpha = 1
      context.lineWidth = 1
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [active])
  return <canvas ref={ref} className="pixel-canvas effect-canvas" aria-label="Live Bleed crystalline audio facets" />
}

const paintLine = (mask: Float32Array, width: number, height: number,
  from: [number, number], to: [number, number], value: number) => {
  let [x0, y0] = from
  const [x1, y1] = to
  const dx = Math.abs(x1 - x0)
  const sx = x0 < x1 ? 1 : -1
  const dy = -Math.abs(y1 - y0)
  const sy = y0 < y1 ? 1 : -1
  let error = dx + dy
  while (true) {
    for (let oy = -1; oy <= 1; oy += 1) for (let ox = -1; ox <= 1; ox += 1) {
      const x = x0 + ox, y = y0 + oy
      if (x >= 0 && y >= 0 && x < width && y < height) mask[y * width + x] = value
    }
    if (x0 === x1 && y0 === y1) break
    const twice = 2 * error
    if (twice >= dy) { error += dy; x0 += sx }
    if (twice <= dx) { error += dx; y0 += sy }
  }
}

export function SpectralDrawCanvas({ values, width, height, resetSignal, active }: {
  values: number[], width: number, height: number, resetSignal: number, active: boolean
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const raster = useRef<HTMLCanvasElement | null>(null)
  const mask = useRef(new Float32Array(Math.max(1, width * height)))
  const drawing = useRef(false)
  const erasing = useRef(false)
  const previous = useRef<[number, number] | null>(null)
  const keyboardCursor = useRef<[number, number]>([Math.floor(width / 2), Math.floor(height / 2)])
  const [revision, setRevision] = useState(0)

  useEffect(() => {
    mask.current = new Float32Array(Math.max(1, width * height))
    values.slice(0, width * height).forEach((value, index) => { mask.current[index] = value })
    setRevision((current) => current + 1)
  }, [values, width, height])

  useEffect(() => {
    if (resetSignal === 0) return
    mask.current.fill(0)
    setRevision((current) => current + 1)
  }, [resetSignal])

  useEffect(() => {
    const canvas = ref.current
    if (!canvas || !active) return
    const context = setup(canvas, width, height)
    if (!context) return
    const current = mask.current
    const bitmap = raster.current ?? document.createElement('canvas')
    raster.current = bitmap
    if (bitmap.width !== width) bitmap.width = width
    if (bitmap.height !== height) bitmap.height = height
    const bitmapContext = bitmap.getContext('2d')
    if (!bitmapContext) return
    const pixels = bitmapContext.createImageData(width, height)
    for (let index = 0; index < width * height; index += 1) {
      const value = current[index] ?? 0
      const shade = value > .02 ? Math.round(95 + value * 142) : 16
      const offset = index * 4
      pixels.data[offset] = shade
      pixels.data[offset + 1] = shade
      pixels.data[offset + 2] = shade
      pixels.data[offset + 3] = 255
    }
    bitmapContext.putImageData(pixels, 0, 0)
    context.save()
    context.setTransform(1, 0, 0, 1, 0, 0)
    context.imageSmoothingEnabled = false
    context.drawImage(bitmap, 0, 0, canvas.width, canvas.height)
    context.restore()
  }, [active, revision, width, height])

  const point = useCallback((event: React.PointerEvent<HTMLCanvasElement>): [number, number] => {
    const bounds = event.currentTarget.getBoundingClientRect()
    return [Math.max(0, Math.min(width - 1, Math.floor((event.clientX - bounds.left) / bounds.width * width))),
      Math.max(0, Math.min(height - 1, Math.floor((event.clientY - bounds.top) / bounds.height * height)))]
  }, [width, height])
  const apply = useCallback((event: React.PointerEvent<HTMLCanvasElement>) => {
    const next = point(event)
    paintLine(mask.current, width, height, previous.current ?? next, next, erasing.current ? 0 : 1)
    previous.current = next
    setRevision((current) => current + 1)
  }, [height, point, width])
  const finish = useCallback((event: React.PointerEvent<HTMLCanvasElement>) => {
    if (!drawing.current) return
    drawing.current = false
    previous.current = null
    event.currentTarget.releasePointerCapture(event.pointerId)
    sendPluginCommand('setSpectralCanvas', { values: Array.from(mask.current) })
  }, [])
  const handleKey = (event: React.KeyboardEvent<HTMLCanvasElement>) => {
    const [x, y] = keyboardCursor.current
    if (event.key.startsWith('Arrow')) {
      event.preventDefault()
      const step = event.shiftKey ? 8 : 1
      keyboardCursor.current = [
        Math.max(0, Math.min(width - 1, x + (event.key === 'ArrowRight' ? step : event.key === 'ArrowLeft' ? -step : 0))),
        Math.max(0, Math.min(height - 1, y + (event.key === 'ArrowDown' ? step : event.key === 'ArrowUp' ? -step : 0)))
      ]
      setRevision((current) => current + 1)
    } else if (event.key === ' ' || event.key === 'Enter' || event.key === 'Delete' || event.key === 'Backspace') {
      event.preventDefault()
      paintLine(mask.current, width, height, keyboardCursor.current, keyboardCursor.current,
        event.key === 'Delete' || event.key === 'Backspace' || event.shiftKey ? 0 : 1)
      setRevision((current) => current + 1)
      sendPluginCommand('setSpectralCanvas', { values: Array.from(mask.current) })
    }
  }
  return <div className="spectral-editor">
    <canvas ref={ref} className="pixel-canvas spectral-canvas" aria-label="Spectral mask drawing surface. Drag to draw; right-click or Shift-drag erases." tabIndex={0}
      onKeyDown={handleKey}
      onContextMenu={(event) => event.preventDefault()}
      onPointerDown={(event) => { drawing.current = true; erasing.current = event.button === 2 || event.shiftKey || event.altKey; previous.current = null; event.currentTarget.setPointerCapture(event.pointerId); apply(event) }}
      onPointerMove={(event) => { if (drawing.current) apply(event) }} onPointerUp={finish} onPointerCancel={finish} />
  </div>
}

export function StereoMeterCanvas({ left, right }: { left: number, right: number }) {
  const ref = useRef<HTMLCanvasElement>(null)
  const levels = useRef([0, 0])
  const peaks = useRef([left, right])
  peaks.current = [left, right]
  useEffect(() => {
    const canvas = ref.current
    if (!canvas) return
    let frame = 0
    let previous = 0
    const draw = (time: number) => {
      frame = requestAnimationFrame(draw)
      if (time - previous < 33) return
      previous = time
      levels.current[0] = Math.max(Math.max(0, peaks.current[0]), levels.current[0] * .86)
      levels.current[1] = Math.max(Math.max(0, peaks.current[1]), levels.current[1] * .86)
      const context = setup(canvas, 56, 142)
      if (!context) return
      context.font = 'bold 8px monospace'; context.textAlign = 'center'; context.fillStyle = '#eeeeee'
      context.fillText('L', 17, 9); context.fillText('R', 41, 9)
      for (let channel = 0; channel < 2; channel += 1) {
        const db = 20 * Math.log10(Math.max(.004, levels.current[channel]))
        const lit = Math.round(Math.max(0, Math.min(1, (db + 48) / 52)) * 20)
        for (let segment = 0; segment < 20; segment += 1) {
          context.fillStyle = segment < lit ? '#eeeeee' : '#383838'
          context.fillRect(8 + channel * 24, 130 - segment * 5, 17, 3)
        }
      }
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [])
  return <canvas ref={ref} className="pixel-canvas stereo-meter-canvas" aria-label="Stereo output meter" />
}
