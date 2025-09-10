#!/usr/bin/env bash
# QtScrcpy Phone Farm Management Functions
# Extracted from flake.nix for better organization and faster shell loading

# Development convenience functions
build_debug() {
  echo "🔨 Building QtScrcpy in Debug mode..."
  cd build/debug
  cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  make -j$(nproc)
  cd ../..
}

build_release() {
  echo "🔨 Building QtScrcpy in Release mode..."
  cd build/release  
  cmake ../.. -DCMAKE_BUILD_TYPE=Release
  make -j$(nproc)
  cd ../..
}

clean_build() {
  rm -rf build/*
  echo "🧹 Build directories cleaned"
}

run_qtscrcpy() {
  echo "🚀 Starting QtScrcpy with optimized graphics settings..."
  echo "📱 Connected devices:"
  adb devices
  echo "🎮 Launching QtScrcpy GUI with desktop OpenGL..."
  # Force desktop OpenGL to fix shader precision issues
  QT_OPENGL=desktop \
  QT_XCB_GL_INTEGRATION=xcb_glx \
  MESA_GL_VERSION_OVERRIDE=3.3 \
  MESA_GLSL_VERSION_OVERRIDE=330 \
  ./output/x64/RelWithDebInfo/QtScrcpy "$@"
}

run_qtscrcpy_device() {
  local device_ip="$1"
  if [ -z "$device_ip" ]; then
    echo "❌ Usage: run_qtscrcpy_device <device_ip:port>"
    echo "📋 Example: run_qtscrcpy_device 192.168.40.101:5555"
    return 1
  fi
  echo "🚀 Starting QtScrcpy for device: $device_ip with desktop OpenGL..."
  # Force desktop OpenGL to fix shader precision issues
  QT_OPENGL=desktop \
  QT_XCB_GL_INTEGRATION=xcb_glx \
  MESA_GL_VERSION_OVERRIDE=3.3 \
  MESA_GLSL_VERSION_OVERRIDE=330 \
  ./output/x64/RelWithDebInfo/QtScrcpy -s "$device_ip" --window-title "Phone Farm - $device_ip"
}

kill_qtscrcpy() {
  echo "🛑 Killing all QtScrcpy processes..."
  pkill -f QtScrcpy || echo "✅ No QtScrcpy processes running"
}

list_devices() {
  echo "📱 Scanning for connected Android devices..."
  adb devices -l
}

run_qtscrcpy_farm() {
  echo "🏭 Starting QtScrcpy Phone Farm - Multi-Device Mode"
  echo "📱 Detecting connected devices..."
  
  # Get list of connected devices
  local devices=($(adb devices | grep -E '\tdevice$' | cut -f1))
  local device_count=${#devices[@]}
  
  if [ $device_count -eq 0 ]; then
    echo "❌ No devices detected. Please ensure devices are connected and authorized."
    echo "💡 Try: adb devices"
    return 1
  fi
  
  echo "✅ Found $device_count device(s)"
  
  # Kill any existing instances
  kill_qtscrcpy
  sleep 2
  
  # Launch QtScrcpy for each device with proper positioning
  local x_pos=0
  local y_pos=0
  local window_width=350
  local window_height=400
  local columns=4  # Arrange in 4 columns
  
  for i in "${!devices[@]}"; do
    local device="${devices[$i]}"
    local device_num=$((i + 1))
    local title="Farm Device $device_num - $device"
    
    # Calculate window position (grid layout)
    local col=$((i % columns))
    local row=$((i / columns))
    x_pos=$((col * window_width))
    y_pos=$((row * window_height))
    
    echo "🚀 Launching Device $device_num: $device at position ($x_pos, $y_pos)"
    
    # Launch with desktop OpenGL and positioning
    QT_OPENGL=desktop \
    QT_XCB_GL_INTEGRATION=xcb_glx \
    MESA_GL_VERSION_OVERRIDE=3.3 \
    MESA_GLSL_VERSION_OVERRIDE=330 \
    ./output/x64/RelWithDebInfo/QtScrcpy \
      -s "$device" \
      --window-title "$title" \
      --window-x "$x_pos" \
      --window-y "$y_pos" \
      --max-size 800 \
      --bit-rate 4M \
      --max-fps 30 \
      --stay-awake &
    
    # Small delay to prevent resource conflicts
    sleep 1.5
  done
  
  echo "🎯 Farm Mode: Launched $device_count devices in grid layout"
  echo "💡 Use 'kill_qtscrcpy' to stop all devices"
}

run_qtscrcpy_custom() {
  local device_list="$1"
  if [ -z "$device_list" ]; then
    echo "❌ Usage: run_qtscrcpy_custom \"device1:port device2:port device3:port\""
    echo "📋 Example: run_qtscrcpy_custom \"192.168.40.101:5555 192.168.40.102:5555\""
    return 1
  fi
  
  echo "🏭 Starting Custom Multi-Device Configuration"
  
  # Kill any existing instances
  kill_qtscrcpy
  sleep 2
  
  # Convert device list to array
  local devices=($device_list)
  local device_count=${#devices[@]}
  
  echo "✅ Launching $device_count custom devices"
  
  # Launch each device with custom positioning
  local x_pos=0
  local y_pos=0
  local window_width=350
  local window_height=400
  
  for i in "${!devices[@]}"; do
    local device="${devices[$i]}"
    local device_num=$((i + 1))
    local title="Custom Device $device_num - $device"
    
    # Calculate window position
    local col=$((i % 3))  # 3 columns for custom mode
    local row=$((i / 3))
    x_pos=$((col * window_width))
    y_pos=$((row * window_height))
    
    echo "🚀 Launching Custom Device $device_num: $device"
    
    QT_OPENGL=desktop \
    QT_XCB_GL_INTEGRATION=xcb_glx \
    MESA_GL_VERSION_OVERRIDE=3.3 \
    MESA_GLSL_VERSION_OVERRIDE=330 \
    ./output/x64/RelWithDebInfo/QtScrcpy \
      -s "$device" \
      --window-title "$title" \
      --window-x "$x_pos" \
      --window-y "$y_pos" \
      --max-size 1080 \
      --bit-rate 6M \
      --max-fps 60 \
      --stay-awake &
    
    sleep 1.5
  done
  
  echo "🎯 Custom Mode: Launched $device_count devices"
}

# Initialize development structure if needed
init_dev_structure() {
  if [ ! -d "build" ]; then
    echo "📁 Creating build directory structure..."
    mkdir -p build/{debug,release,test}
  fi
  
  if [ ! -d "logs" ]; then
    mkdir -p logs
  fi
  
  if [ ! -d "data" ]; then
    mkdir -p data/{db,cache,uploads}
  fi
}

# ADB server setup
init_adb() {
  if command -v adb >/dev/null 2>&1; then
    echo "🔧 Starting ADB server..."
    adb start-server 2>/dev/null || true
  fi
}

# Functions are automatically available when sourced