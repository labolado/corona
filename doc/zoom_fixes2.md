# WebGL Canvas Scaling and Viewport Optimization

## Current Implementation Analysis

### 1. Viewport and Scale Handling
```javascript
// Current viewport handling
if (gl) {
    var displayWidth = Math.floor(canvas.clientWidth * devicePixelRatio);
    var displayHeight = Math.floor(canvas.clientHeight * devicePixelRatio);
    
    if (canvas.width !== displayWidth || canvas.height !== displayHeight) {
        canvas.width = displayWidth;
        canvas.height = displayHeight;
        gl.viewport(0, 0, displayWidth, displayHeight);
    }
}

// Current scale calculation
var devicePixelRatio = window.devicePixelRatio || 1;
var scale = viewPort.width / (Module.appInitWidth * devicePixelRatio);
if (Module.appContentWidth > 0) {
    scale *= Module.appInitWidth / Module.appContentWidth;
}
```

Problems:
- Viewport updates not synchronized with scale changes
- Complex scale calculation with potential precision issues
- devicePixelRatio applied inconsistently

### 2. Canvas Dimension Management
```javascript
// Current dimension handling
canvas.style.width = viewPort.width + "px";
canvas.style.height = viewPort.height + "px";
```

Issues:
- CSS dimensions set without considering devicePixelRatio
- No clear separation between logical and physical pixels
- Potential misalignment between canvas and WebGL viewport

## Optimized Solution

### 1. Unified Viewport Management
```javascript
function updateViewport(canvas, viewPort, gl) {
    const devicePixelRatio = window.devicePixelRatio || 1;
    
    // Set logical (CSS) dimensions
    canvas.style.width = viewPort.width + "px";
    canvas.style.height = viewPort.height + "px";
    
    // Calculate physical dimensions
    const displayWidth = Math.floor(viewPort.width * devicePixelRatio);
    const displayHeight = Math.floor(viewPort.height * devicePixelRatio);
    
    // Update if dimensions changed
    if (canvas.width !== displayWidth || canvas.height !== displayHeight) {
        canvas.width = displayWidth;
        canvas.height = displayHeight;
        
        if (gl) {
            gl.viewport(0, 0, displayWidth, displayHeight);
        }
        return true; // dimensions changed
    }
    return false; // no change
}
```

### 2. Simplified Scale Calculation
```javascript
function calculateScale(viewPort, Module) {
    const devicePixelRatio = window.devicePixelRatio || 1;
    const effectiveWidth = Module.appContentWidth > 0 ? 
        Module.appContentWidth : Module.appInitWidth;
    
    // Single, clear scale calculation
    return viewPort.width / (effectiveWidth * devicePixelRatio);
}
```

### 3. Combined Update Function
```javascript
function refreshNativeObject() {
    const gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl");
    
    // Update viewport and get change status
    const dimensionsChanged = updateViewport(canvas, viewPort, gl);
    
    // Calculate new scale
    const scale = calculateScale(viewPort, Module);
    
    return {
        scale,
        dimensionsChanged,
        devicePixelRatio: window.devicePixelRatio || 1
    };
}
```

## Implementation Guide

### 1. Core Updates
- Replace current viewport management code
- Update scale calculation
- Implement unified refresh function

### 2. Key Considerations
- Always use devicePixelRatio for physical pixel calculations
- Maintain synchronization between canvas and WebGL viewport
- Handle dimension changes efficiently

## Testing Strategy

### 1. Viewport Tests
- [ ] Verify viewport dimensions match physical pixels
- [ ] Check viewport alignment with canvas
- [ ] Test viewport updates during resize

### 2. Scale Tests
- [ ] Validate scale calculations
- [ ] Check rendering at different zoom levels
- [ ] Test with various devicePixelRatio values

### 3. Performance Tests
- [ ] Measure update frequency
- [ ] Monitor memory usage
- [ ] Check rendering quality

## Debug Support
```javascript
function logViewportUpdate(canvas, viewPort, scale) {
    const devicePixelRatio = window.devicePixelRatio || 1;
    console.log({
        logical: {
            width: viewPort.width,
            height: viewPort.height
        },
        physical: {
            width: canvas.width,
            height: canvas.height
        },
        devicePixelRatio,
        scale,
        timestamp: Date.now()
    });
}
```

## Common Issues

### 1. Rendering Quality
- Ensure physical pixels align with device pixels
- Maintain correct aspect ratios
- Handle devicePixelRatio changes properly

### 2. Performance
- Only update viewport when necessary
- Batch dimension changes
- Cache devicePixelRatio value when appropriate

### 3. Edge Cases
- Handle zero dimensions
- Manage extreme zoom levels
- Deal with devicePixelRatio changes

## Future Improvements

1. **Optimization**
   - Implement dimension change detection
   - Add viewport state caching
   - Optimize frequent updates

2. **Robustness**
   - Add error handling
   - Implement fallback modes
   - Add validation checks
