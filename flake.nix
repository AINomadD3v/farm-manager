{
  description = "Phone Farm Manager - Enterprise-grade Qt-based Android device mirroring and farm management system";

  # HOW TO USE THIS FLAKE
  # ====================
  #
  # 1. INITIAL SETUP:
  #    git clone <repository> phone-farm-manager && cd phone-farm-manager
  #    nix develop                    # Enter development environment (minimal, fast)
  #
  # 2. BUILD THE APPLICATION:
  #    build_release                  # Build optimized release version
  #    # Output will be in: output/x64/RelWithDebInfo/QtScrcpy
  #
  # 3. START THE TOOL:
  #    run_qtscrcpy                   # Launch GUI for single device
  #    run_qtscrcpy_farm              # Launch multi-device farm mode (auto-detects all connected devices)
  #    run_qtscrcpy_device 192.168.1.100:5555  # Connect to specific device
  #
  # 4. SPECIALIZED ENVIRONMENTS:
  #    nix develop .#qt               # Qt6 development environment
  #    nix develop .#android          # Android SDK/NDK tools
  #    nix develop .#full             # Complete environment (slower startup)
  #
  # 5. PRODUCTION INSTALL:
  #    nix build                      # Build package
  #    nix run                        # Run directly from flake
  #    nix profile install .          # Install to user profile
  #
  # 6. NIXOS SERVICE (optional):
  #    Add to configuration.nix:
  #      services.qtscrcpy.enable = true;
  #
  # For more details, see README.md

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    android-nixpkgs = {
      url = "github:tadfisher/android-nixpkgs/stable";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, flake-utils, android-nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        # Optimized nixpkgs with minimal overlays for cache-friendly builds
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
            doCheckByDefault = false;
            enableParallelBuilding = true;
          };
        };

        # Separate package sets to avoid cache invalidation
        androidPkgs = android-nixpkgs.sdk.${system};
        
        # Lazy-loaded Android SDK (only built when android shell is used)
        androidComposition = androidPkgs (sdkPkgs: with sdkPkgs; [
          cmdline-tools-latest
          build-tools-35-0-0
          build-tools-34-0-0
          platform-tools
          platforms-android-35
          platforms-android-34
          platforms-android-33
          emulator
          system-images-android-33-google-apis-x86-64
          ndk-27-2-12479018
        ]);

        # Core development tools - shared across all shells
        coreTools = with pkgs; [
          git
          vim
          tmux
          jq
          which
          findutils
          coreutils
          bashInteractive
          android-tools  # System ADB for QtScrcpy
        ];

        # Build system essentials - minimal set
        buildTools = with pkgs; [
          cmake
          ninja
          pkg-config
          gnumake
          gcc13
          makeWrapper
        ];

        # Qt6 packages - separated for lazy loading
        qt6Full = with pkgs.qt6; [
          qtbase
          qtmultimedia
          qtnetworkauth
          qttools
          qtwayland
          qtdeclarative
          qtsvg
          qtimageformats
          qt5compat
          qtshadertools
          wrapQtAppsHook
        ];

        # Graphics and system libraries for Qt development
        qtSystemLibs = with pkgs; [
          mesa
          libGL
          libGLU
          xorg.libX11
          xorg.libXext
          xorg.libXrandr
          wayland
          wayland-protocols
          alsa-lib
          pulseaudio
          systemd
          udev
          openssl
          zlib
        ];

        # Heavy multimedia stack - only loaded in full environment
        multimediaStack = with pkgs; [
          ffmpeg_7
          gst_all_1.gstreamer
          gst_all_1.gst-plugins-base
          gst_all_1.gst-plugins-good
          gst_all_1.gst-plugins-bad
          gst_all_1.gst-vaapi
          pipewire
        ];

        # Development and debugging tools
        devTools = with pkgs; [
          gdb
          valgrind
          clang-tools
          strace
          htop
          lsof
        ];

        # Common environment variables for all shells
        commonEnvVars = ''
          export QT_SELECT=6
          export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt6.qtbase}/lib/qt-6/plugins/platforms"
          export QML2_IMPORT_PATH="${pkgs.qt6.qtdeclarative}/lib/qt-6/qml"
          export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/lib/qt-6/plugins"
          export CMAKE_PREFIX_PATH="${pkgs.qt6.qtbase}:${pkgs.qt6.qttools}:$CMAKE_PREFIX_PATH"
          export MAKEFLAGS="-j$(nproc)"
          export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
          
          # Configure QtScrcpy to use system ADB instead of bundled binary
          export QTSCRCPY_ADB_PATH="${pkgs.android-tools}/bin/adb"
        '';

        # Graphics settings for CPU-only software rendering (cross-machine compatibility)
        graphicsEnvVars = ''
          export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
          export __GLX_VENDOR_LIBRARY_NAME=mesa
          export QT_OPENGL=desktop
          export QT_QPA_PLATFORM=xcb
          export QT_XCB_GL_INTEGRATION=xcb_glx
          export MESA_GL_VERSION_OVERRIDE=3.3
          export MESA_GLSL_VERSION_OVERRIDE=330
          export LIBGL_ALWAYS_SOFTWARE=1
          export LIBGL_DRI3_DISABLE=1
        '';

        # Load farm functions
        farmFunctionsLoader = ''
          # Load QtScrcpy farm management functions
          if [ -f "scripts/farm-functions.sh" ]; then
            echo "📦 Loading QtScrcpy farm management functions..."
            source scripts/farm-functions.sh
            echo "✅ Farm functions loaded"
          else
            echo "⚠️  Farm functions not found at scripts/farm-functions.sh"
          fi
        '';

        # ADB symlink automation for development environments
        adbSymlinkFixer = ''
          # Function to ensure ADB symlink is correct
          fix_adb_symlink() {
            local output_dir="output/x64/RelWithDebInfo"
            if [ -d "$output_dir" ]; then
              if [ -f "$output_dir/adb" ] && [ ! -L "$output_dir/adb" ]; then
                echo "🔧 Replacing bundled ADB with NixOS system ADB symlink..."
                rm -f "$output_dir/adb"
                ln -sf "${pkgs.android-tools}/bin/adb" "$output_dir/adb"
                echo "✅ ADB symlink fixed: $output_dir/adb -> ${pkgs.android-tools}/bin/adb"
              elif [ ! -f "$output_dir/adb" ]; then
                echo "🔧 Creating ADB symlink for NixOS..."
                ln -sf "${pkgs.android-tools}/bin/adb" "$output_dir/adb"
                echo "✅ ADB symlink created: $output_dir/adb -> ${pkgs.android-tools}/bin/adb"
              fi
            fi
          }
          
          # Auto-fix ADB on shell entry
          fix_adb_symlink
        '';

        # === SHELL DEFINITIONS ===

        # DEFAULT SHELL - Minimal, fast loading (< 5 seconds)
        defaultShell = pkgs.mkShell {
          name = "qtscrcpy-minimal";
          buildInputs = coreTools ++ buildTools ++ qt6Full ++ qtSystemLibs ++ [ pkgs.ffmpeg_7.dev pkgs.ffmpeg_7.lib ];
          nativeBuildInputs = [ pkgs.qt6.wrapQtAppsHook pkgs.makeWrapper ];

          shellHook = ''
            echo "⚡ QtScrcpy Development Environment - Ready to Use!"
            echo "=========================================="
            echo ""
            echo "🚀 QUICK START:"
            echo "  1. Build:  build_release"
            echo "  2. Start:  run_qtscrcpy        # Single device mode"
            echo "            run_qtscrcpy_farm   # Multi-device farm mode"
            echo ""
            echo "📱 Device commands:"
            echo "  list_devices                   # List connected devices"
            echo "  kill_qtscrcpy                  # Stop all instances"
            echo "=========================================="
            ${commonEnvVars}
            ${graphicsEnvVars}
            ${adbSymlinkFixer}
            ${farmFunctionsLoader}

            init_dev_structure
            init_adb

            echo "✅ Ready to go! Run 'run_qtscrcpy_farm' to start."
          '';
        };

        # QT SHELL - Qt6 development focused
        qtShell = pkgs.mkShell {
          name = "qtscrcpy-qt-dev";
          buildInputs = coreTools ++ buildTools ++ qt6Full ++ qtSystemLibs;
          nativeBuildInputs = [ pkgs.qt6.wrapQtAppsHook pkgs.makeWrapper ];
          
          shellHook = ''
            echo "🎨 QtScrcpy Qt6 Development Environment"
            echo "======================================"
            echo "Qt6 Version: $(qmake -version 2>/dev/null | grep -E 'Qt version' || echo 'Qt6 Available')"
            echo "CMake Version: $(cmake --version | head -n1)"
            echo "======================================"
            ${commonEnvVars}
            ${graphicsEnvVars}
            ${adbSymlinkFixer}
            ${farmFunctionsLoader}
            
            init_dev_structure

            echo "🚀 HOW TO START Phone Farm Manager:"
            echo "  run_qtscrcpy                       # Launch single device GUI"
            echo "  run_qtscrcpy_farm                  # Launch multi-device farm mode"
            echo "  run_qtscrcpy_device 192.168.1.100:5555  # Connect to specific device"
            echo ""
            echo "🔨 Build commands:"
            echo "  build_debug                        # Build debug version"
            echo "  build_release                      # Build release version"
            echo "  clean_build                        # Clean build artifacts"
            echo ""
            echo "📱 Device commands:"
            echo "  list_devices                       # List connected devices"
            echo "  kill_qtscrcpy                      # Stop all instances"
            echo ""
            echo "🔄 Switch environments:"
            echo "  nix develop .#android              # Switch to Android tools"
            echo "  nix develop .#full                 # Switch to complete environment"
            echo ""
            echo "🎯 Ready for Qt6 development!"
          '';
        };

        # ANDROID SHELL - Android development tools
        androidShell = pkgs.mkShell {
          name = "qtscrcpy-android-dev";
          buildInputs = coreTools ++ buildTools ++ [ androidComposition ];
          nativeBuildInputs = [ pkgs.makeWrapper ];
          
          shellHook = ''
            echo "📱 QtScrcpy Android Development Environment"
            echo "=========================================="
            echo "Android SDK: ${androidComposition}/share/android-sdk"
            echo "NDK Version: $(ls ${androidComposition}/share/android-sdk/ndk/ 2>/dev/null || echo 'NDK Available')"
            echo "=========================================="
            ${commonEnvVars}
            ${adbSymlinkFixer}
            
            # Android development environment
            export ANDROID_HOME="${androidComposition}/share/android-sdk"
            export ANDROID_SDK_ROOT="${androidComposition}/share/android-sdk"
            export ANDROID_NDK_ROOT="${androidComposition}/share/android-sdk/ndk/27.2.12479018"
            export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
            export PATH="${androidComposition}/bin:$PATH"
            
            ${farmFunctionsLoader}
            init_dev_structure
            init_adb

            echo "🚀 HOW TO START Phone Farm Manager:"
            echo "  run_qtscrcpy                       # Launch single device GUI"
            echo "  run_qtscrcpy_farm                  # Launch multi-device farm mode"
            echo "  run_qtscrcpy_device <ip:port>      # Connect to specific device"
            echo ""
            echo "📱 Device management:"
            echo "  list_devices                       # List connected devices"
            echo "  adb devices                        # ADB device list"
            echo "  init_adb                           # Start ADB server"
            echo ""
            echo "🔧 Build commands:"
            echo "  build_release                      # Build release version"
            echo ""
            echo "🔄 Switch environments:"
            echo "  nix develop .#qt                   # Switch to Qt6 development"
            echo "  nix develop .#full                 # Switch to complete environment"
            echo ""
            echo "🎯 Ready for Android development!"
          '';
        };

        # FULL SHELL - Complete development environment (use sparingly)
        fullShell = pkgs.mkShell {
          name = "qtscrcpy-full-dev";
          buildInputs = coreTools ++ buildTools ++ qt6Full ++ qtSystemLibs ++ 
                       multimediaStack ++ devTools ++ [ androidComposition ];
          nativeBuildInputs = [ pkgs.qt6.wrapQtAppsHook pkgs.makeWrapper ];
          
          shellHook = ''
            echo "🚀 QtScrcpy Complete Development Environment"
            echo "============================================"
            echo "⚠️  Full environment - longer load time (~30-60 seconds)"
            echo "Qt6 Version: $(qmake -version 2>/dev/null | grep -E 'Qt version' || echo 'Qt6 Available')"
            echo "CMake Version: $(cmake --version | head -n1)"
            echo "Android SDK: ${androidComposition}/share/android-sdk"
            echo "============================================"
            ${commonEnvVars}
            ${graphicsEnvVars}
            ${adbSymlinkFixer}
            
            # Android environment
            export ANDROID_HOME="${androidComposition}/share/android-sdk"
            export ANDROID_SDK_ROOT="${androidComposition}/share/android-sdk"
            export ANDROID_NDK_ROOT="${androidComposition}/share/android-sdk/ndk/27.2.12479018"
            export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
            export PATH="${androidComposition}/bin:$PATH"
            
            # Development environment optimization
            export PKG_CONFIG_PATH="${pkgs.qt6.qtbase}/lib/pkgconfig:${pkgs.openssl.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"
            export CPATH="${pkgs.qt6.qtbase}/include:$CPATH"
            export LIBRARY_PATH="${pkgs.qt6.qtbase}/lib:$LIBRARY_PATH"
            export LD_LIBRARY_PATH="${pkgs.qt6.qtbase}/lib:${pkgs.mesa}/lib:$LD_LIBRARY_PATH"
            
            ${farmFunctionsLoader}
            init_dev_structure
            init_adb
            
            echo "📚 Complete command set available:"
            echo "  source scripts/farm-functions.sh  # Load farm functions manually if needed"
            echo ""
            echo "🔨 Build commands:"
            echo "  build_debug, build_release, clean_build"
            echo ""
            echo "🚀 QtScrcpy execution commands:"
            echo "  run_qtscrcpy, run_qtscrcpy_device, run_qtscrcpy_farm"
            echo ""
            echo "📱 Device management:"
            echo "  list_devices, kill_qtscrcpy"
            echo ""
            echo "🏭 Phone farm commands:"
            echo "  run_qtscrcpy_farm, run_qtscrcpy_custom"
            echo ""
            echo "🔄 Switch environments:"
            echo "  echo 'qt' > .qtscrcpy-env && direnv reload       # Switch to Qt6 only"
            echo "  echo 'android' > .qtscrcpy-env && direnv reload  # Switch to Android only"
            echo "  echo 'default' > .qtscrcpy-env && direnv reload  # Back to minimal"
            echo ""
            echo "🎯 Ready for complete QtScrcpy development!"
          '';
        };

      in {
        # === DEVELOPMENT SHELLS ===
        devShells = {
          default = defaultShell;  # Fast minimal shell
          qt = qtShell;           # Qt6 development
          android = androidShell; # Android tools
          full = fullShell;       # Complete environment
        };
        
        # Production package build (optimized, no tests)
        packages.qtscrcpy = pkgs.stdenv.mkDerivation rec {
          pname = "qtscrcpy";
          version = "2.3.0";
          
          src = ./.;
          
          nativeBuildInputs = buildTools ++ [ pkgs.qt6.wrapQtAppsHook pkgs.makeWrapper ];
          buildInputs = qt6Full ++ qtSystemLibs ++ [ androidComposition ];
          
          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
            "-DENABLE_DETAILED_LOGS=OFF"
            "-DQT_DESIRED_VERSION=6"
            "-DQT_FIND_PRIVATE_MODULES=ON"
            "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
          ];
          
          doCheck = false;
          doInstallCheck = false;
          
          postInstall = ''
            if [ -f $out/bin/QtScrcpy ]; then
              wrapProgram $out/bin/QtScrcpy \
                --set QT_QPA_PLATFORM_PLUGIN_PATH "${pkgs.qt6.qtbase}/lib/qt-6/plugins/platforms" \
                --set QML2_IMPORT_PATH "${pkgs.qt6.qtdeclarative}/lib/qt-6/qml" \
                --set QT_PLUGIN_PATH "${pkgs.qt6.qtbase}/lib/qt-6/plugins" \
                --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath (qt6Full ++ qtSystemLibs)}" \
                --set ANDROID_HOME "${androidComposition}/share/android-sdk" \
                --set ANDROID_SDK_ROOT "${androidComposition}/share/android-sdk" \
                --set QTSCRCPY_ADB_PATH "${pkgs.android-tools}/bin/adb"
                
              # Replace bundled ADB with symlink to NixOS system ADB
              if [ -f $out/bin/adb ]; then
                rm -f $out/bin/adb
                ln -sf "${pkgs.android-tools}/bin/adb" $out/bin/adb
              fi
            fi
          '';
          
          meta = with pkgs.lib; {
            description = "Enterprise Qt-based Android device mirroring and farm management";
            homepage = "https://github.com/barry-ran/QtScrcpy";
            license = licenses.asl20;
            platforms = platforms.linux;
            maintainers = [ ];
          };
        };
        
        packages.default = self.packages.${system}.qtscrcpy;

        # Lightweight NixOS module (removed heavy services)
        nixosModules.qtscrcpy = { config, lib, pkgs, ... }:
          with lib;
          let
            cfg = config.services.qtscrcpy;
          in {
            options.services.qtscrcpy = {
              enable = mkEnableOption "QtScrcpy phone farm service";
              package = mkOption {
                type = types.package;
                default = self.packages.${pkgs.system}.qtscrcpy;
                description = "QtScrcpy package to use";
              };
              user = mkOption {
                type = types.str;
                default = "qtscrcpy";
                description = "User to run QtScrcpy service";
              };
              dataDir = mkOption {
                type = types.path;
                default = "/var/lib/qtscrcpy";
                description = "Data directory for QtScrcpy";
              };
              port = mkOption {
                type = types.int;
                default = 27183;
                description = "Port for QtScrcpy communication";
              };
            };
            
            config = mkIf cfg.enable {
              users.users.${cfg.user} = {
                isSystemUser = true;
                group = cfg.user;
                home = cfg.dataDir;
                createHome = true;
                extraGroups = [ "adbusers" "video" "audio" ];
              };
              
              users.groups.${cfg.user} = {};
              users.groups.adbusers = {};
              
              programs.adb.enable = true;
              
              services.udev.extraRules = ''
                SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", GROUP="adbusers"
                SUBSYSTEM=="usb", ATTR{idVendor}=="04e8", MODE="0666", GROUP="adbusers"
                SUBSYSTEM=="usb", ATTR{idVendor}=="2717", MODE="0666", GROUP="adbusers"
              '';
            };
          };
      }
    );
}