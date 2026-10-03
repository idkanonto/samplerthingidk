import { type CSSProperties, useCallback, useEffect, useRef, useState } from 'react'
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
  particleCount = 600, speed = 1, audioLevel, pressure, shape, active }: {
  className?: string, color?: string, trailOpacity?: number, particleCount?: number, speed?: number,
  audioLevel: number, pressure: number, shape: number, active: boolean
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const particlesRef = useRef<NeuralParticle[]>([])
  const dimensionsRef = useRef({ width: 0, height: 0, particleCount: 0 })
  const dataRef = useRef({ audioLevel, pressure, shape })
  dataRef.current = { audioLevel, pressure, shape }

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container || !active) return
    const context = canvas.getContext('2d')
    if (!context) return

    let width = Math.max(1, container.clientWidth)
    let height = Math.max(1, container.clientHeight)
    const mouse = { x: -1000, y: -1000 }
    let frame = 0
    let previousFrame = 0
    let energy = 0

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
    const handleMouseMove = (event: MouseEvent) => {
      const rect = canvas.getBoundingClientRect()
      mouse.x = event.clientX - rect.left
      mouse.y = event.clientY - rect.top
    }
    const handleMouseLeave = () => { mouse.x = -1000; mouse.y = -1000 }
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
      const visualEnergy = energy * pressureCurve
      const shapeBias = clamped((current.shape + 100) / 200)
      const flowSpeed = speed * (1 + 0.30 * visualEnergy)
      const friction = 0.966 - 0.018 * shapeBias

      context.fillStyle = `rgba(0, 0, 0, ${trailOpacity})`
      context.fillRect(0, 0, width, height)
      for (const particle of particlesRef.current) {
        const angle = (Math.cos(particle.x * 0.005) + Math.sin(particle.y * 0.005)) * Math.PI
        particle.vx += Math.cos(angle) * 0.2 * flowSpeed
        particle.vy += Math.sin(angle) * 0.2 * flowSpeed

        const dx = mouse.x - particle.x
        const dy = mouse.y - particle.y
        const distance = Math.hypot(dx, dy)
        const interactionRadius = 150
        if (distance < interactionRadius) {
          const force = (interactionRadius - distance) / interactionRadius
          particle.vx -= dx * force * 0.05
          particle.vy -= dy * force * 0.05
        }

        particle.x += particle.vx
        particle.y += particle.vy
        particle.vx *= friction
        particle.vy *= friction
        if (++particle.age > particle.life) resetParticle(particle)
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
    container.addEventListener('mousemove', handleMouseMove)
    container.addEventListener('mouseleave', handleMouseLeave)
    return () => {
      window.removeEventListener('resize', handleResize)
      container.removeEventListener('mousemove', handleMouseMove)
      container.removeEventListener('mouseleave', handleMouseLeave)
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

/** User-supplied Waves field, retaining its line geometry and pointer response. */
export function FaultWaves({ audioLevel, active, className = '', strokeColor = '#ffffff',
  backgroundColor = '#000000', pointerSize = 0.5 }: {
  audioLevel: number, active: boolean, className?: string, strokeColor?: string,
  backgroundColor?: string, pointerSize?: number
}) {
  const containerRef = useRef<HTMLDivElement>(null)
  const svgRef = useRef<SVGSVGElement>(null)
  const activeRef = useRef(active)
  const audioRef = useRef(audioLevel)
  activeRef.current = active
  audioRef.current = audioLevel
  const mouseRef = useRef({ x: -10, y: 0, lx: 0, ly: 0, sx: 0, sy: 0, v: 0, vs: 0, a: 0, set: false })
  const pathsRef = useRef<SVGPathElement[]>([])
  const linesRef = useRef<WavePoint[][]>([])
  const noiseRef = useRef<((x: number, y: number) => number) | null>(null)
  const rafRef = useRef<number | null>(null)
  const tickRef = useRef<((time: number) => void) | null>(null)
  const boundingRef = useRef<DOMRect | null>(null)

  useEffect(() => {
    const container = containerRef.current
    const svg = svgRef.current
    if (!container || !svg) return
    noiseRef.current = createNoise2D()

    const setSize = () => {
      if (!containerRef.current || !svgRef.current) return
      boundingRef.current = containerRef.current.getBoundingClientRect()
      const { width, height } = boundingRef.current
      svgRef.current.style.width = `${width}px`
      svgRef.current.style.height = `${height}px`
    }
    const setLines = () => {
      if (!svgRef.current || !boundingRef.current) return
      const { width, height } = boundingRef.current
      linesRef.current = []
      pathsRef.current.forEach((path) => path.remove())
      pathsRef.current = []
      const xGap = 8
      const yGap = 8
      const oWidth = width + 200
      const oHeight = height + 30
      const totalLines = Math.ceil(oWidth / xGap)
      const totalPoints = Math.ceil(oHeight / yGap)
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
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path')
        path.classList.add('a__line', 'js-line')
        path.setAttribute('fill', 'none')
        path.setAttribute('stroke', strokeColor)
        path.setAttribute('stroke-width', '1')
        svgRef.current.appendChild(path)
        pathsRef.current.push(path)
        linesRef.current.push(points)
      }
    }

    const updateMousePosition = (x: number, y: number) => {
      if (!boundingRef.current) return
      const mouse = mouseRef.current
      mouse.x = x - boundingRef.current.left
      mouse.y = y - boundingRef.current.top + window.scrollY
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
    const onMouseMove = (event: MouseEvent) => updateMousePosition(event.pageX, event.pageY)
    const onTouchMove = (event: TouchEvent) => {
      event.preventDefault()
      const touch = event.touches[0]
      if (touch) updateMousePosition(touch.clientX, touch.clientY)
    }
    const onResize = () => { setSize(); setLines() }
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
      linesRef.current.forEach((points, lineIndex) => {
        const path = pathsRef.current[lineIndex]
        if (points.length < 2 || !path) return
        const first = moved(points[0], false)
        let drawing = `M ${first.x} ${first.y}`
        for (let index = 1; index < points.length; index++) {
          const point = moved(points[index])
          drawing += `L ${point.x} ${point.y}`
        }
        path.setAttribute('d', drawing)
      })
    }

    setSize()
    setLines()
    let smoothedEnergy = 0
    let previousFrame = 0
    let elapsed = 0
    const tick = (time: number) => {
      rafRef.current = null
      if (!activeRef.current) return
      const delta = previousFrame === 0 ? 1 / 60
        : Math.max(1 / 120, Math.min(1 / 20, (time - previousFrame) / 1000))
      previousFrame = time
      elapsed += delta * 1000
      smoothedEnergy = smoothAudioLevel(smoothedEnergy, clamped(audioRef.current), delta)
      const { current: mouse } = mouseRef
      mouse.sx += (mouse.x - mouse.sx) * 0.1
      mouse.sy += (mouse.y - mouse.sy) * 0.1
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
      movePoints(elapsed, smoothedEnergy)
      drawLines()
      rafRef.current = requestAnimationFrame(tick)
    }
    tickRef.current = tick
    const onContainerResize = () => onResize()
    window.addEventListener('resize', onContainerResize)
    window.addEventListener('mousemove', onMouseMove)
    container.addEventListener('touchmove', onTouchMove, { passive: false })
    return () => {
      if (rafRef.current !== null) cancelAnimationFrame(rafRef.current)
      rafRef.current = null
      tickRef.current = null
      window.removeEventListener('resize', onContainerResize)
      window.removeEventListener('mousemove', onMouseMove)
      container.removeEventListener('touchmove', onTouchMove)
      pathsRef.current.forEach((path) => path.remove())
      pathsRef.current = []
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
    style={{ backgroundColor, position: 'absolute', top: 0, left: 0, margin: 0, padding: 0,
      width: '100%', height: '100%', overflow: 'hidden', '--x': '-0.5rem', '--y': '50%' } as CSSProperties}>
    <svg ref={svgRef} className="block h-full w-full js-svg" xmlns="http://www.w3.org/2000/svg" />
    <div className="pointer-dot" style={{ position: 'absolute', top: 0, left: 0,
      width: `${pointerSize}rem`, height: `${pointerSize}rem`, background: strokeColor,
      borderRadius: '50%', transform: 'translate3d(calc(var(--x) - 50%), calc(var(--y) - 50%), 0)',
      willChange: 'transform' }} />
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
