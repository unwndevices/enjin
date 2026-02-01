# Deployment Guide

This guide explains how the enjin documentation is deployed and how to test changes locally.

## Overview

The documentation is automatically deployed to GitHub Pages at:
**https://unwndevices.github.io/enjin/**

## Deployment Process

Documentation is deployed automatically via GitHub Actions when:

- Changes are pushed to the `main` branch
- Changes affect:
  - `docs/**` - Documentation source files
  - `include/**` - Header files that generate API documentation
  - `.github/workflows/docs.yml` - The deployment workflow itself

### Build Pipeline

The GitHub Actions workflow (`.github/workflows/docs.yml`) performs the following steps:

1. **Checkout** - Retrieve the latest code
2. **Setup Node.js** - Install Node.js 18 and cache dependencies
3. **Install dependencies** - Run `npm ci` in docs directory
4. **Install tools** - Install Doxygen and Graphviz
5. **Generate Doxygen XML** - Run `cmake --build . --target docs`
6. **Generate API docs** - Run `scripts/generate-api-docs.js`
7. **Build Docusaurus** - Run `npm run build` in docs directory
8. **Deploy** - Upload to GitHub Pages

Pull requests also trigger the workflow but do not deploy to GitHub Pages (for verification only).

## Deployment Status

To check the deployment status:

1. Go to **Actions** tab: https://github.com/unwndevices/enjin/actions
2. Click on the latest **Deploy Documentation** workflow run
3. View the build logs for any errors or warnings
4. When the workflow completes, the documentation is deployed

The deployment typically takes 2-3 minutes to complete.

## Local Preview

Before pushing changes, you can preview the documentation locally:

### Using the deployment script

```bash
./scripts/deploy-docs.sh
```

This script:
- Installs npm dependencies (if needed)
- Generates Doxygen XML
- Runs API documentation generation
- Builds Docusaurus
- Serves the site at http://localhost:8080

Press `Ctrl+C` to stop the server.

### Manual build

If you prefer to build manually:

```bash
# 1. Install npm dependencies (first time only)
cd docs && npm install && cd ..

# 2. Generate Doxygen XML
mkdir -p build
cd build
cmake ..
cmake --build . --target docs
cd ..

# 3. Generate API documentation
node scripts/generate-api-docs.js

# 4. Build Docusaurus
cd docs
npm run build

# 5. Serve locally
npm run serve
```

Then open http://localhost:3000 in your browser.

## Manual Deployment

If you need to deploy manually without pushing to main:

```bash
cd docs
npm run deploy
```

This uses `docusaurus deploy` to push the build to the `gh-pages` branch.

## Troubleshooting

### Deployment fails

**Symptoms:** GitHub Actions workflow shows red X, deployment step fails

**Possible causes:**
1. **Broken documentation links** - Check the build logs for `onBrokenLinks` errors
2. **Doxygen warnings** - XML generation warnings may indicate missing documentation
3. **Node.js errors** - Check if npm packages are outdated or incompatible

**Solutions:**
1. Run the deployment script locally to reproduce the issue
2. Check the workflow logs for specific error messages
3. Fix broken links or missing documentation
4. Update dependencies if needed

### 404 errors on GitHub Pages

**Symptoms:** https://unwndevices.github.io/enjin/ returns 404

**Possible causes:**
1. **GitHub Pages not enabled** - Pages feature may be disabled
2. **Wrong branch** - Pages configured for wrong branch
3. **Wrong folder** - Pages configured for wrong source folder

**Solutions:**
1. Check GitHub Pages settings: https://github.com/unwndevices/enjin/settings/pages
2. Ensure GitHub Pages is enabled
3. Set source to **"GitHub Actions"**
4. Re-run the workflow deployment

### Broken links after deployment

**Symptoms:** Site loads but links return 404 or show broken images

**Possible causes:**
1. **baseUrl mismatch** - `docusaurus.config.js` baseUrl doesn't match GitHub Pages URL
2. **Path issues** - Hardcoded paths instead of using Docusaurus links

**Solutions:**
1. Verify `docusaurus.config.js` has:
   ```javascript
   url: 'https://unwndevices.github.io',
   baseUrl: '/enjin/',
   ```
2. Use Docusaurus Link component for internal links
3. Clear browser cache and try again

### Doxygen warnings

**Symptoms:** Workflow logs show numerous Doxygen warnings during XML generation

**Possible causes:**
1. **Missing documentation** - Some functions or classes lack documentation comments
2. **Parameter mismatches** - Documented parameters don't match function signatures
3. **Empty documentation** - Documentation blocks without content

**Solutions:**
1. Review warnings to identify specific issues
2. Add missing `@param`, `@return`, and `@brief` tags
3. Fix parameter documentation to match signatures
4. Note: 210 warnings are acceptable for initial setup (see STATE.md)

### API pages not generated

**Symptoms:** API reference section is empty or missing

**Possible causes:**
1. **Doxygen XML missing** - CMake docs target failed or wasn't run
2. **XML parsing errors** - generate-api-docs.js encountered errors
3. **API plugin disabled** - Docusaurus plugin commented out in config

**Solutions:**
1. Check build logs for Doxygen errors
2. Run API generation script locally: `node scripts/generate-api-docs.js`
3. Verify `docs/src/api/` directory contains generated files
4. Check Docusaurus config for API plugin settings

## GitHub Pages Configuration

The documentation is hosted on GitHub Pages with these settings:

- **Owner:** unwndevices
- **Repository:** enjin
- **URL:** https://unwndevices.github.io/enjin/
- **Deployment method:** GitHub Actions (not gh-pages branch)
- **Branch:** Deployed from workflow runs on main branch

To verify or modify:
1. Go to repository Settings > Pages
2. Ensure "Source" is set to **"GitHub Actions"**
3. The workflow will deploy automatically on main branch pushes

## Next Steps

- [ ] Verify GitHub Pages is enabled in repository settings
- [ ] Push changes to main branch
- [ ] Monitor GitHub Actions workflow
- [ ] Visit https://unwndevices.github.io/enjin/ to verify deployment
- [ ] Test local preview before pushing changes
