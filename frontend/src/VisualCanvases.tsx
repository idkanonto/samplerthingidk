import { useCallback, useEffect, useRef, useState } from 'react'
import { sendPluginCommand, type VisualisationState } from './juceBridge'

const setup = (canvas: HTMLCanvasElement, width: number, height: number) => {
  if (canvas.width !== width) canvas.width = width
  if (canvas.height !== height) canvas.height = height
  const context = canvas.getContext('2d')
  if (!context) return null
  context.imageSmoothingEnabled = false
  context.fillStyle = '#111210'
  context.fillRect(0, 0, width, height)
  return context
}

const grid = (context: CanvasRenderingContext2D, width: number, height: number, columns: number, rows: number) => {
  context.fillStyle = '#3e3f3b'
  for (let x = 1; x < columns; x += 1) context.fillRect(Math.round(x * width / columns), 0, 1, height)
  for (let y = 1; y < rows; y += 1) context.fillRect(0, Math.round(y * height / rows), width, 1)
}

export function WaveformCanvas({ waveform }: { waveform?: [number, number][] }) {
  const ref = useRef<HTMLCanvasElement>(null)
  useEffect(() => {
    const canvas = ref.current
    if (!canvas) return
    const context = setup(canvas, 256, 96)
    if (!context) return
    grid(context, 256, 96, 8, 4)
    if (!waveform?.length) return
    context.fillStyle = '#eeede5'
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

export function FaultCanvas({ pressure, visualisation }: {
  pressure: number, visualisation: VisualisationState
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const history = useRef<{ mutation: number, division: number }[]>([])
  const previousProgress = useRef(0)
  useEffect(() => {
    const progress = Math.max(0, Math.min(1, visualisation.faultProgress || 0))
    if (progress < previousProgress.current) {
      history.current = [...history.current.slice(-11), {
        mutation: visualisation.faultMutation || 0,
        division: visualisation.faultDivision || 16
      }]
    }
    previousProgress.current = progress
    const canvas = ref.current
    if (!canvas) return
    const context = setup(canvas, 256, 140)
    if (!context) return
    grid(context, 256, 140, 16, 8)
    const segments = [...history.current, {
      mutation: visualisation.faultMutation || 0,
      division: visualisation.faultDivision || 16
    }].slice(-12)
    const blockWidth = 256 / 12
    segments.forEach((segment, index) => {
      const x = Math.floor(index * blockWidth) + 2
      const width = Math.max(3, Math.floor(blockWidth) - 4)
      context.fillStyle = segment.mutation === 0 ? '#343630' : '#eeede5'
      if (segment.mutation === 1) {
        for (let y = 18; y < 122; y += 12) context.fillRect(x, y, width, 2)
        for (let offset = 0; offset < width; offset += 5) context.fillRect(x + offset, 18 + offset * 2, 2, 80)
      } else if (segment.mutation === 2) {
        for (let y = 17; y < 124; y += 7) for (let px = 0; px < width; px += 4)
          if ((px + y + index) % 3 !== 0) context.fillRect(x + px, y, 2, 3)
      } else if (segment.mutation === 3) {
        for (let px = 0; px < width; px += 2) {
          const y = 70 - Math.round(Math.sin((px / Math.max(1, width)) * Math.PI * 3) * 34)
          context.fillRect(x + px, y, 3, 3)
        }
      } else context.fillRect(x, 68, width, 3)
      context.font = '600 9px "IBM Plex Mono Local", monospace'
      context.fillStyle = '#96958d'
      context.fillText(`1/${segment.division}`, x, 134)
    })
    const currentX = Math.floor((segments.length - 1) * blockWidth) + 1
    context.fillStyle = '#eeede5'
    context.fillRect(currentX, 6, Math.max(1, Math.floor(blockWidth * progress)), 4)
    context.fillStyle = '#96958d'
    context.font = '600 9px "IBM Plex Mono Local", monospace'
    const names = ['DRY', 'PULL', 'DUST', 'BEND']
    context.fillText(`${names[visualisation.faultMutation] ?? 'DRY'}  ${Math.round(pressure)}%`, 5, 12)
  }, [pressure, visualisation.faultDivision, visualisation.faultMutation, visualisation.faultProgress])
  return <canvas ref={ref} className="pixel-canvas fault-canvas"
    aria-label="Live Fault segment activity, mutation type, division, and progress" />
}

export function EffectCanvas({ amount, visualisation }: {
  type: 'smear', amount: number, visualisation: VisualisationState
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const data = useRef({ amount, visualisation })
  data.current = { amount, visualisation }
  useEffect(() => {
    const canvas = ref.current
    if (!canvas) return
    let frame = 0
    let previous = 0
    const draw = (time: number) => {
      frame = requestAnimationFrame(draw)
      if (time - previous < 33) return
      previous = time
      const context = setup(canvas, 128, 68)
      if (!context) return
      const current = data.current
      const telemetry = current.visualisation
      const strength = Math.max(0, Math.min(1, current.amount / 100))
      context.fillStyle = '#eeede5'
      const activity = Math.max(strength * .45, telemetry.smearActivity)
      const gain = Math.max(.2, telemetry.smearGain)
      const liveTime = telemetry.smearActivity > .002 || telemetry.smearGain > .002 ? time : 0
      for (let particle = 0; particle < 62; particle += 1) {
        const drift = (liveTime * .012 * (1 + activity) + particle * 19) % 148
        const x = Math.floor(drift - 10)
        const y = 8 + ((particle * 29 + Math.floor(liveTime * .018)) % 52)
        const length = 1 + Math.floor((particle % 7) * activity * 2.4)
        context.globalAlpha = .25 + ((particle * 13) % 70) / 100 * gain
        context.fillRect(x, y, Math.max(1, length), particle % 5 === 0 ? 2 : 1)
      }
      context.globalAlpha = 1
      for (let x = 0; x < 128; x += 3) {
        const amplitude = Math.sin(x * .23 + liveTime * .002) * (4 + strength * 8)
        context.fillRect(x, Math.round(34 + amplitude), 5 + Math.round(activity * 9), 1)
      }
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [])
  return <canvas ref={ref} className="pixel-canvas effect-canvas" aria-label="Bleed activity display" />
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

export function SpectralDrawCanvas({ values, width, height, scan, spectrum, resetSignal }: {
  values: number[], width: number, height: number, scan: number, spectrum?: number[], resetSignal: number
}) {
  const ref = useRef<HTMLCanvasElement>(null)
  const mask = useRef(new Float32Array(Math.max(1, width * height)))
  const drawing = useRef(false)
  const erasing = useRef(false)
  const previous = useRef<[number, number] | null>(null)
  const keyboardCursor = useRef<[number, number]>([Math.floor(width / 2), Math.floor(height / 2)])
  const [keyboardFocused, setKeyboardFocused] = useState(false)
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
    if (!canvas) return
    const context = setup(canvas, width, height)
    if (!context) return
    grid(context, width, height, 8, 4)
    const current = mask.current
    for (let y = 0; y < height; y += 1) for (let x = 0; x < width; x += 1) {
      const value = current[y * width + x]
      if (value <= .02) continue
      const shade = Math.round(95 + value * 142)
      context.fillStyle = `rgb(${shade},${shade},${shade})`
      context.fillRect(x, y, 1, 1)
    }
    if (spectrum?.length) {
      context.fillStyle = '#74756f'
      spectrum.forEach((value, index) => {
        const y = height - 1 - Math.floor(index * height / spectrum.length)
        context.fillRect(0, y, Math.max(1, Math.round(value * width * .18)), 1)
      })
    }
    context.fillStyle = '#eeede5'
    context.fillRect(Math.max(0, Math.min(width - 1, Math.floor(scan * width))), 0, 1, height)
    if (keyboardFocused) {
      const [x, y] = keyboardCursor.current
      context.fillRect(Math.max(0, x - 2), y, 5, 1)
      context.fillRect(x, Math.max(0, y - 2), 1, 5)
    }
  }, [revision, scan, spectrum, width, height, keyboardFocused])

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
    <canvas ref={ref} className="pixel-canvas spectral-canvas" aria-label="Spectral mask drawing surface. Arrow keys move the cursor; Space draws; Delete erases." tabIndex={0}
      onFocus={() => setKeyboardFocused(true)} onBlur={() => setKeyboardFocused(false)} onKeyDown={handleKey}
      onContextMenu={(event) => event.preventDefault()}
      onPointerDown={(event) => { drawing.current = true; erasing.current = event.button === 2 || event.shiftKey || event.altKey; previous.current = null; event.currentTarget.setPointerCapture(event.pointerId); apply(event) }}
      onPointerMove={(event) => { if (drawing.current) apply(event) }} onPointerUp={finish} onPointerCancel={finish} />
    <span className="spectral-label high">HIGH</span><span className="spectral-label low">LOW</span><span className="spectral-label time">TIME →</span>
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
      context.font = 'bold 8px monospace'; context.textAlign = 'center'; context.fillStyle = '#eeede5'
      context.fillText('L', 17, 9); context.fillText('R', 41, 9)
      for (let channel = 0; channel < 2; channel += 1) {
        const db = 20 * Math.log10(Math.max(.004, levels.current[channel]))
        const lit = Math.round(Math.max(0, Math.min(1, (db + 48) / 52)) * 20)
        for (let segment = 0; segment < 20; segment += 1) {
          context.fillStyle = segment < lit ? '#eeede5' : '#383936'
          context.fillRect(8 + channel * 24, 130 - segment * 5, 17, 3)
        }
      }
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [])
  return <canvas ref={ref} className="pixel-canvas stereo-meter-canvas" aria-label="Stereo output meter" />
}
