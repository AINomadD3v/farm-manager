# Farm Manager - Update Guide

## Quick Update

The simplest way to update your Farm Manager installation:

```bash
./update.sh
```

This will:
1. ✅ Check your current version
2. ✅ Show you what's new
3. ✅ Pull latest changes from git
4. ✅ Rebuild the application
5. ✅ Prompt to update your installation (if already installed)

---

## Usage Options

### Basic Update (Interactive)
```bash
./update.sh
```
- Shows changes before updating
- Asks for confirmation at each step
- Prompts to install if already installed

### Auto-Install After Update
```bash
./update.sh --install
```
- Updates and rebuilds
- Automatically installs/updates without prompting

### Rebuild Without Updating
```bash
./update.sh --no-pull
```
- Skips git pull
- Just rebuilds with current code
- Useful for testing local changes

### Build in Debug Mode
```bash
./update.sh --debug
```
- Builds with debug symbols
- Useful for troubleshooting

### Skip Change Summary
```bash
./update.sh --skip-changes
```
- Doesn't show git log/diff
- Faster for quick updates

---

## Command Reference

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-i, --install` | Auto-install after rebuild |
| `-s, --skip-changes` | Skip showing git changes |
| `-d, --debug` | Build in Debug mode |
| `--no-pull` | Skip git pull (rebuild only) |

---

## Update Workflow

### 1. Standard Update (Recommended)
```bash
cd ~/tools/farm-manager
./update.sh
```

**What happens:**
- Shows your current version and git commit
- Fetches latest changes from repository
- Displays summary of new commits and changed files
- Asks if you want to proceed
- Pulls changes and rebuilds
- Offers to update installation if already installed

### 2. Quick Update for Installed Users
```bash
cd ~/tools/farm-manager
./update.sh --install
```

**What happens:**
- Same as above but automatically updates installation
- No prompts for installation step
- Fastest workflow for regular updates

### 3. Test Local Changes
```bash
cd ~/tools/farm-manager
# Make your changes
./update.sh --no-pull
```

**What happens:**
- Skips git pull
- Rebuilds with your local modifications
- Good for testing before committing

---

## First-Time Installation

If you haven't installed Farm Manager yet:

### On NixOS:
```bash
cd ~/tools/farm-manager
./update.sh              # Get latest code and build
./install-nixos.sh       # Install to system
```

### On Other Linux:
```bash
cd ~/tools/farm-manager
./update.sh              # Get latest code and build
./install.sh             # Install to ~/.local
```

---

## Environment Variables

You can customize behavior with environment variables:

```bash
# Build in Debug mode
BUILD_TYPE=Debug ./update.sh

# Auto-install without prompting
AUTO_INSTALL=true ./update.sh

# Combine options
BUILD_TYPE=Debug AUTO_INSTALL=true ./update.sh
```

---

## Troubleshooting

### "Not a git repository" Error
You're not in the farm-manager directory. Navigate to it:
```bash
cd ~/tools/farm-manager
./update.sh
```

### "Uncommitted changes" Warning
You have local modifications. Either:
- Commit your changes: `git commit -am "My changes"`
- Stash them: `git stash`
- Continue anyway when prompted

### Build Fails
1. Make sure you're in the nix development environment
2. Try entering manually: `nix develop`
3. Then run: `./update.sh --no-pull`

### Installation Not Detected
The script looks for `~/.local/share/qtscrcpy-farm/QtScrcpy`. If you haven't installed yet:
```bash
./install-nixos.sh    # NixOS
# or
./install.sh          # Other Linux
```

---

## What Gets Updated

### Code Changes
- All source files (QtScrcpy/*)
- Build scripts
- Configuration files

### Binary Output
- `output/x64/Release/QtScrcpy` (or Debug)
- All supporting files (adb, scrcpy-server, etc.)

### Installation (if installed)
- `~/.local/share/qtscrcpy-farm/QtScrcpy`
- `~/.local/bin/qtscrcpy-farm` (launcher)
- Desktop entry
- Systemd service (NixOS only)

---

## Version Information

Check your current version:
```bash
cd ~/tools/farm-manager
git log -1 --oneline
```

Check installed version:
```bash
ls -lh ~/.local/share/qtscrcpy-farm/QtScrcpy
```

View update history:
```bash
git log --oneline -20
```

---

## Advanced Usage

### Update Multiple Times Per Day
```bash
# Quick check and update
./update.sh --install --skip-changes
```

### Review Changes Before Updating
```bash
# See what's new without updating
git fetch origin
git log --oneline HEAD..origin/master
git diff --stat HEAD..origin/master

# Then update
./update.sh
```

### Rollback to Previous Version
```bash
# Find the commit you want
git log --oneline -20

# Rollback
git reset --hard <commit-hash>
./update.sh --no-pull
```

---

## Scheduled Updates (Optional)

### Create a Daily Update Script

Create `~/update-farm-manager.sh`:
```bash
#!/bin/bash
cd ~/tools/farm-manager
./update.sh --install --skip-changes
```

Make it executable:
```bash
chmod +x ~/update-farm-manager.sh
```

### Add to Cron (runs at 2 AM daily)
```bash
crontab -e
```

Add line:
```
0 2 * * * /home/yourusername/update-farm-manager.sh >> /tmp/farm-update.log 2>&1
```

---

## Support

### Check for Help
```bash
./update.sh --help
```

### View Build Output
All build output is shown in terminal during update.

### Report Issues
If updates fail consistently:
1. Check git status: `git status`
2. View build log
3. Report to development team with error output

---

## Best Practices

1. **Update regularly**: Run `./update.sh` weekly
2. **Review changes**: Don't skip the change summary
3. **Keep installation updated**: Use `--install` if installed
4. **Backup configs**: Before major updates, backup your settings
5. **Test after update**: Run `qtscrcpy-farm` to verify

---

## Quick Reference Card

```bash
# Standard update
./update.sh

# Fast update for installed users
./update.sh --install

# Rebuild without updating
./update.sh --no-pull

# Debug build
./update.sh --debug

# Get help
./update.sh --help
```

---

**Last Updated**: 2025-11-08
**Compatible With**: Farm Manager v2.3.0+
