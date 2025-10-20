# Home Manager Module for QtScrcpy Phone Farm Manager
# Add this to your home.nix to declaratively install the application
#
# Usage:
#   1. In your /etc/nixos/home.nix, add:
#      imports = [ /home/phone-node/tools/farm-manager/home-manager-module.nix ];
#
#   2. Then rebuild:
#      sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
#
# Or for standalone home-manager:
#      home-manager switch

{ config, pkgs, lib, ... }:

let
  # Path to the built QtScrcpy binary
  farmManagerSource = /home/phone-node/tools/farm-manager/output/x64/RelWithDebInfo;

  # Create a wrapped version of QtScrcpy
  qtscrcpy-farm = pkgs.stdenv.mkDerivation {
    pname = "qtscrcpy-farm";
    version = "2.2.2-farm";

    src = farmManagerSource;

    dontBuild = true;
    dontConfigure = true;

    nativeBuildInputs = [ pkgs.makeWrapper ];

    installPhase = ''
      mkdir -p $out/bin
      mkdir -p $out/share/qtscrcpy-farm

      # Copy the main executable
      cp QtScrcpy $out/share/qtscrcpy-farm/
      chmod +x $out/share/qtscrcpy-farm/QtScrcpy

      # Copy required files
      cp scrcpy-server $out/share/qtscrcpy-farm/

      # Copy optional files if they exist
      [ -f sndcpy.apk ] && cp sndcpy.apk $out/share/qtscrcpy-farm/ || true
      [ -f sndcpy.sh ] && cp sndcpy.sh $out/share/qtscrcpy-farm/ || true

      # Create symlink to adb (use system adb from android-tools)
      ln -sf ${pkgs.android-tools}/bin/adb $out/share/qtscrcpy-farm/adb

      # Create wrapper script
      makeWrapper $out/share/qtscrcpy-farm/QtScrcpy $out/bin/qtscrcpy-farm \
        --chdir "$out/share/qtscrcpy-farm" \
        --prefix PATH : ${lib.makeBinPath [ pkgs.android-tools ]}
    '';

    meta = with lib; {
      description = "QtScrcpy Phone Farm Manager - Manage multiple Android devices";
      license = licenses.asl20;
      platforms = platforms.linux;
    };
  };

in {
  # Install the package to home.packages
  home.packages = [ qtscrcpy-farm ];

  # Create desktop entry
  xdg.desktopEntries.qtscrcpy-farm = {
    name = "QtScrcpy Phone Farm Manager";
    genericName = "Android Device Farm Manager";
    comment = "Manage and monitor multiple Android devices simultaneously";
    exec = "qtscrcpy-farm";
    icon = "phone";
    terminal = false;
    categories = [ "Utility" "System" "Network" ];
    keywords = [ "android" "adb" "scrcpy" "farm" "devices" ];
  };

  # Optional: Create systemd user service for auto-start
  systemd.user.services.qtscrcpy-farm = {
    Unit = {
      Description = "QtScrcpy Phone Farm Manager";
      Documentation = "https://github.com/AINomadD3v/farm-manager";
      After = [ "graphical-session.target" ];
    };

    Service = {
      Type = "simple";
      ExecStart = "${qtscrcpy-farm}/bin/qtscrcpy-farm";
      Restart = "on-failure";
      RestartSec = 10;

      # Environment
      Environment = [
        "DISPLAY=:0"
        "QT_QPA_PLATFORM=xcb"
      ];
    };

    Install = {
      WantedBy = [ "default.target" ];
    };
  };
}
