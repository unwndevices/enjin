#!/bin/bash
set -e
echo "Building documentation locally..."

if [ ! -d "docs/node_modules" ]; then
    echo "Installing npm dependencies..."
    cd docs && npm install && cd ..
fi

echo "Generating Doxygen XML..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>/dev/null || cmake ..
cmake --build . --target docs
cd ..

echo "Building Docusaurus site..."
cd docs
npm run build
cd ..

echo "Serving site at http://localhost:8080"
echo "Press Ctrl+C to stop"
cd docs/build
python3 -m http.server 8080
