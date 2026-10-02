import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import './styles.css'

const updateEditorScale = () => {
  const scale = Math.min(window.innerWidth / 1080, window.innerHeight / 675)
  document.documentElement.style.setProperty('--editor-scale', String(scale))
}

updateEditorScale()
window.addEventListener('resize', updateEditorScale)

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode><App /></React.StrictMode>
)
