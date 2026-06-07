---
name: release-management
description: Creates GitHub releases following the Dark Souls Tracker project's standard format with installation instructions, changelog sections, and proper versioning.
version: 1.0.0
author: JoShMiQueL
tags: [github, release, changelog, versioning]
---

# Release Management Skill

This skill provides a standardized workflow for creating GitHub releases for the Dark Souls Tracker project.

## Quick Start

### Simple Workflow
1. **Analyze changes:** `git log vPREVIOUS..HEAD --oneline`
2. **Choose template:** Copy appropriate template from `templates/` directory
3. **Customize:** Replace placeholders and add changelog content
4. **Create release:** Use the file with `gh release create --notes-file`

### Example Workflow
```bash
# 1. Get last tag and analyze changes
LAST_TAG=$(git describe --tags --abbrev=0)
git log $LAST_TAG..HEAD --oneline

# 2. Copy template (e.g., for minor release)
cp templates/minor-release.txt release_notes.txt

# 3. Edit release_notes.txt with your changes
# Replace {VERSION} and {PREVIOUS_VERSION} placeholders
# Add your changelog content

# 4. Create tag and release
git tag -a v1.2.0 -m "Release v1.2.0"
git push origin v1.2.0
gh release create v1.2.0 --title "v1.2.0 - Description" --notes-file release_notes.txt

# 5. Clean up
rm release_notes.txt
```

## Release Format Standard

All releases must follow the exact format defined in the templates. Key elements:

### Required Sections
- **Installation** (always first) - Standard installation instructions
- **Verify download** - SHA256 checksum verification
- **Changelog sections** - Organized by category with emojis
- **Example Usage** - Practical examples of new features
- **Full Changelog link** - Comparison to previous version

### Standard Changelog Sections
- 🎉 **What's New** - New features and user-facing changes
- 🔧 **Refactoring** - Code improvements and reorganization
- 🐛 **Bug Fixes** - Bug fixes and corrections
- 📦 **CI/CD** - Build and deployment changes
- 📝 **Documentation** - Documentation updates

## Workflow

### Step 1: Analyze Changes
```bash
# Get commits since last tag
git log vPREVIOUS..HEAD --oneline

# Review each commit in detail
git show <commit-hash> --stat
git show <commit-hash>
```

### Step 2: Determine Version
- **Major (X.0.0)**: Breaking changes, incompatible API changes
- **Minor (x.Y.0)**: New features, backward-compatible changes
- **Patch (x.y.Z)**: Bug fixes, documentation updates

### Step 3: Create Release Notes
1. Copy appropriate template from `templates/` directory:
   - `major-release.txt` - For breaking changes
   - `minor-release.txt` - For new features
   - `patch-release.txt` - For bug fixes only

2. Replace placeholders:
   - `{VERSION}` → Your new version (e.g., "1.2.0")
   - `{PREVIOUS_VERSION}` → Previous version (e.g., "1.1.0")

3. Add your changelog content following the format

**IMPORTANT:** Always use a file to avoid shell escaping issues with backticks.

### Step 4: Create Tag and Release
```bash
# Create annotated tag
git tag -a vX.Y.Z -m "Release vX.Y.Z"

# Push tag to GitHub
git push origin vX.Y.Z

# Create release using the notes file
gh release create vX.Y.Z --title "vX.Y.Z - Brief Description" --notes-file release_notes.txt

# Clean up
rm release_notes.txt
```

### Step 5: Edit Release (if needed)
```bash
# To update release notes after creation
gh release edit vX.Y.Z --notes-file release_notes.txt
```

## Changelog Writing Guidelines

### Commit Analysis
- Review each commit individually with `git show <hash>`
- Group related commits together
- Focus on user-facing changes
- Include technical details for developers

### Section Organization
- **Features first** - What users care about most
- **Refactoring second** - Technical improvements
- **Bug fixes third** - Corrections and fixes
- **CI/CD fourth** - Build and deployment changes
- **Documentation last** - Docs and guides

### Writing Style
- Use present tense ("Adds" not "Added")
- Be specific about what changed
- Include context for why changes were made
- Use backticks for code, files, and paths
- Use emojis for section headers

### Example Changelog Entries
See the `examples/` directory for detailed examples:
- `examples/feature-examples.md` - Feature addition examples
- `examples/refactoring-examples.md` - Refactoring examples
- `examples/bugfix-examples.md` - Bug fix examples

## Common Pitfalls

### Shell Escaping Issues
**Problem:** Backticks and special characters get mangled when passed directly to `gh release`.

**Solution:** Always use a notes file:
```bash
# WRONG - backticks get interpreted
gh release edit v1.1.0 --notes "Download `dinput8.dll` below"

# RIGHT - use file
echo "Download \`dinput8.dll\` below" > notes.txt
gh release edit v1.1.0 --notes-file notes.txt
```

### Missing Installation Section
**Problem:** Users don't know how to install the mod.

**Solution:** Always include the standard installation section at the top (included in all templates).

### Inconsistent Formatting
**Problem:** Different releases look different.

**Solution:** Always use the templates from `templates/` directory.

### Missing Full Changelog Link
**Problem:** Users can't see detailed commit history.

**Solution:** All templates include the full changelog comparison link at the end.

## Skill File Structure

```
release-management/
├── SKILL.md                    # Main skill file (this file)
├── templates/                  # Release note templates
│   ├── major-release.txt      # Breaking changes template
│   ├── minor-release.txt      # New features template
│   └── patch-release.txt      # Bug fixes template
├── examples/                   # Changelog entry examples
│   ├── feature-examples.md   # Feature addition examples
│   ├── refactoring-examples.md # Refactoring examples
│   └── bugfix-examples.md    # Bug fix examples
└── references/                 # Reference materials
    └── v1.1.0-reference.md   # Reference release format
```

## Version History Reference

- **v1.0.0** - Initial release with basic stat tracking
- **v1.1.0** - Template system & custom formatting

**Reference Release:** See [v1.1.0](https://github.com/JoShMiqueL/dark-souls-remastered-stream-overlay/releases/tag/v1.1.0) for a complete example of the standard format. Detailed analysis available in `references/v1.1.0-reference.md`.

## Quick Reference Commands

### Get Last Tag
```bash
git describe --tags --abbrev=0
```

### Get Commits Since Last Tag
```bash
git log vPREVIOUS..HEAD --oneline
```

### Get Detailed Commit Info
```bash
git show <commit-hash> --stat
git show <commit-hash>
```

### Create Annotated Tag
```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
```

### Push Tag to GitHub
```bash
git push origin vX.Y.Z
```

### Create Release from File
```bash
gh release create vX.Y.Z --title "vX.Y.Z - Description" --notes-file release_notes.txt
```

### Edit Existing Release
```bash
gh release edit vX.Y.Z --notes-file release_notes.txt
```

### Delete Tag (Local and Remote)
```bash
git tag -d vX.Y.Z
git push origin :refs/tags/vX.Y.Z
```

### List All Tags
```bash
git tag -l
```

## Testing

After creating a release:
1. Visit the release page on GitHub
2. Verify formatting looks correct
3. Check that all links work
4. Ensure installation instructions are clear
5. Verify the full changelog link points to the correct comparison
6. Test the actual DLL if possible
7. Verify the SHA256 checksum matches