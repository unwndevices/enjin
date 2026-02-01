/**
 * Generate API documentation from Doxygen XML
 *
 * Parses Doxygen XML files and generates Docusaurus markdown pages
 * organized by module (core, graphics, ui, utils, components, animation, effects, scripting, abstract)
 */

const fs = require('fs');
const path = require('path');
const xml2js = require('xml2js');

// Configuration
const config = {
  xmlDir: path.join(__dirname, '..', 'docs', 'xml'),
  outputDir: path.join(__dirname, '..', 'docs', 'src', 'api'),
  modules: {
    abstract: {
      classes: ['ICanvas', 'IComponent', 'IScene'],
      description: 'Abstract interfaces for core engine components',
    },
    animation: {
      classes: ['AnimationSystem', 'AnimationTrack', 'EasingFunctions', 'C_Animation'],
      description: 'Animation system and easing functions',
    },
    components: {
      classes: [
        'ButtonDial', 'C_Draw', 'C_Drawable', 'C_ImageCache', 'C_LuaScript',
        'C_Planet', 'C_Position', 'C_Probe', 'C_Satellite', 'C_Sprite',
        'FillUpGauge', 'Label', 'Slider', 'Tickmarks'
      ],
      description: 'Component types for game objects',
    },
    core: {
      classes: [
        'Component', 'ComponentBase', 'ComponentQuery', 'ComponentQuery_Iterator',
        'ComponentStorage', 'ComponentStorage_Iterator', 'EntityManager', 'HandlePool',
        'Object', 'ObjectCollection', 'Scene', 'SceneStateMachine',
        'Signal', 'SignalConnection', 'StackAllocator', 'StaticPool'
      ],
      description: 'Core engine components and systems',
    },
    effects: {
      classes: ['PostFx', 'Effects'],
      description: 'Post-processing effects',
    },
    graphics: {
      classes: [
        'Canvas4', 'Canvas4_ESP32S3', 'Canvas8', 'CanvasExtended',
        'CanvasGraphicsAdapter', 'C_Canvas', 'ImageExporter', 'PackedPixel4',
        'Primitives', 'RenderSystem', 'Sprite', 'TextRenderer'
      ],
      description: 'Graphics rendering system',
    },
    scripting: {
      classes: [
        'LuaBindings', 'LuaCanvas', 'LuaEngine', 'LuaFileSystem',
        'LuaInterpreter', 'LuaPlatform', 'LuaScriptSystem', 'ScriptFactory',
        'MinimalLuaInterpreter', 'IScriptGraphics', 'IScriptInterpreter', 'FileInterface'
      ],
      description: 'Lua scripting integration',
    },
    ui: {
      classes: ['System', 'SystemBase', 'SystemManager'],
      description: 'UI system management',
    },
    utils: {
      classes: ['InputSystem', 'math_TrigLUT', 'Colors', 'DrawingHelpers', 'Noise', 'Polar', 'Signals'],
      description: 'Utility functions and helpers',
    },
  },
  namespaces: {
    Colors: { module: 'utils', description: 'Color constants and utilities' },
    DrawingHelpers: { module: 'utils', description: 'Drawing utility functions' },
    math: { module: 'utils', description: 'Mathematical functions' },
    Noise: { module: 'utils', description: 'Noise generation functions' },
    Polar: { module: 'utils', description: 'Polar coordinate utilities' },
    Signals: { module: 'utils', description: 'Signal and event handling' },
  },
};

// Ensure output directories exist
Object.keys(config.modules).forEach(module => {
  const moduleDir = path.join(config.outputDir, module);
  if (!fs.existsSync(moduleDir)) {
    fs.mkdirSync(moduleDir, { recursive: true });
  }
});

// Parse XML file
function parseXmlFile(filePath) {
  const xmlContent = fs.readFileSync(filePath, 'utf-8');
  return new Promise((resolve, reject) => {
    xml2js.parseString(xmlContent, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

// Extract text from XML nodes
function extractText(node) {
  if (!node) return '';
  if (typeof node === 'string') return node;
  if (Array.isArray(node)) {
    return node.map(extractText).join('');
  }
  if (typeof node === 'object') {
    return Object.values(node).map(extractText).join('');
  }
  return '';
}

// Format C++ type for display
function formatType(type) {
  if (!type) return 'void';
  if (typeof type === 'string') {
    return type
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&')
      .replace(/std::/g, '')
      .replace(/enjin2::/g, '');
  }
  return extractText(type);
}

// Format method signature
function formatMethod(method) {
  const name = method.name[0];
  const type = formatType(method.type);
  const args = method.argsstring ? method.argsstring[0] : '()';
  const isConst = method.$.const === 'yes';
  const isStatic = method.$.static === 'yes';
  const isVirtual = method.$.virt === 'virtual';
  const isConstexpr = method.$.constexpr === 'yes';

  let modifiers = [];
  if (isStatic) modifiers.push('static');
  if (isVirtual) modifiers.push('virtual');
  if (isConstexpr) modifiers.push('constexpr');

  const modifiersStr = modifiers.length > 0 ? modifiers.join(' ') + ' ' : '';
  return `${modifiersStr}${type} ${name}${args}${isConst ? ' const' : ''}`;
}

// Sanitize class name for file
function sanitizeClassName(name) {
  return name
    .replace(/::/g, '_')
    .replace(/<[^>]*>/g, '')
    .replace(/\s+/g, '')
    .replace(/__/g, '_');
}

// Convert to PascalCase for display
function toPascalCase(name) {
  return name
    .replace(/_([a-z])/g, (_, c) => c.toUpperCase())
    .replace(/^([a-z])/, (_, c) => c.toUpperCase());
}

// Process class XML and generate markdown
async function processClass(className, module, classInfo) {
  try {
    // Try different filename patterns
    let xmlFile;
    const possibleNames = [
      `classenjin2_1_1${className.replace(/::/g, '_1_1')}.xml`,
      `classenjin_1_1${className.replace(/::/g, '_1_1')}.xml`,
    ];

    for (const name of possibleNames) {
      const filePath = path.join(config.xmlDir, name);
      if (fs.existsSync(filePath)) {
        xmlFile = filePath;
        break;
      }
    }

    if (!xmlFile) {
      console.log(`Warning: XML file not found for ${className}`);
      return null;
    }

    const xml = await parseXmlFile(xmlFile);
    const compound = xml.doxygen.compounddef[0];
    const compoundName = extractText(compound.compoundname);
    const briefDesc = extractText(compound.briefdescription);
    const detailedDesc = extractText(compound.detaileddescription);
    const location = compound.location ? compound.location[0].$.file : '';

    // Extract methods
    const publicMethods = [];
    const protectedMethods = [];
    const privateMethods = [];

    if (compound.sectiondef) {
      for (const section of compound.sectiondef) {
        const kind = section.$.kind;
        if (!section.memberdef) continue;

        for (const member of section.memberdef) {
          if (member.$.kind !== 'function') continue;

          const prot = member.$.prot;
          const method = {
            name: extractText(member.name),
            signature: formatMethod(member),
            briefDesc: extractText(member.briefdescription),
            detailedDesc: extractText(member.detaileddescription),
          };

          if (prot === 'public') {
            publicMethods.push(method);
          } else if (prot === 'protected') {
            protectedMethods.push(method);
          } else {
            privateMethods.push(method);
          }
        }
      }
    }

    // Generate markdown
    let markdown = `---
id: ${className}
title: ${className}
sidebar_label: ${className}
---

# ${className}

${briefDesc ? briefDesc : 'No description available.'}

${detailedDesc ? '\n' + detailedDesc : ''}

---

**Namespace:** \`${compoundName.split('::')[0]}\`

**Header:** \`${location}\`

`;

    // Public Methods
    if (publicMethods.length > 0) {
      markdown += '## Public Methods\n\n';
      for (const method of publicMethods) {
        markdown += `### \`${method.signature}\`\n\n`;
        if (method.briefDesc) {
          markdown += `${method.briefDesc}\n\n`;
        }
        if (method.detailedDesc) {
          markdown += `${method.detailedDesc}\n\n`;
        }
        markdown += '---\n\n';
      }
    }

    // Protected Methods
    if (protectedMethods.length > 0) {
      markdown += '## Protected Methods\n\n';
      for (const method of protectedMethods) {
        markdown += `### \`${method.signature}\`\n\n`;
        if (method.briefDesc) {
          markdown += `${method.briefDesc}\n\n`;
        }
        if (method.detailedDesc) {
          markdown += `${method.detailedDesc}\n\n`;
        }
        markdown += '---\n\n';
      }
    }

    // Private Methods
    if (privateMethods.length > 0) {
      markdown += '## Private Methods\n\n';
      for (const method of privateMethods) {
        markdown += `### \`${method.signature}\`\n\n`;
        if (method.briefDesc) {
          markdown += `${method.briefDesc}\n\n`;
        }
        if (method.detailedDesc) {
          markdown += `${method.detailedDesc}\n\n`;
        }
        markdown += '---\n\n';
      }
    }

    // Write markdown file
    const sanitized = sanitizeClassName(className);
    const outputFile = path.join(config.outputDir, module, `${sanitized}.md`);
    fs.writeFileSync(outputFile, markdown);
    console.log(`Generated: ${module}/${sanitized}.md`);

    return outputFile;
  } catch (error) {
    console.error(`Error processing ${className}:`, error.message);
    return null;
  }
}

// Process namespace XML and generate markdown
async function processNamespace(namespaceName, module, namespaceInfo) {
  try {
    const xmlFile = path.join(config.xmlDir, `namespaceenjin2_1_1${namespaceName}.xml`);

    if (!fs.existsSync(xmlFile)) {
      console.log(`Warning: XML file not found for namespace ${namespaceName}`);
      return null;
    }

    const xml = await parseXmlFile(xmlFile);
    const compound = xml.doxygen.compounddef[0];
    const compoundName = extractText(compound.compoundname);
    const briefDesc = extractText(compound.briefdescription);
    const detailedDesc = extractText(compound.detaileddescription);
    const location = compound.location ? compound.location[0].$.file : '';

    // Extract functions
    const functions = [];

    if (compound.sectiondef) {
      for (const section of compound.sectiondef) {
        if (!section.memberdef) continue;

        for (const member of section.memberdef) {
          if (member.$.kind !== 'function') continue;

          functions.push({
            name: extractText(member.name),
            signature: formatMethod(member),
            briefDesc: extractText(member.briefdescription),
            detailedDesc: extractText(member.detaileddescription),
          });
        }
      }
    }

    // Generate markdown
    let markdown = `---
id: ${namespaceName}
title: ${namespaceName}
sidebar_label: ${namespaceName}
---

# ${namespaceName}

${briefDesc ? briefDesc : namespaceInfo.description}

${detailedDesc ? '\n' + detailedDesc : ''}

---

**Namespace:** \`${compoundName}\`

**Header:** \`${location}\`

`;

    // Functions
    if (functions.length > 0) {
      markdown += '## Functions\n\n';
      for (const func of functions) {
        markdown += `### \`${func.signature}\`\n\n`;
        if (func.briefDesc) {
          markdown += `${func.briefDesc}\n\n`;
        }
        if (func.detailedDesc) {
          markdown += `${func.detailedDesc}\n\n`;
        }
        markdown += '---\n\n';
      }
    } else {
      markdown += '## Functions\n\nNo functions documented.\n\n';
    }

    // Write markdown file
    const outputFile = path.join(config.outputDir, module, `${namespaceName}.md`);
    fs.writeFileSync(outputFile, markdown);
    console.log(`Generated: ${module}/${namespaceName}.md`);

    return outputFile;
  } catch (error) {
    console.error(`Error processing namespace ${namespaceName}:`, error.message);
    return null;
  }
}

// Main execution
async function main() {
  console.log('Generating API documentation from Doxygen XML...');
  console.log(`XML directory: ${config.xmlDir}`);
  console.log(`Output directory: ${config.outputDir}\n`);

  let generatedFiles = 0;

  // Process classes in each module
  for (const [moduleName, moduleInfo] of Object.entries(config.modules)) {
    console.log(`\nProcessing ${moduleName} module...`);

    for (const className of moduleInfo.classes) {
      const result = await processClass(className, moduleName, moduleInfo);
      if (result) generatedFiles++;
    }
  }

  // Process namespaces
  console.log('\nProcessing namespaces...');
  for (const [nsName, nsInfo] of Object.entries(config.namespaces)) {
    const result = await processNamespace(nsName, nsInfo.module, nsInfo);
    if (result) generatedFiles++;
  }

  console.log(`\nGeneration complete! Generated ${generatedFiles} API documentation files.`);
}

main().catch(console.error);
