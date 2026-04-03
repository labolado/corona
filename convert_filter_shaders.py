#!/usr/bin/env python3
"""
Convert Solar2D filter kernel shaders from GLSL to bgfx .sc format
"""

import os
import re
import subprocess
import sys

def remove_precision_macros(text):
    """Remove precision macros from text"""
    text = text.replace('P_COLOR ', '')
    text = text.replace('P_UV ', '')
    text = text.replace('P_POSITION ', '')
    text = text.replace('P_RANDOM ', '')
    text = text.replace('P_NORMAL ', '')
    return text

def process_highp_directives(text):
    """
    Process FRAGMENT_SHADER_SUPPORTS_HIGHP directives.
    Keep the #if part, remove the #else part, keep #if 0/1 blocks as-is.
    """
    # Pattern for FRAGMENT_SHADER_SUPPORTS_HIGHP: keep #if content, remove #else content
    pattern = r'#if\s+FRAGMENT_SHADER_SUPPORTS_HIGHP\s*(.*?)#else.*?#endif'
    text = re.sub(pattern, r'\1', text, flags=re.DOTALL)
    
    # Remove any remaining #if FRAGMENT_SHADER_SUPPORTS_HIGHP (without #else)
    text = re.sub(r'#if\s+FRAGMENT_SHADER_SUPPORTS_HIGHP\s*', '', text)
    text = re.sub(r'#endif\s*//\s*FRAGMENT_SHADER_SUPPORTS_HIGHP', '', text)
    
    return text

def extract_fragment_code(lua_content):
    """Extract fragment shader code from Lua file"""
    pattern = r'kernel\.fragment\s*=\s*\[\[(.*?)\]\]'
    match = re.search(pattern, lua_content, re.DOTALL)
    if not match:
        return None
    return match.group(1).strip()

def extract_function_body(glsl_code):
    """Extract FragmentKernel function body using brace matching"""
    func_pattern = r'vec4\s+FragmentKernel\s*\([^)]*\)\s*\{'
    match = re.search(func_pattern, glsl_code)
    if not match:
        return None, None
    
    start_idx = match.end() - 1
    
    brace_depth = 0
    end_idx = start_idx
    for i in range(start_idx, len(glsl_code)):
        if glsl_code[i] == '{':
            brace_depth += 1
        elif glsl_code[i] == '}':
            brace_depth -= 1
            if brace_depth == 0:
                end_idx = i
                break
    
    body = glsl_code[start_idx + 1:end_idx].strip()
    before_func = glsl_code[:match.start()].strip()
    
    return body, before_func

def extract_varyings(code):
    """Extract varying declarations from code"""
    varyings = []
    pattern = r'varying\s+(\w+)\s+(\w+)\s*;'
    for match in re.finditer(pattern, code):
        var_type = match.group(1)
        var_name = match.group(2)
        varyings.append((var_type, var_name))
    return varyings

def extract_uniforms(code):
    """Extract uniform declarations from code"""
    uniforms = {}
    pattern = r'uniform\s+(\w+)\s+(u_\w+)\s*;'
    for match in re.finditer(pattern, code):
        var_type = match.group(1)
        var_name = match.group(2)
        uniforms[var_name] = var_type
    return uniforms

def fix_atan(code):
    """Replace atan(y, x) with atan2(y, x) for HLSL compatibility"""
    # Pattern: atan( arg1, arg2 )
    code = re.sub(r'atan\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)', r'atan2(\1, \2)', code)
    return code

def process_body(body_code):
    """Process the function body code"""
    body_code = body_code.replace('texCoord', 'v_TexCoord.xy')
    body_code = fix_atan(body_code)
    
    lines = body_code.split('\n')
    result_lines = []
    
    for line in lines:
        stripped = line.strip()
        
        if stripped.startswith('return '):
            ret_val = stripped[7:].rstrip(';')
            if ret_val.startswith('(') and ret_val.endswith(')'):
                inner = ret_val[1:-1].strip()
                ret_val = inner
            
            line = f'    gl_FragColor = {ret_val};'
        
        result_lines.append(line)
    
    return '\n'.join(result_lines)

def process_kernel_file(filepath):
    """Process a single kernel file and return the converted shader"""
    with open(filepath, 'r') as f:
        content = f.read()
    
    if 'kernel.graph' in content and 'kernel.fragment' not in content:
        return None, "graph-only kernel"
    
    if 'kernel.vertex' in content and 'kernel.fragment' not in content:
        return None, "vertex-only kernel"
    
    fragment_code = extract_fragment_code(content)
    if not fragment_code:
        return None, "no fragment code found"
    
    name_match = re.search(r'kernel\.name\s*=\s*"([^"]+)"', content)
    name = name_match.group(1) if name_match else "unknown"
    
    # Remove precision macros first
    fragment_code = remove_precision_macros(fragment_code)
    
    # Process highp directives
    fragment_code = process_highp_directives(fragment_code)
    
    body, global_code = extract_function_body(fragment_code)
    if body is None:
        return None, "could not extract function body"
    
    # Extract varyings from global code
    varyings = extract_varyings(global_code)
    
    # Build $input line
    base_inputs = ['v_TexCoord', 'v_ColorScale', 'v_UserData']
    for var_type, var_name in varyings:
        if var_name not in base_inputs:
            base_inputs.append(var_name)
    input_line = ', '.join(base_inputs)
    
    # Remove varying declarations from global code
    for var_type, var_name in varyings:
        pattern = rf'varying\s+\w+\s+{var_name}\s*;\s*'
        global_code = re.sub(pattern, '', global_code)
    
    # Process body
    body = process_body(body)
    
    # Extract uniforms
    global_uniforms = extract_uniforms(global_code)
    body_uniforms = extract_uniforms(body)
    all_uniforms = {**global_uniforms, **body_uniforms}
    
    # Remove uniform declarations from global_code and body
    for var_name in global_uniforms:
        pattern = rf'uniform\s+\w+\s+{var_name}\s*;\s*'
        global_code = re.sub(pattern, '', global_code)
    for var_name in body_uniforms:
        pattern = rf'uniform\s+\w+\s+{var_name}\s*;\s*'
        body = re.sub(pattern, '', body)
    
    # Build user data uniforms declaration
    user_data_decls = []
    for i in range(4):
        var_name = f'u_UserData{i}'
        if var_name in all_uniforms:
            var_type = all_uniforms[var_name]
            if var_type == 'float':
                user_data_decls.append(f'uniform vec4 {var_name};  // use .x for scalar')
            else:
                user_data_decls.append(f'uniform {var_type} {var_name};')
        else:
            user_data_decls.append(f'uniform vec4 {var_name};')
    
    global_code = global_code.strip()
    if global_code:
        global_code += '\n\n'
    
    # Build the shader
    user_data_uniforms_str = '\n'.join(user_data_decls)
    
    shader = f'''$input {input_line}

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: {name}

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
{user_data_uniforms_str}

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

{global_code}void main()
{{
{body}
}}
'''
    
    return shader, None

def compile_shader(shader_path, shaderc_path):
    """Compile a shader using shaderc and return success status"""
    cmd = [
        shaderc_path,
        '-f', shader_path,
        '-o', '/dev/null',
        '--type', 'fragment',
        '--platform', 'osx',
        '-p', 'metal',
        '-i', 'external/bgfx/src',
        '-i', 'librtt/Display/Shader/bgfx'
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if result.returncode == 0:
            return True, ""
        else:
            error = result.stderr
            lines = error.split('\n')
            for i, line in enumerate(lines):
                if '>>>' in line and i+1 < len(lines):
                    next_line = lines[i+1].strip()
                    if next_line and not next_line.startswith('0:'):
                        return False, next_line
                    elif next_line.startswith('0:'):
                        return False, next_line
            return False, error[:150]
    except Exception as e:
        return False, str(e)

def main():
    shader_dir = 'librtt/Display/Shader'
    bgfx_dir = f'{shader_dir}/bgfx'
    shaderc_path = 'external/bgfx/.build/projects/xcode15/shadercRelease'
    
    kernel_files = []
    for f in os.listdir(shader_dir):
        if f.startswith('kernel_filter_') and f.endswith('_gl.lua'):
            kernel_files.append(f)
    
    kernel_files.sort()
    
    results = {
        'success': [],
        'compile_fail': [],
        'skipped': [],
        'error': []
    }
    
    for i, kernel_file in enumerate(kernel_files):
        filepath = os.path.join(shader_dir, kernel_file)
        kernel_name = kernel_file.replace('kernel_filter_', '').replace('_gl.lua', '')
        output_file = f'{bgfx_dir}/fs_filter_{kernel_name}.sc'
        
        print(f'[{i+1}/{len(kernel_files)}] Processing {kernel_name}...', end=' ')
        
        shader, error = process_kernel_file(filepath)
        
        if error:
            if error in ["graph-only kernel", "vertex-only kernel"]:
                print(f'SKIPPED ({error})')
                results['skipped'].append((kernel_name, error))
            else:
                print(f'ERROR: {error}')
                results['error'].append((kernel_name, error))
            continue
        
        # Post-process: fix atan to atan2
        shader = re.sub(r'atan\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)', r'atan2(\1, \2)', shader)
        
        # Post-process: fix simple varyings that can be replaced with v_UserData
        # crosshatch: grain -> floor(v_UserData.x)
        if kernel_name == 'crosshatch':
            shader = shader.replace('grain', 'floor(v_UserData.x)')
        # scatter: intensity -> v_UserData.x  
        elif kernel_name == 'scatter':
            shader = shader.replace('intensity', 'v_UserData.x')
        # woodCut: intensity_squared -> (v_UserData.x * v_UserData.x)
        elif kernel_name == 'woodCut':
            shader = shader.replace('intensity_squared', '(v_UserData.x * v_UserData.x)')
        
        with open(output_file, 'w') as f:
            f.write(shader)
        
        success, compile_error = compile_shader(output_file, shaderc_path)
        
        if success:
            print('OK')
            results['success'].append(kernel_name)
        else:
            print(f'COMPILE ERROR: {compile_error[:60]}')
            results['compile_fail'].append((kernel_name, compile_error))
    
    print('\n' + '='*60)
    print('SUMMARY')
    print('='*60)
    print(f"Total: {len(kernel_files)}")
    print(f"Success: {len(results['success'])}")
    print(f"Compile Fail: {len(results['compile_fail'])}")
    print(f"Skipped: {len(results['skipped'])}")
    print(f"Error: {len(results['error'])}")
    
    if results['compile_fail']:
        print('\nCompile failures:')
        for name, error in results['compile_fail']:
            print(f'  - {name}: {error[:80]}')
    
    return results

if __name__ == '__main__':
    main()
