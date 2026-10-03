import { useCallback, useEffect, useRef, useState } from 'react'
import { createNoise2D } from 'simplex-noise'
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

type NeuralParticle = { x: number, y: number, vx: number, vy: number, age: number, life: number }

function clamped(value: number, min = 0, max = 1) {
  return Math.max(min, Math.min(max, Number.isFinite(value) ? value : 0))
}

function smoothAudioLevel(current: number, target: number, deltaSeconds: number) {
  const timeConstant = target > current ? 0.045 : 0.24
  const blend = 1 - Math.exp(-deltaSeconds / timeConstant)
  return current + (target - current) * blend
}

/** User-supplied NeuralBackground flow, with bounded energy-only audio response. */
export function NeuralBackground({ className = '', color = '#ffffff', trailOpacity = 0.15,
  particleCount = 600, speed = 1, audioLevel, pressure, mix, shape, active }: {
  className?: string, color?: string, trailOpacity?: number, particleCount?: number, speed?: number,
  audioLevel: number, pressure: number, mix: number, shape: number, active: boolean
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const particlesRef = useRef<NeuralParticle[]>([])
  const dimensionsRef = useRef({ width: 0, height: 0, particleCount: 0 })
  const dataRef = useRef({ audioLevel, pressure, mix, shape })
  dataRef.current = { audioLevel, pressure, mix, shape }

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container || !active) return
    const context = canvas.getContext('2d')
    if (!context) return

    let width = Math.max(1, container.clientWidth)
    let height = Math.max(1, container.clientHeight)
    const mouse = { x: width * .5, y: height * .5 }
    let frame = 0
    let previousFrame = 0
    let energy = 0
    let virtualPhase = 0

    const initialize = () => {
      width = Math.max(1, container.clientWidth)
      height = Math.max(1, container.clientHeight)
      const sizeChanged = width !== dimensionsRef.current.width
        || height !== dimensionsRef.current.height
        || particleCount !== dimensionsRef.current.particleCount
      if (!sizeChanged && particlesRef.current.length > 0) return
      dimensionsRef.current = { width, height, particleCount }
      const dpr = window.devicePixelRatio || 1
      canvas.width = Math.round(width * dpr)
      canvas.height = Math.round(height * dpr)
      canvas.style.width = `${width}px`
      canvas.style.height = `${height}px`
      context.setTransform(dpr, 0, 0, dpr, 0, 0)
      particlesRef.current = Array.from({ length: particleCount }, () => ({
        x: Math.random() * width,
        y: Math.random() * height,
        vx: 0,
        vy: 0,
        age: 0,
        life: Math.random() * 200 + 100
      }))
    }

    const handleResize = () => initialize()
    const resetParticle = (particle: NeuralParticle) => {
      particle.x = Math.random() * width
      particle.y = Math.random() * height
      particle.vx = 0
      particle.vy = 0
      particle.age = 0
      particle.life = Math.random() * 200 + 100
    }

    const animate = (time: number) => {
      frame = requestAnimationFrame(animate)
      const deltaSeconds = previousFrame === 0 ? 1 / 60
        : Math.max(1 / 120, Math.min(1 / 20, (time - previousFrame) / 1000))
      previousFrame = time
      const current = dataRef.current
      const targetLevel = clamped(current.audioLevel)
      energy = smoothAudioLevel(energy, targetLevel, deltaSeconds)
      const pressureCurve = Math.pow(clamped(current.pressure), 1.8)
      const mixCurve = Math.pow(clamped(current.mix), .75)
      const visualEnergy = energy * pressureCurve * mixCurve
      const shapeBias = clamped((current.shape + 100) / 200)
      const flowSpeed = speed * visualEnergy * 2.2
      const friction = 0.966 - 0.018 * shapeBias
      const currentTrailOpacity = Math.max(.035, Math.min(.34,
        trailOpacity + (shapeBias - .5) * .22))
      virtualPhase = (virtualPhase + deltaSeconds * visualEnergy * 3.2) % (Math.PI * 2)
      const horizontal = Math.cos(virtualPhase)
      const vertical = Math.sin(virtualPhase)
      const superellipseX = Math.sign(horizontal) * Math.sqrt(Math.abs(horizontal))
      const superellipseY = Math.sign(vertical) * Math.sqrt(Math.abs(vertical))
      const targetX = width * (.5 + .43 * superellipseX)
      const targetY = height * (.5 + .39 * superellipseY)
      const cursorBlend = 1 - Math.exp(-deltaSeconds * (4 + 14 * visualEnergy))
      mouse.x += (targetX - mouse.x) * cursorBlend
      mouse.y += (targetY - mouse.y) * cursorBlend

      context.fillStyle = `rgba(0, 0, 0, ${currentTrailOpacity})`
      context.fillRect(0, 0, width, height)
      const visibleParticles = Math.round(particlesRef.current.length * (.18 + .82 * pressureCurve))
      for (let index = 0; index < visibleParticles; index += 1) {
        const particle = particlesRef.current[index]
        const angle = (Math.cos(particle.x * 0.005) + Math.sin(particle.y * 0.005)) * Math.PI
        particle.vx += Math.cos(angle) * 0.2 * flowSpeed
        particle.vy += Math.sin(angle) * 0.2 * flowSpeed

        // A deliberately tiny restoring force counters long-term edge and
        // corner accumulation without introducing a boundary or safe margin.
        // Particles still wrap through, and can occupy, the complete display.
        const gravity = 0.00009 * visualEnergy
        particle.vx += (width * .5 - particle.x) * gravity
        particle.vy += (height * .5 - particle.y) * gravity

        const dx = mouse.x - particle.x
        const dy = mouse.y - particle.y
        const distance = Math.hypot(dx, dy)
        const interactionRadius = 150
        if (visualEnergy > .001 && distance < interactionRadius) {
          const force = (interactionRadius - distance) / interactionRadius
          particle.vx -= dx * force * 0.05 * visualEnergy
          particle.vy -= dy * force * 0.05 * visualEnergy
        }

        particle.x += particle.vx * Math.min(1, visualEnergy * 7)
        particle.y += particle.vy * Math.min(1, visualEnergy * 7)
        particle.vx *= friction
        particle.vy *= friction
        particle.age += Math.min(1, visualEnergy * 5)
        if (particle.age > particle.life) resetParticle(particle)
        if (particle.x < 0) particle.x = width
        if (particle.x > width) particle.x = 0
        if (particle.y < 0) particle.y = height
        if (particle.y > height) particle.y = 0

        const opacity = 1 - Math.abs(particle.age / particle.life - 0.5) * 2
        context.globalAlpha = opacity
        context.fillStyle = color
        context.fillRect(particle.x, particle.y, 1.5, 1.5)
      }
      context.globalAlpha = 1
    }

    initialize()
    frame = requestAnimationFrame(animate)
    window.addEventListener('resize', handleResize)
    return () => {
      window.removeEventListener('resize', handleResize)
      cancelAnimationFrame(frame)
    }
  }, [active, color, particleCount, speed, trailOpacity])

  return <div ref={containerRef} className={`relative h-full w-full overflow-hidden ${className}`}>
    <canvas ref={canvasRef} className="block h-full w-full" aria-label="Bleed crystalline particle flow" />
  </div>
}

type WavePoint = {
  x: number, y: number, wave: { x: number, y: number },
  cursor: { x: number, y: number, vx: number, vy: number }
}

type FaultCanvasSize = { width: number, height: number, pixelRatio: number }

/** User-supplied Waves field, retaining its line geometry and pointer response. */
export function FaultWaves({ eventSerial, active, className = '', strokeColor = '#ffffff',
  backgroundColor = '#000000' }: {
  eventSerial: number, active: boolean, className?: string, strokeColor?: string,
  backgroundColor?: string
}) {
  const containerRef = useRef<HTMLDivElement>(null)
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const activeRef = useRef(active)
  const eventSerialRef = useRef(eventSerial)
  activeRef.current = active
  eventSerialRef.current = eventSerial
  const mouseRef = useRef({ x: -10, y: 0, lx: 0, ly: 0, sx: 0, sy: 0, v: 0, vs: 0, a: 0, set: false })
  const linesRef = useRef<WavePoint[][]>([])
  const noiseRef = useRef<((x: number, y: number) => number) | null>(null)
  const rafRef = useRef<number | null>(null)
  const tickRef = useRef<((time: number) => void) | null>(null)
  const sizeRef = useRef<FaultCanvasSize | null>(null)

  useEffect(() => {
    const container = containerRef.current
    const canvas = canvasRef.current
    if (!container || !canvas) return
    const context = canvas.getContext('2d')
    if (!context) return
    noiseRef.current = createNoise2D()

    const setSize = () => {
      if (!containerRef.current || !canvasRef.current) return false
      // clientWidth/clientHeight stay in the monitor's logical coordinate
      // system. getBoundingClientRect() includes the complete editor's CSS
      // scale and would apply 75/125/150% a second time to this canvas.
      const width = Math.round(containerRef.current.clientWidth)
      const height = Math.round(containerRef.current.clientHeight)
      const pixelRatio = Math.max(1, window.devicePixelRatio || 1)
      // JUCE's WebView can mount before the editor receives its final bounds.
      // Ignore that transient box and let ResizeObserver perform the real pass.
      if (width < 2 || height < 2) return false
      if (sizeRef.current?.width === width
        && sizeRef.current?.height === height
        && sizeRef.current?.pixelRatio === pixelRatio
        && linesRef.current.length > 0) return false
      sizeRef.current = { width, height, pixelRatio }
      canvasRef.current.width = Math.max(1, Math.round(width * pixelRatio))
      canvasRef.current.height = Math.max(1, Math.round(height * pixelRatio))
      // CSS owns the visual size. Only the backing bitmap follows DPI.
      canvasRef.current.style.width = '100%'
      canvasRef.current.style.height = '100%'
      context.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0)
      context.imageSmoothingEnabled = false
      return true
    }
    const setLines = () => {
      if (!sizeRef.current) return
      const { width, height } = sizeRef.current
      linesRef.current = []
      const xGap = 9
      const yGap = 9
      // A fixed logical overscan keeps cursor deformation beyond the bezel
      // without changing composition when the whole editor is scaled.
      const overscan = 64
      const oWidth = width + overscan * 2
      const oHeight = height + overscan * 2
      const totalLines = Math.ceil(oWidth / xGap)
      const totalPoints = Math.ceil(oHeight / yGap)
      // Overscan the original wave field beyond every bezel edge. Cursor-force
      // displacement can no longer reveal the SVG boundary inside the monitor.
      const xStart = (width - xGap * totalLines) / 2
      const yStart = (height - yGap * totalPoints) / 2

      for (let line = 0; line < totalLines; line++) {
        const points: WavePoint[] = []
        for (let pointIndex = 0; pointIndex < totalPoints; pointIndex++) {
          points.push({
            x: xStart + xGap * line,
            y: yStart + yGap * pointIndex,
            wave: { x: 0, y: 0 },
            cursor: { x: 0, y: 0, vx: 0, vy: 0 }
          })
        }
        linesRef.current.push(points)
      }
    }

    const updateVirtualPointer = (x: number, y: number) => {
      const mouse = mouseRef.current
      mouse.x = x
      mouse.y = y
      if (!mouse.set) {
        mouse.sx = mouse.x
        mouse.sy = mouse.y
        mouse.lx = mouse.x
        mouse.ly = mouse.y
        mouse.set = true
      }
      containerRef.current?.style.setProperty('--x', `${mouse.sx}px`)
      containerRef.current?.style.setProperty('--y', `${mouse.sy}px`)
    }
    const onResize = () => { if (setSize()) setLines() }
    const moved = (point: WavePoint, withCursorForce = true) => ({
      x: point.x + point.wave.x + (withCursorForce ? point.cursor.x : 0),
      y: point.y + point.wave.y + (withCursorForce ? point.cursor.y : 0)
    })
    const movePoints = (time: number, energy: number) => {
      const noise = noiseRef.current
      if (!noise) return
      const movement = 1 + 0.42 * energy
      const { current: mouse } = mouseRef
      linesRef.current.forEach((points) => points.forEach((point) => {
        const move = noise((point.x + time * 0.008) * 0.003,
          (point.y + time * 0.003) * 0.002) * 8
        point.wave.x = Math.cos(move) * 12 * movement
        point.wave.y = Math.sin(move) * 6 * movement

        const dx = point.x - mouse.sx
        const dy = point.y - mouse.sy
        const distance = Math.hypot(dx, dy)
        const radius = Math.max(175, mouse.vs)
        if (distance < radius) {
          const scale = 1 - distance / radius
          const force = Math.cos(distance * 0.001) * scale
          point.cursor.vx += Math.cos(mouse.a) * force * radius * mouse.vs * 0.00035
          point.cursor.vy += Math.sin(mouse.a) * force * radius * mouse.vs * 0.00035
        }
        point.cursor.vx += -point.cursor.x * 0.01
        point.cursor.vy += -point.cursor.y * 0.01
        point.cursor.vx *= 0.95
        point.cursor.vy *= 0.95
        point.cursor.x = Math.max(-50, Math.min(50, point.cursor.x + point.cursor.vx))
        point.cursor.y = Math.max(-50, Math.min(50, point.cursor.y + point.cursor.vy))
      }))
    }
    const drawLines = () => {
      const size = sizeRef.current
      if (!size) return
      context.fillStyle = backgroundColor
      context.fillRect(0, 0, size.width, size.height)
      context.strokeStyle = strokeColor
      context.lineWidth = 1
      context.lineCap = 'butt'
      context.lineJoin = 'miter'
      linesRef.current.forEach((points) => {
        if (points.length < 2) return
        const first = moved(points[0], false)
        context.beginPath()
        context.moveTo(first.x, first.y)
        for (let index = 1; index < points.length; index++) {
          const point = moved(points[index])
          context.lineTo(point.x, point.y)
        }
        context.stroke()
      })
    }

    onResize()
    let previousFrame = 0
    let elapsed = 0
    let displayedPhase = 0
    let targetPhase = 0
    let phaseAtEvent = 0
    let eventMotionElapsed = 1
    let lastEventSerial = eventSerialRef.current
    const eventRoute = [
      [.10, .14], [.90, .14], [.10, .86], [.90, .86],
      [.10, .14], [.10, .86], [.90, .14], [.90, .86]
    ]
    const eventMotionDuration = .65
    const tick = (time: number) => {
      rafRef.current = null
      if (!activeRef.current) return
      const delta = previousFrame === 0 ? 1 / 60
        : Math.max(1 / 120, Math.min(1 / 20, (time - previousFrame) / 1000))
      previousFrame = time
      const currentEventSerial = eventSerialRef.current
      const eventDelta = Math.max(0, Math.min(8, currentEventSerial - lastEventSerial))
      if (eventDelta > 0) {
        phaseAtEvent = displayedPhase
        targetPhase += eventDelta / eventRoute.length
        eventMotionElapsed = 0
        elapsed += 92 * eventDelta
      }
      lastEventSerial = currentEventSerial
      eventMotionElapsed = Math.min(eventMotionDuration, eventMotionElapsed + delta)
      const eventProgress = eventMotionElapsed / eventMotionDuration
      const easedEventProgress = eventProgress * eventProgress * (3 - 2 * eventProgress)
      displayedPhase = phaseAtEvent + (targetPhase - phaseAtEvent) * easedEventProgress
      const width = sizeRef.current?.width ?? 1
      const height = sizeRef.current?.height ?? 1
      const routePosition = ((displayedPhase % 1) + 1) % 1 * eventRoute.length
      const segment = Math.floor(routePosition) % eventRoute.length
      const progress = routePosition - Math.floor(routePosition)
      const from = eventRoute[segment]
      const to = eventRoute[(segment + 1) % eventRoute.length]
      updateVirtualPointer(width * (from[0] + (to[0] - from[0]) * progress),
        height * (from[1] + (to[1] - from[1]) * progress))
      const { current: mouse } = mouseRef
      mouse.sx = mouse.x
      mouse.sy = mouse.y
      const dx = mouse.x - mouse.lx
      const dy = mouse.y - mouse.ly
      const distance = Math.hypot(dx, dy)
      mouse.v = distance
      mouse.vs = Math.min(100, mouse.vs + (distance - mouse.vs) * 0.1)
      mouse.lx = mouse.x
      mouse.ly = mouse.y
      mouse.a = Math.atan2(dy, dx)
      containerRef.current?.style.setProperty('--x', `${mouse.sx}px`)
      containerRef.current?.style.setProperty('--y', `${mouse.sy}px`)
      movePoints(elapsed, Math.min(1, eventDelta * .5))
      drawLines()
      rafRef.current = requestAnimationFrame(tick)
    }
    tickRef.current = tick
    const onContainerResize = () => onResize()
    const resizeObserver = new ResizeObserver(onContainerResize)
    resizeObserver.observe(container)
    window.addEventListener('resize', onContainerResize)
    // A second layout pass is required by some native WebView2 hosts even
    // when ResizeObserver is available.
    requestAnimationFrame(onContainerResize)
    return () => {
      if (rafRef.current !== null) cancelAnimationFrame(rafRef.current)
      rafRef.current = null
      tickRef.current = null
      resizeObserver.disconnect()
      window.removeEventListener('resize', onContainerResize)
      linesRef.current = []
    }
  }, [strokeColor])

  useEffect(() => {
    if (active && rafRef.current === null && tickRef.current)
      rafRef.current = requestAnimationFrame(tickRef.current)
    else if (!active && rafRef.current !== null) {
      cancelAnimationFrame(rafRef.current)
      rafRef.current = null
    }
  }, [active])

  return <div ref={containerRef} className={`waves-component relative overflow-hidden ${className}`}
    style={{ backgroundColor, position: 'relative', display: 'block', margin: 0, padding: 0,
      width: '100%', height: '100%', minWidth: 0, minHeight: 0, overflow: 'hidden' }}>
    <canvas ref={canvasRef} className="pixel-canvas" aria-label="Fault event-driven wave field" />
  </div>
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

export function SpectralDrawCanvas({ values, width, height, resetSignal, scanPosition, active }: {
  values: number[], width: number, height: number, resetSignal: number, scanPosition: number, active: boolean
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
    if (active) {
      const scanX = Math.max(0, Math.min(1, scanPosition)) * width
      const trail = context.createLinearGradient(scanX - 3, 0, scanX + 3, 0)
      trail.addColorStop(0, 'rgba(238,238,238,0)')
      trail.addColorStop(.5, 'rgba(238,238,238,.12)')
      trail.addColorStop(1, 'rgba(238,238,238,0)')
      context.fillStyle = trail
      context.fillRect(scanX - 3, 0, 6, height)
      context.fillStyle = '#eeeeee'
      context.fillRect(Math.round(scanX), 0, Math.max(1, 1 / (window.devicePixelRatio || 1)), height)
    }
  }, [active, revision, scanPosition, width, height])

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
      for (let channel = 0; channel < 2; channel += 1) {
        const db = 20 * Math.log10(Math.max(.004, levels.current[channel]))
        const lit = Math.round(Math.max(0, Math.min(1, (db + 48) / 52)) * 27)
        for (let segment = 0; segment < 27; segment += 1) {
          context.fillStyle = segment < lit ? '#eeeeee' : '#383838'
          context.fillRect(8 + channel * 24, 134 - segment * 5, 17, 3)
        }
      }
    }
    frame = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(frame)
  }, [])
  return <canvas ref={ref} className="pixel-canvas stereo-meter-canvas" aria-label="Stereo output meter" />
}
