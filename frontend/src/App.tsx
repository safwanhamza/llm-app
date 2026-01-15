
import React, { useState } from 'react';
import './App.css';

interface HeatParams {
  width: number;
  height: number;
  diffusion_rate: number;
  time_steps: number;
  delta_t: number;
  delta_x: number;
}

interface NBodyParams {
  num_bodies: number;
  time_steps: number;
  delta_t: number;
  g_constant: number;
}

function App() {
  const [mode, setMode] = useState<'heat' | 'nbody'>('heat');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [visualizationUrl, setVisualizationUrl] = useState<string | null>(null);

  const [heatParams, setHeatParams] = useState<HeatParams>({
    width: 100,
    height: 100,
    diffusion_rate: 1.0,
    time_steps: 100,
    delta_t: 0.1,
    delta_x: 1.0
  });

  const [nbodyParams, setNbodyParams] = useState<NBodyParams>({
    num_bodies: 50,
    time_steps: 200,
    delta_t: 0.01,
    g_constant: 1.0
  });

  const apiBase = 'http://localhost:8080/api';

  const runSimulation = async () => {
    setLoading(true);
    setResult(null);
    try {
      const endpoint = mode === 'heat' ? '/simulate/heat' : '/simulate/nbody';
      const body = mode === 'heat' ? heatParams : nbodyParams;

      const response = await fetch(`${apiBase}${endpoint}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });

      const data = await response.json();
      setResult(data.result);
    } catch (e) {
      console.error(e);
      alert('Error running simulation');
    } finally {
      setLoading(false);
    }
  };

  const optimize = async (goal: string) => {
    setLoading(true);
    try {
      const endpoint = mode === 'heat' ? '/optimize/heat' : '/optimize/nbody';
      const body = mode === 'heat'
        ? { target_property: goal, desired_value: 1.0 }
        : { target_behavior: goal, body_count: nbodyParams.num_bodies };

      const response = await fetch(`${apiBase}${endpoint}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });

      const optimizedParams = await response.json();
      if (mode === 'heat') setHeatParams(optimizedParams);
      else setNbodyParams(optimizedParams);
    } catch (e) {
      console.error(e);
      alert('Error optimizing');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="App" style={{ padding: '20px' }}>
      <h1>Scientific Simulation Dashboard</h1>

      <div style={{ marginBottom: '20px' }}>
        <button onClick={() => setMode('heat')} disabled={loading}>Heat Equation</button>
        <button onClick={() => setMode('nbody')} disabled={loading}>N-Body Gravity</button>
      </div>

      <div style={{ display: 'flex', gap: '20px' }}>
        <div style={{ width: '300px' }}>
          <h2>Parameters</h2>
          {mode === 'heat' ? (
            <>
              <label>Diffusion Rate: <input type="number" value={heatParams.diffusion_rate} onChange={e => setHeatParams({...heatParams, diffusion_rate: parseFloat(e.target.value)})} /></label><br/>
              <label>Time Steps: <input type="number" value={heatParams.time_steps} onChange={e => setHeatParams({...heatParams, time_steps: parseInt(e.target.value)})} /></label><br/>
              <button onClick={() => optimize('fast_diffusion')}>Optimize for Fast Diffusion (AI)</button>
              <button onClick={() => optimize('stable')}>Optimize for Stability (AI)</button>
            </>
          ) : (
            <>
              <label>Bodies: <input type="number" value={nbodyParams.num_bodies} onChange={e => setNbodyParams({...nbodyParams, num_bodies: parseInt(e.target.value)})} /></label><br/>
              <label>G Constant: <input type="number" value={nbodyParams.g_constant} onChange={e => setNbodyParams({...nbodyParams, g_constant: parseFloat(e.target.value)})} /></label><br/>
              <button onClick={() => optimize('minimize_collisions')}>Optimize: Minimize Collisions (AI)</button>
              <button onClick={() => optimize('high_activity')}>Optimize: High Activity (AI)</button>
            </>
          )}
          <br/><br/>
          <button onClick={runSimulation} disabled={loading} style={{ fontSize: '1.2em', padding: '10px' }}>
            {loading ? 'Simulating...' : 'Run Simulation'}
          </button>
        </div>

        <div style={{ flex: 1, border: '1px solid #ccc', padding: '10px', minHeight: '400px' }}>
          <h2>Visualization</h2>
          {result && mode === 'heat' && <HeatMapVisualizer width={result.width} height={result.height} data={result.data} />}
          {result && mode === 'nbody' && <NBodyVisualizer result={result} />}
          {!result && <p>No simulation result yet.</p>}
        </div>
      </div>
    </div>
  );
}

const HeatMapVisualizer = ({ width, height, data }: { width: number, height: number, data: number[] }) => {
  const canvasRef = React.useRef<HTMLCanvasElement>(null);

  React.useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const imgData = ctx.createImageData(width, height);
    // Normalize data for visualization
    const maxVal = Math.max(...data, 1.0);

    for (let i = 0; i < data.length; i++) {
        const val = data[i];
        const intensity = Math.min(255, Math.floor((val / maxVal) * 255));
        imgData.data[i * 4 + 0] = intensity; // R
        imgData.data[i * 4 + 1] = 0; // G
        imgData.data[i * 4 + 2] = 255 - intensity; // B
        imgData.data[i * 4 + 3] = 255; // Alpha
    }
    ctx.putImageData(imgData, 0, 0);
  }, [width, height, data]);

  return <canvas ref={canvasRef} width={width} height={height} style={{ width: '100%', maxWidth: '500px', imageRendering: 'pixelated' }} />;
};

const NBodyVisualizer = ({ result }: { result: any }) => {
  const canvasRef = React.useRef<HTMLCanvasElement>(null);
  const [step, setStep] = useState(0);

  // Animate
  React.useEffect(() => {
      let animationFrameId: number;

      const animate = () => {
          setStep(prev => {
              const next = prev + 1;
              if (next >= result.steps) return 0;
              return next;
          });
          animationFrameId = requestAnimationFrame(animate);
      };

      animationFrameId = requestAnimationFrame(animate);
      return () => cancelAnimationFrame(animationFrameId);
  }, [result]);

  React.useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = 'black';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const numBodies = result.num_bodies;
    const positions = result.all_positions; // [x0, y0, x1, y1, ... step1 ... step2 ...]

    const offset = step * numBodies * 2;

    ctx.fillStyle = 'white';
    const scale = 2.0;
    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;

    for (let i = 0; i < numBodies; i++) {
        const x = positions[offset + i * 2];
        const y = positions[offset + i * 2 + 1];

        ctx.beginPath();
        ctx.arc(centerX + x * scale, centerY + y * scale, 3, 0, Math.PI * 2);
        ctx.fill();
    }

  }, [step, result]);

  return <canvas ref={canvasRef} width={500} height={500} />;
}

export default App;
