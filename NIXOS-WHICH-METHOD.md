# Which NixOS Installation Method Should I Use?

Quick guide to help you choose the right installation method for QtScrcpy Phone Farm Manager on NixOS.

## Quick Decision Tree

```
Do you want the simplest installation?
  └─ YES → Use Method 1 (Quick Script)
  └─ NO ↓

Do you want declarative infrastructure-as-code?
  └─ YES → Use Method 2 (Home Manager)
  └─ NO ↓

Do you need full manual control?
  └─ YES → Use Method 3 (Manual)
```

## Method Comparison

| Feature | Quick Script | Home Manager | Manual |
|---------|-------------|--------------|--------|
| **Installation Time** | 10 seconds | 2-5 minutes | 5-10 minutes |
| **Commands Required** | 2 | Edit 1 file + rebuild | 6+ commands |
| **Difficulty** | Easy | Medium | Advanced |
| **Declarative** | No | Yes | No |
| **Survives Rebuilds** | Yes (in ~/.local) | Yes (managed) | Yes (in ~/.local) |
| **Easy Updates** | Re-run script | Rebuild config | Manual copy |
| **Uninstall** | 5 commands | Remove import | 5 commands |
| **Best For** | Testing, Quick Setup | Production, Team | Learning, Custom |

## Method 1: Quick Script (Recommended)

### Use This If:
- ✅ You want to get started immediately
- ✅ You're testing or evaluating the application
- ✅ You're not familiar with Nix/NixOS advanced features
- ✅ You don't need declarative configuration
- ✅ You want a standard Linux-like experience

### Installation:
```bash
chmod +x install-nixos.sh
./install-nixos.sh
```

### Pros:
- Fastest method (10 seconds)
- No NixOS configuration changes
- Familiar to Linux users
- Easy to test and iterate
- Works immediately

### Cons:
- Not declarative
- Need to re-run after rebuilding binary
- Not version-controlled with your config

### When to Update:
After rebuilding the application, just run the script again:
```bash
./install-nixos.sh
```
It will overwrite the old installation.

---

## Method 2: Home Manager (Best for Production)

### Use This If:
- ✅ You use home-manager already
- ✅ You want declarative configuration
- ✅ You're deploying to multiple machines
- ✅ You want version-controlled infrastructure
- ✅ You're comfortable with Nix expressions

### Installation:
1. Edit `/etc/nixos/home.nix`:
   ```nix
   imports = [ /home/phone-node/tools/farm-manager/home-manager-module.nix ];
   ```

2. Rebuild:
   ```bash
   sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
   ```

### Pros:
- Fully declarative (infrastructure as code)
- Survives system rebuilds automatically
- Easy to enable/disable (just add/remove import)
- Clean uninstallation
- Reproducible across systems
- Integrated with NixOS lifecycle

### Cons:
- Requires home-manager setup
- Longer rebuild times
- Need to rebuild after binary changes
- More complex for beginners

### When to Update:
After rebuilding the application:
```bash
sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
```
Home-manager will detect changes and update automatically.

---

## Method 3: Manual Installation

### Use This If:
- ✅ You want to learn the installation process
- ✅ You need custom paths or configuration
- ✅ You're troubleshooting or debugging
- ✅ You want complete control
- ✅ You're creating your own installer

### Installation:
See `INSTALL-NIXOS.md` → Method 3 section for step-by-step commands.

### Pros:
- Complete control over every step
- Educational (learn how it works)
- Flexible for custom setups
- No dependencies on scripts or modules

### Cons:
- Most time-consuming
- Error-prone (manual typing)
- Need to remember all steps
- Hard to reproduce

### When to Update:
Manually copy files again or create your own update script.

---

## Detailed Scenarios

### Scenario: First-Time User
**Recommendation**: Method 1 (Quick Script)

You just want to try the application and see if it works for you.
```bash
./install-nixos.sh
~/.local/bin/qtscrcpy-farm
```

### Scenario: Development Team
**Recommendation**: Method 2 (Home Manager)

Multiple developers need consistent setup across machines.
```nix
# In your team's NixOS config repository
imports = [ ./modules/qtscrcpy-farm.nix ];
```

### Scenario: Production Server
**Recommendation**: Method 2 (Home Manager)

Server needs automatic updates and declarative configuration.
```nix
# In /etc/nixos/home.nix
imports = [ /opt/qtscrcpy/home-manager-module.nix ];
```

### Scenario: CI/CD Pipeline
**Recommendation**: Method 1 (Quick Script)

Automated testing needs quick installation in containers.
```bash
./install-nixos.sh && ~/.local/bin/qtscrcpy-farm --test
```

### Scenario: Learning NixOS
**Recommendation**: Method 3 (Manual)

You want to understand how NixOS works and learn the details.
Follow the step-by-step guide in `INSTALL-NIXOS.md`.

### Scenario: Custom Integration
**Recommendation**: Method 3 (Manual)

You need to integrate with custom tooling or paths.
Use manual installation and adapt paths as needed.

---

## Auto-Start Comparison

All methods support systemd auto-start, but the setup differs:

### Method 1 & 3 (Script/Manual):
```bash
systemctl --user enable qtscrcpy-farm
systemctl --user start qtscrcpy-farm
```

### Method 2 (Home Manager):
Auto-start is already configured in the module. To enable:
```nix
# In home-manager-module.nix, the service is already defined
# Just add to imports and rebuild
```

Or disable the service in the module if you don't want auto-start:
```nix
# Comment out the systemd.user.services section
```

---

## Migration Between Methods

### From Method 1 to Method 2:
1. Uninstall Method 1:
   ```bash
   rm -rf ~/.local/share/qtscrcpy-farm
   rm ~/.local/bin/qtscrcpy-farm
   rm ~/.local/share/applications/qtscrcpy-farm.desktop
   rm ~/.config/systemd/user/qtscrcpy-farm.service
   systemctl --user daemon-reload
   ```

2. Install Method 2:
   ```bash
   # Edit home.nix and rebuild
   ```

### From Method 2 to Method 1:
1. Remove from home.nix:
   ```nix
   # Remove the import line
   ```

2. Rebuild:
   ```bash
   sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
   ```

3. Install Method 1:
   ```bash
   ./install-nixos.sh
   ```

---

## Summary Recommendation

| Your Situation | Use This |
|----------------|----------|
| 🎯 Just want it to work | **Method 1** |
| 🏢 Production deployment | **Method 2** |
| 👥 Team with shared config | **Method 2** |
| 🚀 Quick testing | **Method 1** |
| 📚 Learning NixOS | **Method 3** |
| 🔧 Custom setup | **Method 3** |
| 🏠 Personal workstation | **Method 1 or 2** |
| 🖥️ Server environment | **Method 2** |

---

## Still Not Sure?

### Start with Method 1, then decide:
```bash
# Try it out first
./install-nixos.sh
~/.local/bin/qtscrcpy-farm

# Like it? Keep using Method 1
# Want it declarative? Switch to Method 2
# Need customization? Try Method 3
```

**You can always change methods later.** There's no lock-in.

---

## Quick Reference

| Method | Install Command | Run Command |
|--------|----------------|-------------|
| 1 | `./install-nixos.sh` | `~/.local/bin/qtscrcpy-farm` |
| 2 | Edit home.nix + rebuild | `qtscrcpy-farm` |
| 3 | Manual steps | `~/.local/bin/qtscrcpy-farm` |

## Documentation Links

- **Quick Start**: [QUICKSTART-NIXOS.md](QUICKSTART-NIXOS.md)
- **Complete Guide**: [INSTALL-NIXOS.md](INSTALL-NIXOS.md)
- **Summary**: [NIXOS-INSTALLATION-SUMMARY.md](NIXOS-INSTALLATION-SUMMARY.md)
- **Home Manager Module**: [home-manager-module.nix](home-manager-module.nix)

---

**Bottom Line**: If you're reading this and still unsure, just use **Method 1**. It works great and you can always change later.
